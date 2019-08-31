//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/supervisor.h"
#include "rotor/behavior.h"
#include "cassert"

using namespace rotor;

actor_behavior_t::~actor_behavior_t() {}

void actor_behavior_t::on_start_shutdown() noexcept {
    substate = behavior_state_t::SHUTDOWN_STARTED;
    actor.state = state_t::SHUTTING_DOWN;
    action_unsubscribe_self();
}

void actor_behavior_t::action_unsubscribe_self() noexcept {
    substate = behavior_state_t::UNSUBSCRIPTION_STARTED;
    actor_ptr_t self{&actor};
    actor.get_supervisor().unsubscribe_actor(self);
}

void actor_behavior_t::action_confirm_request() noexcept {
    if (actor.shutdown_request) {
        actor.reply_to(*actor.shutdown_request);
        actor.shutdown_request.reset();
    }
    return action_commit_shutdown();
}

void actor_behavior_t::action_commit_shutdown() noexcept {
    assert(actor.state == state_t::SHUTTING_DOWN);
    actor.state = state_t::SHUTTED_DOWN;
    return action_shutdonw_finish();
}

void actor_behavior_t::action_shutdonw_finish() noexcept {
    actor.shutdown_finish();
    substate = behavior_state_t::SHUTDOWN_ENDED;
}

void actor_behavior_t::on_unsubscription() noexcept {
    assert(substate == behavior_state_t::UNSUBSCRIPTION_STARTED);
    action_confirm_request();
}

supervisor_behavior_t::supervisor_behavior_t(supervisor_t &sup) : actor_behavior_t{sup} {}

void supervisor_behavior_t::on_start_shutdown() noexcept {
    substate = behavior_state_t::SHUTDOWN_STARTED;
    auto &sup = static_cast<supervisor_t &>(actor);
    sup.state = state_t::SHUTTING_DOWN;
    action_shutdown_children();
}

void supervisor_behavior_t::action_shutdown_children() noexcept {
    auto &sup = static_cast<supervisor_t &>(actor);
    auto &actors_map = sup.actors_map;
    if (!actors_map.empty()) {
        for (auto pair : actors_map) {
            auto &addr = pair.first;
            sup.request<payload::shutdown_request_t>(addr, addr).timeout(sup.shutdown_timeout);
        }
        substate = behavior_state_t::SHUTDOWN_CHILDREN_STARTED;
    } else {
        return action_unsubscribe_self();
    }
}

void supervisor_behavior_t::on_childen_removed() noexcept {
    assert(substate == behavior_state_t::SHUTDOWN_CHILDREN_STARTED);
    action_unsubscribe_self();
}

void supervisor_behavior_t::on_shutdown_fail(const address_ptr_t &, const std::error_code &ec) noexcept {
    auto &sup = static_cast<supervisor_t &>(actor);
    sup.context->on_error(ec);
}
