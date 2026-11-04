#include "ota/OtaSession.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace ota {

OtaSession::OtaSession(FlashTarget& flash, Notifier& notifier, Options options)
    : flash_(flash)
    , notifier_(notifier)
    , options_(std::move(options))
{
}

void OtaSession::onControl(std::span<const uint8_t> data, uint16_t chunk, uint32_t nowMs)
{
    const std::optional<Opcode> opcode = commandOpcode(data);
    if (!opcode) {
        const uint8_t raw = data.empty() ? 0 : data[0];
        notifier_.notify(encodeResponse(static_cast<Opcode>(raw | 0x80), Status::InvalidCommand));
        return;
    }
    switch (*opcode) {
    case Opcode::Begin:
        begin(data, chunk, nowMs);
        break;
    case Opcode::End:
        end();
        break;
    case Opcode::Abort:
        if (state_ == State::Receiving) {
            flash_.abort();
            reset();
        }
        notifier_.notify(encodeResponse(Opcode::AbortResponse, Status::Ok));
        break;
    default:
        break;
    }
}

void OtaSession::begin(std::span<const uint8_t> data, uint16_t chunk, uint32_t nowMs)
{
    auto respond = [&](Status status) {
        notifier_.notify(encodeBeginResponse(status, status == Status::Ok ? chunk : 0,
                                             status == Status::Ok ? options_.window : 0));
    };
    if (!enabled_) {
        respond(Status::NotInUpdateMode);
        return;
    }
    if (state_ == State::Done) {
        respond(Status::InvalidCommand); // restarting into the new image
        return;
    }
    if (state_ == State::Receiving) {
        flash_.abort(); // a reconnecting client starts over
        reset();
    }
    const uint32_t size = parseBegin(data)->imageSize;
    if (size < AppImage::kHeaderLength || size > flash_.capacity()) {
        respond(Status::InvalidSize);
        return;
    }
    const Status status = flash_.begin(size);
    if (status != Status::Ok) {
        respond(status);
        return;
    }
    state_ = State::Receiving;
    dataRejected_ = false;
    imageSize_ = size;
    lastActivityMs_ = nowMs;
    respond(Status::Ok);
}

void OtaSession::onData(std::span<const uint8_t> data, uint32_t nowMs)
{
    if (state_ != State::Receiving) {
        // Packets still in flight after a failure: the failure was reported once.
        if (!dataRejected_) {
            dataRejected_ = true;
            notifier_.notify(encodeProgress(Status::NotStarted, 0));
        }
        return;
    }
    const std::optional<DataPacket> packet = parseData(data);
    if (!packet) {
        fail(Status::InvalidCommand);
        return;
    }
    if (packet->seq != nextSeq_) {
        fail(Status::SequenceError);
        return;
    }
    if (packet->payload.size() > imageSize_ - received_) {
        fail(Status::TooMuchData);
        return;
    }
    nextSeq_ = static_cast<uint16_t>(nextSeq_ + 1);
    lastActivityMs_ = nowMs;

    const Status status = consume(packet->payload);
    if (status != Status::Ok) {
        fail(status);
        return;
    }
    ++packetsSinceAck_;
    const bool complete = received_ == imageSize_;
    if (complete || packetsSinceAck_ >= std::max<uint16_t>(options_.window / 2, 1)) {
        packetsSinceAck_ = 0;
        // Acknowledge what left the queue (the header may still be held back),
        // so the client's window always moves.
        notifier_.notify(encodeProgress(Status::Ok, received_));
    }
}

Status OtaSession::consume(std::span<const uint8_t> payload)
{
    // Hold back the start of the image until its header has been checked:
    // nothing that isn't firmware for this device reaches the flash.
    if (received_ < header_.size()) {
        const size_t take = std::min<size_t>(payload.size(), header_.size() - received_);
        std::memcpy(header_.data() + received_, payload.data(), take);
        received_ += take;
        payload = payload.subspan(take);
        if (received_ < header_.size()) {
            return Status::Ok;
        }
        if (const Status status = validateHeader(); status != Status::Ok) {
            return status;
        }
        if (const Status status = flash_.write(header_); status != Status::Ok) {
            return status;
        }
        written_ = received_;
    }
    if (payload.empty()) {
        return Status::Ok;
    }
    received_ += payload.size();
    if (const Status status = flash_.write(payload); status != Status::Ok) {
        return status;
    }
    written_ = received_;
    return Status::Ok;
}

Status OtaSession::validateHeader()
{
    const std::optional<AppImage> image = AppImage::parse(header_);
    if (!image || image->chipId != options_.chipId || image->projectName != options_.projectName) {
        return Status::InvalidImage;
    }
    imageVersion_ = image->version;
    return Status::Ok;
}

void OtaSession::end()
{
    if (state_ != State::Receiving) {
        notifier_.notify(encodeResponse(Opcode::EndResponse, Status::NotStarted));
        return;
    }
    if (received_ != imageSize_) {
        notifier_.notify(encodeResponse(Opcode::EndResponse, Status::Incomplete));
        return;
    }
    const Status status = flash_.finish();
    if (status != Status::Ok) {
        reset(); // finish() released the slot
        notifier_.notify(encodeResponse(Opcode::EndResponse, status));
        return;
    }
    state_ = State::Done;
    notifier_.notify(encodeResponse(Opcode::EndResponse, Status::Ok));
}

void OtaSession::onOverflow()
{
    if (state_ == State::Receiving) {
        fail(Status::Overflow);
    }
}

void OtaSession::onDisconnect()
{
    if (state_ == State::Receiving) {
        flash_.abort();
        reset();
    }
}

void OtaSession::poll(uint32_t nowMs)
{
    if (state_ == State::Receiving && nowMs - lastActivityMs_ >= options_.timeoutMs) {
        fail(Status::Timeout);
    }
}

void OtaSession::fail(Status status)
{
    const uint32_t written = written_;
    flash_.abort();
    reset();
    dataRejected_ = true;
    notifier_.notify(encodeProgress(status, written));
}

void OtaSession::reset()
{
    state_ = State::Idle;
    imageSize_ = 0;
    received_ = 0;
    written_ = 0;
    nextSeq_ = 0;
    packetsSinceAck_ = 0;
    imageVersion_.clear();
}

} // namespace ota
