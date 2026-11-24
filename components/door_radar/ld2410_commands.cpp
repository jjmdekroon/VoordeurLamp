#include "ld2410_commands.h"

namespace ld2410 {

namespace {

constexpr uint8_t COMMAND_HEADER[4] = {0xFD, 0xFC, 0xFB, 0xFA};
constexpr uint8_t COMMAND_FOOTER[4] = {0x04, 0x03, 0x02, 0x01};
constexpr size_t FRAME_OVERHEAD = 4 + 2 + 2 + 4;  // header + lengte + commandowoord + footer

}  // namespace

size_t buildCommand(uint16_t command, const uint8_t *value, size_t valueLength, uint8_t *out,
                    size_t capacity) {
  const size_t total = FRAME_OVERHEAD + valueLength;
  if (out == nullptr || total > capacity) return 0;
  if (valueLength > 0 && value == nullptr) return 0;

  size_t i = 0;
  for (uint8_t b : COMMAND_HEADER) out[i++] = b;
  const uint16_t dataLength = static_cast<uint16_t>(2 + valueLength);
  out[i++] = static_cast<uint8_t>(dataLength & 0xFF);
  out[i++] = static_cast<uint8_t>(dataLength >> 8);
  out[i++] = static_cast<uint8_t>(command & 0xFF);
  out[i++] = static_cast<uint8_t>(command >> 8);
  for (size_t v = 0; v < valueLength; v++) out[i++] = value[v];
  for (uint8_t b : COMMAND_FOOTER) out[i++] = b;
  return i;
}

size_t buildEnableConfiguration(uint8_t *out, size_t capacity) {
  static constexpr uint8_t VALUE[2] = {0x01, 0x00};
  return buildCommand(CMD_ENABLE_CONFIGURATION, VALUE, sizeof(VALUE), out, capacity);
}

size_t buildEnableEngineeringMode(uint8_t *out, size_t capacity) {
  return buildCommand(CMD_ENABLE_ENGINEERING_MODE, nullptr, 0, out, capacity);
}

size_t buildEndConfiguration(uint8_t *out, size_t capacity) {
  return buildCommand(CMD_END_CONFIGURATION, nullptr, 0, out, capacity);
}

bool isAckFor(const AckFrame &ack, uint16_t command) {
  return ack.command == static_cast<uint16_t>(command | ACK_COMMAND_FLAG);
}

bool isAckSuccess(const AckFrame &ack, uint16_t command) {
  return isAckFor(ack, command) && ack.status == 0;
}

}  // namespace ld2410
