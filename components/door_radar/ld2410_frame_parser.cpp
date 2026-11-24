#include "ld2410_frame_parser.h"

namespace ld2410 {

namespace {

constexpr uint8_t REPORT_HEADER[4] = {0xF4, 0xF3, 0xF2, 0xF1};
constexpr uint8_t REPORT_FOOTER[4] = {0xF8, 0xF7, 0xF6, 0xF5};
constexpr uint8_t ACK_HEADER[4] = {0xFD, 0xFC, 0xFB, 0xFA};
constexpr uint8_t ACK_FOOTER[4] = {0x04, 0x03, 0x02, 0x01};

constexpr uint8_t REPORT_HEAD = 0xAA;
constexpr uint8_t REPORT_TAIL = 0x55;
constexpr uint8_t REPORT_CHECK = 0x00;

// Indexen binnen de frame-inhoud van een rapport.
constexpr uint16_t IDX_TYPE = 0;
constexpr uint16_t IDX_HEAD = 1;
constexpr uint16_t IDX_TARGET = 2;
constexpr uint16_t BASIC_TARGET_LENGTH = 9;
constexpr uint16_t IDX_MAX_MOVING_GATE = IDX_TARGET + BASIC_TARGET_LENGTH;  // 11
constexpr uint16_t IDX_MAX_STATIONARY_GATE = IDX_MAX_MOVING_GATE + 1;       // 12
constexpr uint16_t IDX_GATE_ENERGY = IDX_MAX_STATIONARY_GATE + 1;           // 13
constexpr uint16_t TAIL_LENGTH = 2;
constexpr uint16_t MIN_BASIC_LENGTH = IDX_TARGET + BASIC_TARGET_LENGTH + TAIL_LENGTH;  // 13

// ACK: commandowoord (2) + status (2) + data.
constexpr uint16_t ACK_STATUS_END = 4;

uint16_t readU16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

}  // namespace

void FrameParser::reset() {
  state_ = State::Header;
  kind_ = Kind::None;
  pos_ = 0;
  length_ = 0;
  received_ = 0;
}

void FrameParser::markInvalid() {
  invalidFrames_++;
  reset();
}

void FrameParser::restartWith(uint8_t byte) {
  reset();
  if (byte == REPORT_HEADER[0]) {
    kind_ = Kind::Report;
    pos_ = 1;
  } else if (byte == ACK_HEADER[0]) {
    kind_ = Kind::Ack;
    pos_ = 1;
  }
}

FrameParser::Result FrameParser::feed(uint8_t byte, uint32_t nowMs) {
  switch (state_) {
    case State::Header: {
      if (pos_ == 0) {
        restartWith(byte);
        return Result::None;
      }
      const uint8_t *header = (kind_ == Kind::Report) ? REPORT_HEADER : ACK_HEADER;
      if (byte != header[pos_]) {
        // Ruis of een nieuw begin: opnieuw synchroniseren zonder als fout te tellen.
        restartWith(byte);
        return Result::None;
      }
      pos_++;
      if (pos_ == 4) {
        state_ = State::Length;
        pos_ = 0;
        length_ = 0;
      }
      return Result::None;
    }

    case State::Length:
      if (pos_ == 0) {
        length_ = byte;
        pos_ = 1;
        return Result::None;
      }
      length_ |= static_cast<uint16_t>(byte) << 8;
      if (length_ == 0 || length_ > MAX_PAYLOAD) {
        markInvalid();
        return Result::None;
      }
      state_ = State::Payload;
      received_ = 0;
      return Result::None;

    case State::Payload:
      payload_[received_++] = byte;
      if (received_ == length_) {
        state_ = State::Footer;
        pos_ = 0;
      }
      return Result::None;

    case State::Footer: {
      const uint8_t *footer = (kind_ == Kind::Report) ? REPORT_FOOTER : ACK_FOOTER;
      if (byte != footer[pos_]) {
        markInvalid();
        return Result::None;
      }
      pos_++;
      if (pos_ == 4) {
        return finishFrame(nowMs);
      }
      return Result::None;
    }
  }
  return Result::None;
}

FrameParser::Result FrameParser::finishFrame(uint32_t nowMs) {
  const Kind kind = kind_;
  Result result = Result::None;
  if (kind == Kind::Report) {
    RadarFrame decoded{};
    if (decodeReport(decoded)) {
      decoded.timestampMs = nowMs;
      frame_ = decoded;
      result = Result::Report;
    }
  } else {
    AckFrame decoded{};
    if (decodeAck(decoded)) {
      ack_ = decoded;
      result = Result::Ack;
    }
  }
  if (result == Result::None) {
    markInvalid();
  } else {
    reset();
  }
  return result;
}

bool FrameParser::decodeReport(RadarFrame &out) const {
  if (length_ < MIN_BASIC_LENGTH) return false;
  if (payload_[IDX_HEAD] != REPORT_HEAD) return false;
  if (payload_[length_ - 2] != REPORT_TAIL || payload_[length_ - 1] != REPORT_CHECK) return false;

  const uint8_t typeByte = payload_[IDX_TYPE];
  if (typeByte != static_cast<uint8_t>(FrameType::Engineering) &&
      typeByte != static_cast<uint8_t>(FrameType::Basic)) {
    return false;
  }
  out.type = static_cast<FrameType>(typeByte);

  const uint8_t *t = &payload_[IDX_TARGET];
  if (t[0] > static_cast<uint8_t>(TargetState::Both)) return false;
  out.targetState = static_cast<TargetState>(t[0]);
  out.movingDistanceCm = readU16(&t[1]);
  out.movingEnergy = t[3];
  out.stationaryDistanceCm = readU16(&t[4]);
  out.stationaryEnergy = t[6];
  out.detectionDistanceCm = readU16(&t[7]);
  if (out.movingEnergy > MAX_ENERGY || out.stationaryEnergy > MAX_ENERGY) return false;

  if (out.type == FrameType::Basic) return true;

  // Engineering: N-waarden en energie per zone.
  if (length_ < IDX_GATE_ENERGY + TAIL_LENGTH) return false;
  const uint8_t maxMoving = payload_[IDX_MAX_MOVING_GATE];
  const uint8_t maxStationary = payload_[IDX_MAX_STATIONARY_GATE];
  if (maxMoving > MAX_GATE_INDEX || maxStationary > MAX_GATE_INDEX) return false;
  const uint16_t needed = IDX_GATE_ENERGY + (maxMoving + 1) + (maxStationary + 1) + TAIL_LENGTH;
  if (length_ < needed) return false;

  out.maxMovingGate = maxMoving;
  out.maxStationaryGate = maxStationary;
  uint16_t idx = IDX_GATE_ENERGY;
  for (uint8_t g = 0; g <= maxMoving; g++) {
    const uint8_t e = payload_[idx++];
    if (e > MAX_ENERGY) return false;
    out.movingGateEnergy[g] = e;
  }
  for (uint8_t g = 0; g <= maxStationary; g++) {
    const uint8_t e = payload_[idx++];
    if (e > MAX_ENERGY) return false;
    out.stationaryGateEnergy[g] = e;
  }
  // Eventuele extra bytes (M) tot aan de tail worden genegeerd.
  return true;
}

bool FrameParser::decodeAck(AckFrame &out) const {
  if (length_ < ACK_STATUS_END) return false;
  out.command = readU16(&payload_[0]);
  out.status = readU16(&payload_[2]);
  uint16_t extra = length_ - ACK_STATUS_END;
  if (extra > MAX_ACK_DATA) extra = MAX_ACK_DATA;
  for (uint16_t i = 0; i < extra; i++) out.data[i] = payload_[ACK_STATUS_END + i];
  out.dataLength = static_cast<uint8_t>(extra);
  return true;
}

}  // namespace ld2410
