#include "status/LedPattern.hpp"

namespace status {

namespace {

constexpr uint32_t kBlinkHalfPeriodMs = 500;

constexpr Color kOff{0, 0, 0};
constexpr Color kRed{255, 0, 0};
constexpr Color kYellow{255, 160, 0};
constexpr Color kBlue{0, 0, 255};

Color scaled(Color color, uint8_t brightness)
{
    auto scale = [brightness](uint8_t channel) { return static_cast<uint8_t>(channel * brightness / 255); };
    return {scale(color.r), scale(color.g), scale(color.b)};
}

} // namespace

State stateFrom(bool scaleConnected, bool machineConnected)
{
    if (!scaleConnected) {
        return State::SearchingScale;
    }
    return machineConnected ? State::Bridging : State::WaitingForMachine;
}

Color colorFor(State state, uint32_t nowMs, uint8_t brightness)
{
    switch (state) {
    case State::SearchingScale:
        return (nowMs / kBlinkHalfPeriodMs) % 2 == 0 ? scaled(kRed, brightness) : kOff;
    case State::WaitingForMachine:
        return scaled(kYellow, brightness);
    case State::Bridging:
        return scaled(kBlue, brightness);
    }
    return kOff;
}

} // namespace status
