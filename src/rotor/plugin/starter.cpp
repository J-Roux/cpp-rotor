//
// Copyright (c) 2019-2020 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/starter.h"
#include "rotor/supervisor.h"

using namespace rotor;
using namespace rotor::internal;

void starter_plugin_t::activate(actor_base_t *actor_) noexcept {
    actor = actor_;
    subscribe(&starter_plugin_t::on_start);
    plugin_t::activate(actor);
}

void starter_plugin_t::on_start(message::start_trigger_t&) noexcept {
    actor->state = state_t::OPERATIONAL;
    auto& callback = actor->start_callback;
    if (callback) callback(*actor);
}
