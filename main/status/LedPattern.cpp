#include "status/LedPattern.hpp"

namespace status {

namespace {

constexpr uint32_t kBlinkHalfPeriodMs = 500;
constexpr uint32_t kFastBlinkHalfPeriodMs = 100;

constexpr Color kOff{0, 0, 0};
constexpr Color kRed{255, 0, 0};
constexpr Color kYellow{255, 160, 0};
constexpr Color kBlue{0, 0, 255};
constexpr Color kGreen{0, 255, 0};
constexpr Color kPurple{160, 0, 255};

Color scaled(Color color, uint8_t brightness)
{
    auto scale = [brightness](uint8_t channel) { return static_cast<uint8_t>(channel * brightness / 255); };
    return {scale(color.r), scale(color.g), scale(color.b)};
}

bool shown(State state, LedMode mode)
{
    switch (state) {
    case State::UpdateMode:
    case State::Updating:
        return true;
    case State::Bridging:
    case State::OtherClient:
        return mode != LedMode::Off;
    case State::SearchingScale:
    case State::WaitingForMachine:
        return mode == LedMode::Standard;
    }
    return false;
}

} // namespace

State stateFrom(bool scaleConnected, bool clientConnected, bool clientIsMachine)
{
    if (!scaleConnected) {
        return State::SearchingScale;
    }
    if (!clientConnected) {
        return State::WaitingForMachine;
    }
    return clientIsMachine ? State::Bridging : State::OtherClient;
}

Color colorFor(State state, uint32_t nowMs, uint8_t brightness, LedMode mode)
{
    if (!shown(state, mode)) {
        return kOff;
    }
    switch (state) {
    case State::SearchingScale:
        return (nowMs / kBlinkHalfPeriodMs) % 2 == 0 ? scaled(kRed, brightness) : kOff;
    case State::WaitingForMachine:
        return scaled(kYellow, brightness);
    case State::Bridging:
        return scaled(kBlue, brightness);
    case State::OtherClient:
        return scaled(kGreen, brightness);
    case State::UpdateMode:
        return scaled(kPurple, brightness);
    case State::Updating:
        return (nowMs / kFastBlinkHalfPeriodMs) % 2 == 0 ? scaled(kPurple, brightness) : kOff;
    }
    return kOff;
}

} // namespace status
