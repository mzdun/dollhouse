// Copyright (c) 2026 Marcin Zdun
// This code is licensed under MIT license (see LICENSE for details)

#include <pyxel/gfx/interactions.hpp>
#include <utility>

namespace pyxel::gfx::interactions {
	state& state::transform(group_name const& group, group_transformation const& change) {
		groups[group].push_back(change);
		return *this;
	}

	state& state::transform(group_name const& group, std::span<group_transformation const> changes) {
		auto& list = groups[group];
		list.insert(list.end(), changes.begin(), changes.end());
		return *this;
	}

	state& state::set(slot_name const& slot, slot_value const& value) {
		slots[slot].push_back({.set = value});
		return *this;
	}

	transitions& transitions::for_all(transition rule) {
		defaults = std::move(rule);
		return *this;
	}

	transitions& transitions::for_group(group_name group, transition rule) {
		groups.insert_or_assign(std::move(group), std::move(rule));
		return *this;
	}

	transitions& transitions::for_slot(slot_name slot, transition rule) {
		slots.insert_or_assign(std::move(slot), std::move(rule));
		return *this;
	}
}  // namespace pyxel::gfx::interactions
