//
// Copyright (c) 2019-2022 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor.hpp"
#include "supervisor_test.h"
#include "access.h"

namespace r = rotor;
namespace rt = r::test;

static size_t destroyed = 0;

struct my_supervisor_t : public rt::supervisor_test_t {
    using rt::supervisor_test_t::supervisor_test_t;

    // clang-format off
    using plugins_list_t = std::tuple<
        r::plugin::address_maker_plugin_t,
        r::plugin::locality_plugin_t,
        r::plugin::delivery_plugin_t<r::plugin::local_delivery_t>,  // for coverage
        r::plugin::lifetime_plugin_t,
        r::plugin::init_shutdown_plugin_t,
        r::plugin::foreigners_support_plugin_t,
        r::plugin::child_manager_plugin_t,
        r::plugin::link_server_plugin_t,
        r::plugin::link_client_plugin_t,
        r::plugin::registry_plugin_t,
        r::plugin::starter_plugin_t>;
    // clang-format on

    void init_start() noexcept override {
        rt::supervisor_test_t::init_start();
        assert(state == r::state_t::INITIALIZING);
        init_start_count++;
    }

    void init_finish() noexcept override {
        rt::supervisor_test_t::init_finish();
        assert(state == r::state_t::INITIALIZED);
        init_finish_count++;
    }

    void shutdown_start() noexcept override {
        rt::supervisor_test_t::shutdown_start();
        shutdown_start_count++;
        assert(state == r::state_t::SHUTTING_DOWN);
    }

    void shutdown_finish() noexcept override {
        rt::supervisor_test_t::shutdown_finish();
        shutdown_finish_count++;
        assert(state == r::state_t::SHUT_DOWN);
    }

    ~my_supervisor_t() { ++destroyed; }

    std::uint32_t init_start_count = 0;
    std::uint32_t init_finish_count = 0;
    std::uint32_t shutdown_start_count = 0;
    std::uint32_t shutdown_finish_count = 0;
};

TEST_CASE("two supervisors, different localities, shutdown 2nd", "[supervisor]") {
    r::system_context_t system_context;

    const char locality1[] = "abc";
    const char locality2[] = "def";
    auto sup1 =
        system_context.create_supervisor<my_supervisor_t>().locality(locality1).timeout(rt::default_timeout).finish();
    auto sup2 = sup1->create_actor<my_supervisor_t>().locality(locality2).timeout(rt::default_timeout).finish();

    REQUIRE(&sup2->get_supervisor() == sup2.get());
    REQUIRE(sup2->access<rt::to::parent_supervisor>() == sup1.get());

    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::INITIALIZING);
    REQUIRE(sup2->get_state() == r::state_t::INITIALIZING);
    REQUIRE(sup1->init_start_count == 1);
    REQUIRE(sup1->init_finish_count == 0);
    REQUIRE(sup2->init_start_count == 1);
    REQUIRE(sup2->init_finish_count == 0);

    sup2->do_process();
    REQUIRE(sup1->get_state() == r::state_t::INITIALIZING);
    REQUIRE(sup2->get_state() == r::state_t::INITIALIZED);
    REQUIRE(sup1->init_start_count == 1);
    REQUIRE(sup1->init_finish_count == 0);
    REQUIRE(sup2->init_start_count == 1);
    REQUIRE(sup2->init_finish_count == 1);

    sup1->do_process();
    REQUIRE(sup1->init_start_count == 1);
    REQUIRE(sup1->init_finish_count == 1);
    REQUIRE(sup2->init_start_count == 1);
    REQUIRE(sup2->init_finish_count == 1);
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::INITIALIZED);
    REQUIRE(sup1->shutdown_start_count == 0);

    sup2->do_process();
    REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup1->init_start_count == 1);
    REQUIRE(sup1->init_finish_count == 1);
    REQUIRE(sup2->init_start_count == 1);
    REQUIRE(sup2->init_finish_count == 1);
    REQUIRE(sup2->shutdown_start_count == 0);
    REQUIRE(sup1->shutdown_start_count == 0);

    sup2->do_shutdown();
    sup2->do_process();
    REQUIRE(sup1->shutdown_start_count == 0);
    REQUIRE(sup1->shutdown_finish_count == 0);
    REQUIRE(sup2->shutdown_start_count == 0);
    REQUIRE(sup2->shutdown_finish_count == 0);

    sup1->do_process();
    REQUIRE(sup1->shutdown_start_count == 0);
    REQUIRE(sup1->shutdown_finish_count == 0);
    REQUIRE(sup2->shutdown_start_count == 0);
    REQUIRE(sup2->shutdown_finish_count == 0);

    sup2->do_process();
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup1->shutdown_start_count == 0);
    REQUIRE(sup1->shutdown_finish_count == 0);
    REQUIRE(sup2->shutdown_start_count == 1);
    REQUIRE(sup2->shutdown_finish_count == 1);

    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup1->shutdown_start_count == 0);
    REQUIRE(sup1->shutdown_finish_count == 0);
    REQUIRE(sup2->shutdown_start_count == 1);
    REQUIRE(sup2->shutdown_finish_count == 1);

    sup1->do_shutdown();
    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup1->shutdown_start_count == 1);
    REQUIRE(sup1->shutdown_finish_count == 1);

    REQUIRE(sup1->get_leader_queue().size() == 0);
    REQUIRE(sup1->get_points().size() == 0);
    REQUIRE(rt::empty(sup1->get_subscription()));

    REQUIRE(sup2->get_leader_queue().size() == 0);
    REQUIRE(sup2->get_points().size() == 0);
    REQUIRE(rt::empty(sup2->get_subscription()));
}

