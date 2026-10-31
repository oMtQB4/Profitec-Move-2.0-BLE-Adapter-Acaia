#include "scale/drivers/Drivers.hpp"

#include "scale/drivers/EurekaPrecisa.hpp"

namespace scale {

void registerBuiltinDrivers(DriverRegistry& registry)
{
    registry.add<EurekaPrecisa>();
}

} // namespace scale
