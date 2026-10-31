#include "scale/drivers/Drivers.hpp"

#include "scale/drivers/BookooThemis.hpp"
#include "scale/drivers/Blackcoffee.hpp"
#include "scale/drivers/DecentScale.hpp"
#include "scale/drivers/DifluidMicrobalance.hpp"
#include "scale/drivers/DiyScales.hpp"
#include "scale/drivers/EurekaPrecisa.hpp"
#include "scale/drivers/Felicita.hpp"
#include "scale/drivers/Futula.hpp"
#include "scale/drivers/GoatStoryGina.hpp"
#include "scale/drivers/HiroiaJimmy.hpp"
#include "scale/drivers/Skale.hpp"
#include "scale/drivers/Smartchef.hpp"
#include "scale/drivers/Timemore.hpp"
#include "scale/drivers/VariaAku.hpp"
#include "scale/drivers/WeighMaster.hpp"
#include "scale/drivers/WeighMyBru.hpp"

namespace scale {

void registerBuiltinDrivers(DriverRegistry& registry)
{
    // First match wins: keep specific matchers before broader ones.
    registry.add<EurekaPrecisa>(); // tested on hardware
    registry.add<DecentScale>();
    registry.add<DifluidMicrobalanceTi>(); // before Microbalance: its names contain "microbalance"
    registry.add<DifluidMicrobalance>();
    registry.add<Felicita>();
    registry.add<HiroiaJimmy>();
    registry.add<Skale>();
    registry.add<VariaAku>();
    registry.add<BookooThemis>();
    registry.add<TimemoreBlackMirror>(); // before Basic/Dot: "timemore scale" is the most specific
    registry.add<TimemoreBasic>();
    registry.add<TimemoreDot>();
    registry.add<Smartchef>();
    registry.add<Blackcoffee>();
    registry.add<Futula>();
    registry.add<WeighMaster>();
    registry.add<WeighMyBru>();
    registry.add<GoatStoryGina>();
    registry.add<DiyPythonScale>();
    registry.add<DiyRustScale>();
}

} // namespace scale