TEST_CASE("two supervisors, different localities, shutdown 1st", "[supervisor]") {
    r::system_context_t system_context;

    const char locality1[] = "abc";
    const char locality2[] = "def";
    auto sup1 =
        system_context.create_supervisor<my_supervisor_t>().locality(locality1).timeout(rt::default_timeout).finish();
    auto sup2 = sup1->create_actor<my_supervisor_t>().locality(locality2).timeout(rt::default_timeout).finish();

    REQUIRE(&sup2->get_supervisor() == sup2.get());
    REQUIRE(sup2->access<rt::to::parent_supervisor>() == sup1.get());

    sup1->do_process();
    sup2->do_process();

    sup1->do_process();
    sup2->do_process();
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);

    sup1->do_shutdown();
    while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
        sup1->do_process();
        sup2->do_process();
    }
    CHECK(sup1->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup2->get_state() == r::state_t::SHUT_DOWN);

    REQUIRE(sup1->get_leader_queue().size() == 0);
    REQUIRE(sup1->get_points().size() == 0);
    REQUIRE(rt::empty(sup1->get_subscription()));

    REQUIRE(sup2->get_leader_queue().size() == 0);
    REQUIRE(sup2->get_points().size() == 0);
    REQUIRE(rt::empty(sup2->get_subscription()));
}

TEST_CASE("two supervisors & external subscription", "[supervisor]") {
    rt::system_test_context_t ctx1;
    rt::system_test_context_t ctx2;

    auto sup1 = ctx1.create_supervisor<rt::supervisor_test_t>().timeout(rt::default_timeout).finish();
    auto sup2 = ctx2.create_supervisor<rt::supervisor_test_t>()
                    .configurer([&](auto &, r::plugin::plugin_base_t &plugin) {
                        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &p) {
                            using message_t = rt::message::sample_t;
                            auto lambda = r::lambda<message_t>([](message_t &) noexcept { ; });
                            p.subscribe_actor(lambda, sup1->get_address());
                        });
                    })
                    .timeout(rt::default_timeout)
                    .finish();

    auto process = [&]() {
        while (!sup1->get_leader_queue().empty() || !sup2->get_leader_queue().empty()) {
            bool progress = false;
            if (sup1->get_state() != r::state_t::SHUT_DOWN && !sup1->get_leader_queue().empty()) {
                auto msg = &sup1->get_leader_queue().front();
                sup1->do_process();
                progress = sup1->get_leader_queue().empty() || (msg != &sup1->get_leader_queue().front());
            }
            if (sup2->get_state() != r::state_t::SHUT_DOWN && !sup2->get_leader_queue().empty()) {
                auto msg = &sup2->get_leader_queue().front();
                sup2->do_process();
                progress = sup1->get_leader_queue().empty() || (msg != &sup1->get_leader_queue().front());
            }
            if (!progress) {
                break;
            }
        }
    };

    SECTION("server-client shutdown order") {
        process();
        REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
        REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);

        sup1->do_shutdown();
        while (sup1->get_state() != r::state_t::SHUT_DOWN) {
            sup1->do_process();
        }
        CHECK(sup1->get_state() == r::state_t::SHUT_DOWN);
        CHECK(sup2->get_state() == r::state_t::OPERATIONAL);

        sup2->do_shutdown();
        process();
    }

    SECTION("client-server shutdown order, wiht message in progress") {
        process();
        REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
        REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);

        sup2->do_shutdown();
        while (sup2->get_state() != r::state_t::SHUT_DOWN) {
            sup2->do_process();
        }
        CHECK(sup1->get_state() == r::state_t::OPERATIONAL);
        CHECK(sup2->get_state() == r::state_t::SHUT_DOWN);

        auto msg = sup1->get_leader_queue().front();
        sup1->get_leader_queue().pop_front();
        process();

        sup1->send<rt::payload::sample_t>(sup1->get_address(), 5);
        process();

        sup1->get_leader_queue().push_back(std::move(msg));
        sup1->do_shutdown();
        process();
    }

    CHECK(sup1->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup2->get_state() == r::state_t::SHUT_DOWN);
}

