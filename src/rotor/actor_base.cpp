//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/actor_base.h"
#include "rotor/supervisor.h"
//#include <iostream>

using namespace rotor;

actor_base_t::actor_base_t(supervisor_t &supervisor_)
    : supervisor{supervisor_}, state{state_t::NEW}, behaviour{nullptr} {}

actor_base_t::~actor_base_t() { delete behaviour; }

actor_behavior_t *actor_base_t::create_behaviour() noexcept { return new actor_behavior_t(*this); }

void actor_base_t::do_initialize(system_context_t *) noexcept {
    if (!address) {
        address = create_address();
    }
    if (!behaviour) {
        behaviour = create_behaviour();
    }

    supervisor.subscribe_actor(*this, &actor_base_t::on_unsubscription);
    supervisor.subscribe_actor(*this, &actor_base_t::on_external_unsubscription);
    supervisor.subscribe_actor(*this, &actor_base_t::on_initialize);
    supervisor.subscribe_actor(*this, &actor_base_t::on_start);
    supervisor.subscribe_actor(*this, &actor_base_t::on_shutdown);
    supervisor.subscribe_actor(*this, &actor_base_t::on_shutdown_trigger);
    supervisor.subscribe_actor(*this, &actor_base_t::on_subscription);
    state = state_t::INITIALIZING;
}

void actor_base_t::do_shutdown() noexcept { send<payload::shutdown_trigger_t>(supervisor.get_address(), address); }

address_ptr_t actor_base_t::create_address() noexcept { return supervisor.make_address(); }

void actor_base_t::on_initialize(message::init_request_t &msg) noexcept {
    init_request.reset(&msg);
    init_start();
}

void actor_base_t::on_start(message_t<payload::start_actor_t> &) noexcept { state = state_t::OPERATIONAL; }

void actor_base_t::on_shutdown(message::shutdown_request_t &msg) noexcept {
    shutdown_request.reset(&msg);
    shutdown_start();
}

void actor_base_t::on_shutdown_trigger(message::shutdown_trigger_t &) noexcept { do_shutdown(); }

void actor_base_t::init_start() noexcept { behaviour->on_start_init(); }

void actor_base_t::init_finish() noexcept {}

void actor_base_t::shutdown_start() noexcept { behaviour->on_start_shutdown(); }

void actor_base_t::shutdown_finish() noexcept {}

void actor_base_t::on_subscription(message_t<payload::subscription_confirmation_t> &msg) noexcept {
    points.push_back(subscription_point_t{msg.payload.handler, msg.payload.target_address});
}

void actor_base_t::on_unsubscription(message_t<payload::unsubscription_confirmation_t> &msg) noexcept {
    auto &addr = msg.payload.target_address;
    auto &handler = msg.payload.handler;
    remove_subscription(addr, handler);
    supervisor.commit_unsubscription(addr, handler);
    if (msg.payload.fn) {
        (*msg.payload.fn)();
    }
    if (points.empty() && state == state_t::SHUTTING_DOWN) {
        behaviour->on_unsubscription();
    }
}

void actor_base_t::on_external_unsubscription(message_t<payload::external_unsubscription_t> &msg) noexcept {
    auto &addr = msg.payload.target_address;
    auto &handler = msg.payload.handler;
    remove_subscription(addr, msg.payload.handler);
    auto &sup_addr = addr->supervisor.address;
    send<payload::commit_unsubscription_t>(sup_addr, addr, handler);
    if (points.empty() && state == state_t::SHUTTING_DOWN) {
        behaviour->on_unsubscription();
    }
}

void actor_base_t::remove_subscription(const address_ptr_t &addr, const handler_ptr_t &handler) noexcept {
    auto it = points.rbegin();
    while (it != points.rend()) {
        if (it->address == addr && *it->handler == *handler) {
            auto dit = it.base();
            points.erase(--dit);
            return;
        } else {
            ++it;
        }
    }
    assert(0 && "no subscription found");
}
