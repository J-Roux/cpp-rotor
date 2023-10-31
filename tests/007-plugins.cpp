//
// Copyright (c) 2019-2023 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor.hpp"
#include "actor_test.h"
#include "system_context_test.h"
#include "supervisor_test.h"
#include "access.h"

namespace r = rotor;
namespace rt = rotor::test;

constexpr std::uint32_t PID_1 = 1u << 1;
constexpr std::uint32_t PID_2 = 1u << 2;

struct sample_plugin1_t;
struct buggy_plugin_t;
struct sample_plugin2_t;

struct sample_actor_t : public rt::actor_test_t {
    using rt::actor_test_t::actor_test_t;
    using plugins_list_t = std::tuple<sample_plugin1_t, sample_plugin2_t>;

    std::uint32_t init_seq = 0;
    std::uint32_t deinit_seq = 0;
};

struct sample_actor2_t : public sample_actor_t {
    using sample_actor_t::sample_actor_t;
    using plugins_list_t = std::tuple<sample_plugin1_t, sample_plugin2_t, buggy_plugin_t>;
};

struct sample_plugin1_t : public r::plugin::plugin_base_t {
    static std::type_index class_id;

    const std::type_index &identity() const noexcept override { return class_id; }

    void activate(r::actor_base_t *actor_) noexcept override {
        auto &init_seq = static_cast<sample_actor_t *>(actor_)->init_seq;
        init_seq = (init_seq << 8 | PID_1);
        return r::plugin::plugin_base_t::activate(actor_);
    }
    void deactivate() noexcept override {
        auto &deinit_seq = static_cast<sample_actor_t *>(actor)->deinit_seq;
        deinit_seq = (deinit_seq << 8 | PID_1);
        return r::plugin::plugin_base_t::deactivate();
    }
};

std::type_index sample_plugin1_t::class_id = typeid(sample_plugin1_t);

struct sample_plugin2_t : public r::plugin::plugin_base_t {
    static std::type_index class_id;

    const std::type_index &identity() const noexcept override { return class_id; }

    void activate(r::actor_base_t *actor_) noexcept override {
        auto &init_seq = static_cast<sample_actor_t *>(actor_)->init_seq;
        init_seq = (init_seq << 8 | PID_2);
        return r::plugin::plugin_base_t::activate(actor_);
    }

    void deactivate() noexcept override {
        auto &deinit_seq = static_cast<sample_actor_t *>(actor)->deinit_seq;
        deinit_seq = (deinit_seq << 8 | PID_2);
        return r::plugin::plugin_base_t::deactivate();
    }
};

std::type_index sample_plugin2_t::class_id = typeid(sample_plugin2_t);

struct buggy_plugin_t : public r::plugin::plugin_base_t {
    static std::type_index class_id;

    const std::type_index &identity() const noexcept override { return class_id; }

    void activate(r::actor_base_t *actor_) noexcept override {
        actor = actor_;
        actor_->commit_plugin_activation(*this, false);
    }
};

std::type_index buggy_plugin_t::class_id = typeid(buggy_plugin_t);

TEST_CASE("init/deinit sequence", "[plugin]") {
    rt::system_context_test_t system_context;
    using builder_t = typename sample_actor_t::template config_builder_t<sample_actor_t>;
    auto builder = builder_t([&](auto &) {}, system_context);
    auto actor = std::move(builder).timeout(rt::default_timeout).finish();

    std::type_index ptr = typeid(actor);
    CHECK(actor->access<rt::to::get_plugin>(&std::as_const(ptr)) == nullptr);

    REQUIRE(actor->get_activating_plugins().size() == 2);
    REQUIRE(actor->get_deactivating_plugins().size() == 0);

    actor->activate_plugins();
    REQUIRE((actor->init_seq == ((PID_1 << 8) | PID_2)));

    REQUIRE(actor->get_activating_plugins().size() == 0);
    REQUIRE(actor->get_deactivating_plugins().size() == 0);

    actor->deactivate_plugins();
    REQUIRE((actor->deinit_seq == ((PID_2 << 8) | PID_1)));
    REQUIRE(actor->get_deactivating_plugins().size() == 0);
}

TEST_CASE("fail init-plugin sequence", "[plugin]") {
    rt::system_context_test_t system_context;
    using builder_t = typename sample_actor2_t::template config_builder_t<sample_actor2_t>;
    auto builder = builder_t([&](auto &) {}, system_context);
    auto actor = std::move(builder).timeout(rt::default_timeout).finish();

    REQUIRE(actor->get_activating_plugins().size() == 3);
    REQUIRE(actor->get_deactivating_plugins().size() == 0);

    actor->activate_plugins();
    CHECK(actor->init_seq == ((PID_1 << 8) | PID_2));

    REQUIRE(actor->get_activating_plugins().size() == 1);
    REQUIRE(actor->get_deactivating_plugins().size() == 0);

    CHECK(actor->deinit_seq == ((PID_2 << 8) | PID_1));
}