TEST_CASE("two supervisors, same locality", "[supervisor]") {
    r::system_context_ptr_t system_context = new r::system_context_t();

    auto mark = destroyed;
    const char locality[] = "locality";
    auto sup1 =
        system_context->create_supervisor<my_supervisor_t>().locality(locality).timeout(rt::default_timeout).finish();
    auto sup2 = sup1->create_actor<my_supervisor_t>().locality(locality).timeout(rt::default_timeout).finish();

    REQUIRE(&sup2->get_supervisor() == sup2.get());
    REQUIRE(sup2->access<rt::to::parent_supervisor>() == sup1.get());

    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup1->init_start_count == 1);
    REQUIRE(sup1->init_finish_count == 1);
    REQUIRE(sup2->init_start_count == 1);
    REQUIRE(sup2->init_finish_count == 1);

    sup1->do_shutdown();
    sup1->do_process();

    REQUIRE(sup1->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup2->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup1->shutdown_start_count == 1);

    REQUIRE(sup1->get_leader_queue().size() == 0);
    REQUIRE(sup1->get_points().size() == 0);
    REQUIRE(rt::empty(sup1->get_subscription()));

    REQUIRE(sup2->get_leader_queue().size() == 0);
    REQUIRE(sup2->get_points().size() == 0);
    REQUIRE(rt::empty(sup2->get_subscription()));

    system_context.reset();
    sup1.reset();
    sup2.reset();
    REQUIRE(mark + 2 == destroyed);
}

TEST_CASE("two supervisors, down internal first, same locality", "[supervisor]") {
    r::system_context_t system_context;

    const char locality[] = "locality";
    auto sup1 =
        system_context.create_supervisor<my_supervisor_t>().timeout(rt::default_timeout).locality(locality).finish();
    auto sup2 = sup1->create_actor<my_supervisor_t>().timeout(rt::default_timeout).locality(locality).finish();

    REQUIRE(&sup2->get_supervisor() == sup2.get());
    REQUIRE(sup2->access<rt::to::parent_supervisor>() == sup1.get());

    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);

    // for better coverage
    auto &address = static_cast<r::actor_base_t *>(sup2.get())->get_address();
    auto ec = r::make_error_code(r::error_code_t::success);
    auto reason = r::make_error("some-ctx", ec);
    sup2->send<r::payload::shutdown_trigger_t>(address, address, reason);
    sup1->do_process();

    REQUIRE(sup2->get_state() == r::state_t::SHUT_DOWN);
    CHECK(sup2->get_shutdown_reason()->root()->ec.message() == "success");
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);

    sup1->do_shutdown();
    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::SHUT_DOWN);

    REQUIRE(sup1->get_leader_queue().size() == 0);
    REQUIRE(sup1->get_points().size() == 0);
    REQUIRE(rt::empty(sup1->get_subscription()));

    REQUIRE(sup2->get_leader_queue().size() == 0);
    REQUIRE(sup2->get_points().size() == 0);
    REQUIRE(rt::empty(sup2->get_subscription()));
}

TEST_CASE("message arrival order", "[supervisor]") {
    r::system_context_t system_context;

    int model = 0;
    int states[] = {0, 0};
    auto sup1 = system_context.create_supervisor<rt::supervisor_test_t>()
                    .timeout(rt::default_timeout)
                    .configurer([&](auto &sup, r::plugin::plugin_base_t &plugin) {
                        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &p) {
                            using message_t = rt::message::sample_t;
                            auto lambda = r::lambda<message_t>([&](message_t &) noexcept {
                                printf("sup1\n");
                                if (!states[0]) {
                                    model += 1;
                                } else {
                                    model *= 2;
                                }
                                ++states[0];
                            });
                            p.subscribe_actor(lambda, sup.get_address());
                        });
                    })
                    .finish();

    auto sup2 = sup1->create_actor<rt::supervisor_test_t>()
                    .timeout(rt::default_timeout)
                    .configurer([&](auto &, r::plugin::plugin_base_t &plugin) {
                        plugin.with_casted<r::plugin::starter_plugin_t>([&](auto &p) {
                            using message_t = rt::message::sample_t;
                            auto lambda = r::lambda<message_t>([&](message_t &) noexcept {
                                printf("sup2\n");
                                if (!states[1]) {
                                    model += 2;
                                } else {
                                    model *= 3;
                                }
                                ++states[1];
                            });
                            printf("sup2-sb\n");
                            auto addr = sup1->get_address();
                            p.subscribe_actor(lambda, addr);
                            printf("sup2-sb\n");
                        });
                    })
                    .finish();

    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::OPERATIONAL);
    REQUIRE(sup2->get_state() == r::state_t::OPERATIONAL);

    sup1->send<rt::payload::sample_t>(sup1->get_address(), 0);
    sup1->send<rt::payload::sample_t>(sup1->get_address(), 0);
    sup1->do_process();
    CHECK(model == ((0 + 1 + 2) * 2 * 3));

    sup1->do_shutdown();
    sup1->do_process();
    REQUIRE(sup1->get_state() == r::state_t::SHUT_DOWN);
    REQUIRE(sup2->get_state() == r::state_t::SHUT_DOWN);
}
