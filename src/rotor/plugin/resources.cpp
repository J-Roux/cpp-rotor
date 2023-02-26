//
// Copyright (c) 2019-2023 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/resources.h"
#include "rotor/supervisor.h"
#include <numeric>

using namespace rotor;
using namespace rotor::plugin;

namespace {
namespace to {
struct state {};
struct resources {};
struct continuation_mask {};
} // namespace to
} // namespace

template <> auto &actor_base_t::access<to::state>() noexcept { return state; }
template <> auto &actor_base_t::access<to::resources>() noexcept { return resources; }
template <> auto &actor_base_t::access<to::continuation_mask>() noexcept { return continuation_mask; }

const std::type_index resources_plugin_t::class_identity = typeid(resources_plugin_t);

const std::type_index &resources_plugin_t::identity() const noexcept { return class_identity; }

void resources_plugin_t::activate(actor_base_t *actor_) noexcept {
    actor = actor_;

    reaction_on(reaction_t::INIT);
    reaction_on(reaction_t::SHUTDOWN);

    actor->access<to::resources>() = this;
    actor->configure(*this);

    return plugin_base_t::activate(actor_);
}

bool resources_plugin_t::handle_init(message::init_request_t *) noexcept {
    if (!configured) {
        configured = true;
        actor->configure(*this);
    }
    return !has_any();
}

bool resources_plugin_t::handle_shutdown(message::shutdown_request_t *req) noexcept {
    return !has_any() && plugin_base_t::handle_shutdown(req);
}

void resources_plugin_t::acquire(resource_id_t id) noexcept {
    if (id >= resources.size()) {
        resources.resize(id + 1);
    }
    ++resources[id];
    auto state = actor->access<to::state>();
    if (state == state_t::INITIALIZING) {
        reaction_on(reaction_t::INIT);
    } else if (state >= state_t::OPERATIONAL) {
        reaction_on(reaction_t::SHUTDOWN);
    }
}

bool resources_plugin_t::release(resource_id_t id) noexcept {
    assert(id < resources.size() && "out of bound resource release");
    auto &value = resources[id];
    assert(value > 0 && "release should be previously acquired before releasing");
    --value;
    auto state = actor->access<to::state>();
    bool has_acquired = has_any();
    if (state == state_t::INITIALIZING) {
        if (!has_acquired && !(actor->access<to::continuation_mask>() & actor_base_t::PROGRESS_INIT)) {
            actor->init_continue();
        }
    } else if (state == state_t::SHUTTING_DOWN) {
        if (!has_acquired && !(actor->access<to::continuation_mask>() & actor_base_t::PROGRESS_SHUTDOWN)) {
            actor->shutdown_continue();
        }
    }
    return has_acquired;
}

std::uint32_t resources_plugin_t::has(resource_id_t id) noexcept {
    if (id >= resources.size())
        return 0;
    return resources[id];
}

bool resources_plugin_t::has_any() noexcept {
    auto sum = std::accumulate(resources.begin(), resources.end(), 0);
    return sum > 0;
}
