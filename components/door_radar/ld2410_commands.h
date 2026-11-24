#pragma once
// Bouwt LD2410-commandoframes in een aangeleverde buffer en beoordeelt ACK's.
// Zie contracts/ld2410-uart-protocol.md. Geen heap, geen framework-headers.

#include <cstddef>
#include <cstdint>

#include "ld2410_types.h"

namespace ld2410 {

constexpr uint16_t CMD_ENABLE_CONFIGURATION = 0x00FF;
constexpr uint16_t CMD_END_CONFIGURATION = 0x00FE;
constexpr uint16_t CMD_ENABLE_ENGINEERING_MODE = 0x0062;

/// Bit dat de radar in het commandowoord van een ACK zet.
constexpr uint16_t ACK_COMMAND_FLAG = 0x0100;

/// Groot genoeg voor alle commando's die deze component verstuurt.
constexpr size_t MAX_COMMAND_FRAME = 32;

/// Bouw een commandoframe. Geeft de lengte terug, of 0 als de buffer te klein is.
size_t buildCommand(uint16_t command, const uint8_t *value, size_t valueLength, uint8_t *out,
                    size_t capacity);

size_t buildEnableConfiguration(uint8_t *out, size_t capacity);
size_t buildEnableEngineeringMode(uint8_t *out, size_t capacity);
size_t buildEndConfiguration(uint8_t *out, size_t capacity);

/// true als de ACK bij dit commando hoort (ongeacht de status).
bool isAckFor(const AckFrame &ack, uint16_t command);
/// true als de ACK bij dit commando hoort en succes meldt.
bool isAckSuccess(const AckFrame &ack, uint16_t command);

}  // namespace ld2410
