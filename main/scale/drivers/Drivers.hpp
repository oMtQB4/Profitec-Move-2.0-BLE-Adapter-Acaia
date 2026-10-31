#pragma once

#include "scale/DriverRegistry.hpp"

namespace scale {

/** Registers every built-in scale driver. Add new drivers in Drivers.cpp. */
void registerBuiltinDrivers(DriverRegistry& registry);

} // namespace scale
