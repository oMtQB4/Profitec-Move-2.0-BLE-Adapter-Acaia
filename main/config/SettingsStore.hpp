#pragma once

#include <optional>

#include "common/PeerAddress.hpp"
#include "config/Settings.hpp"

namespace config {

/**
 * Settings and the learned machine address in NVS (namespace "config").
 * Settings are stored in the configurator's record format, so a firmware
 * update that adds a setting keeps the others and uses its default for the new one.
 */
class SettingsStore {
public:
    /** The stored settings applied over `defaults` (the Kconfig values). */
    static Settings load(const Settings& defaults);
    static bool save(const Settings& settings);

    static std::optional<PeerAddress> loadMachine();
    /** Stores the address, or erases it for nullopt. */
    static bool saveMachine(const std::optional<PeerAddress>& address);
};

} // namespace config
