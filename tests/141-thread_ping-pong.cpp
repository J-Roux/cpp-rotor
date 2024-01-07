//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <catch2/catch_test_macros.hpp>
#include "rotor.hpp"
#include "rotor/thread.hpp"
#include "access.h"

namespace r = rotor;
namespace rth = rotor::thread;
namespace pt = boost::posix_time;
namespace rt = r::test;

static std::uint32_t destroyed = 0;
static const void *custom_tag = &custom_tag;

struct supervisor_thread_test_t : public rth::supervisor_thread_t {
    using rth::supervisor_thread_t::supervisor_thread_t;

    ~supervisor_thread_test_t() {
        destroyed += 4;
        printf("~supervisor_thread_test_t\n");
    }

    auto get_leader_queue() {
        return static_cast<supervisor_t *>(this)->access<rt::to::locality_leader>()->access<rt::to::queue>();
    }
    auto &get_subscription() noexcept { return subscription_map; }

    void intercept(r::message_ptr_t &message, const void *tag,
                   const r::continuation_t &continuation) noexcept override {
        if (tag == custom_tag) {
            intercepted = true;
        }
        rth::supervisor_thread_t::intercept(message, tag, continuation);
    }

    bool intercepted = false;
};

struct self_shutdowner_sup_t : public rth::supervisor_thread_t {
    using rth::supervisor_thread_t::supervisor_thread_t;

    void init_finish() noexcept override {
        rth::supervisor_thread_t::init_finish();
        printf("triggering shutdown\n");
        parent->shutdown();
    }

    void shutdown_finish() noexcept override {
        rth::supervisor_thread_t::shutdown_finish();
        printf("shutdown_finish\n");
    }
};

struct system_context_thread_test_t : public rth::system_context_thread_t {
    r::extended_error_ptr_t ee;
    void on_error(r::actor_base_t *, const r::extended_error_ptr_t &err) noexcept override { ee = err; }
};

struct ping_t {};
struct pong_t {};

struct pinger_t : public r::actor_base_t {
    std::uint32_t ping_sent = 0;
    std::uint32_t pong_received = 0;
    rotor::address_ptr_t ponger_addr;

    using r::actor_base_t::actor_base_t;
    ~pinger_t() { destroyed += 1; }

    void set_ponger_addr(const rotor::address_ptr_t &addr) { ponger_addr = addr; }

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        r::actor_base_t::configure(plugin);
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) { p.subscribe_actor(&pinger_t::on_pong); });
    }

    void on_start() noexcept override {
        r::actor_base_t::on_start();
        send<ping_t>(ponger_addr);
        ++ping_sent;
    }

    void on_pong(rotor::message_t<pong_t> &) noexcept {
        ++pong_received;
        supervisor->shutdown();
    }
};

struct ponger_t : public r::actor_base_t {
    std::uint32_t pong_sent = 0;
    std::uint32_t ping_received = 0;
    rotor::address_ptr_t pinger_addr;

    using r::actor_base_t::actor_base_t;
    ~ponger_t() { destroyed += 2; }

    void set_pinger_addr(const rotor::address_ptr_t &addr) { pinger_addr = addr; }

    void configure(r::plugin::plugin_base_t &plugin) noexcept override {
        plugin.with_casted<r::plugin::starter_plugin_t>([](auto &p) {
            r::subscription_info_ptr_t info = p.subscribe_actor(&ponger_t::on_ping);
            info->tag_io();
            info->access<rt::to::tag, const void *>(custom_tag);
        });
    }

    void on_ping(rotor::message_t<ping_t> &) noexcept {
        ++ping_received;
        send<pong_t>(pinger_addr);
        ++pong_sent;
    }
};

struct bad_actor_t : public r::actor_base_t {
    using r::actor_base_t::actor_base_t;

    void on_start() noexcept override { supervisor->shutdown(); }

    void shutdown_finish() noexcept override {}

    ~bad_actor_t() { printf("~bad_actor_t\n"); }
};

TEST_CASE("ping/pong", "[supervisor][thread]") {
    auto system_context = r::intrusive_ptr_t<rth::system_context_thread_t>(new rth::system_context_thread_t());
    auto timeout = r::pt::milliseconds{10};
    auto sup = system_context->create_supervisor<supervisor_thread_test_t>().timeout(timeout).finish();

    auto pinger = sup->create_actor<pinger_t>().timeout(timeout).finish();
    auto ponger = sup->create_actor<ponger_t>().timeout(timeout).finish();
    pinger->set_ponger_addr(static_cast<r::actor_base_t *>(ponger.get())->get_address());
    ponger->set_pinger_addr(static_cast<r::actor_base_t *>(pinger.get())->get_address());

    sup->start();
    system_context->run();

    REQUIRE(pinger->ping_sent == 1);
    REQUIRE(pinger->pong_received == 1);
    REQUIRE(ponger->pong_sent == 1);
    REQUIRE(ponger->ping_received == 1);

    pinger.reset();
    ponger.reset();

    CHECK(static_cast<r::actor_base_t *>(sup.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);
    CHECK(sup->get_leader_queue().size() == 0);
    CHECK(rt::empty(sup->get_subscription()));
    CHECK(sup->intercepted);

    sup.reset();
    system_context.reset();

    REQUIRE(destroyed == 1 + 2 + 4);
}

TEST_CASE("no shutdown confirmation", "[supervisor][thread]") {
    auto destroyed_start = destroyed;
    auto system_context = r::intrusive_ptr_t<system_context_thread_test_t>(new system_context_thread_test_t());
    auto timeout = r::pt::milliseconds{10};
    auto sup = system_context->create_supervisor<supervisor_thread_test_t>().timeout(timeout).finish();

    sup->start();
    sup->create_actor<bad_actor_t>().timeout(timeout).finish();
    system_context->run();

    REQUIRE(system_context->ee->ec == r::error_code_t::request_timeout);

    // act->force_cleanup();
    sup->shutdown();
    sup.reset();
    system_context.reset();
    auto destroyed_end = destroyed;
    CHECK(destroyed_end == destroyed_start + 4);
}

TEST_CASE("supervisors hierarchy", "[supervisor][thread]") {
    auto system_context = r::intrusive_ptr_t<system_context_thread_test_t>(new system_context_thread_test_t());
    auto timeout = r::pt::milliseconds{10};
    auto sup = system_context->create_supervisor<supervisor_thread_test_t>().timeout(timeout).finish();

    auto act = sup->create_actor<self_shutdowner_sup_t>().timeout(timeout).finish();
    sup->start();
    system_context->run();
    CHECK(!system_context->ee);

    CHECK(((r::actor_base_t *)act.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);
    CHECK(((r::actor_base_t *)sup.get())->access<rt::to::state>() == r::state_t::SHUT_DOWN);
}
