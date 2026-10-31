#include "acaia/DeviceInfo.hpp"

namespace acaia {

SystemId systemIdFromAddress(const BdAddress& address)
{
    return {address[5], address[4], address[3], 0xFE, 0xFF, address[2], address[1], address[0]};
}

} // namespace acaia
