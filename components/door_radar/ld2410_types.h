#pragma once
// Framework-vrije datatypes voor het LD2410(B)-protocol.
// Zie specs/001-mmwave-person-detection/data-model.md (RadarFrame) en
// contracts/ld2410-uart-protocol.md. Geen ESPHome-, Arduino- of ESP-IDF-headers.

#include <cstddef>
#include <cstdint>

namespace ld2410 {

/// Aantal afstandszones (zone 0..8).
constexpr uint8_t GATE_COUNT = 9;
/// Hoogste zone-index.
constexpr uint8_t MAX_GATE_INDEX = GATE_COUNT - 1;
/// Afstand per zone bij de fabrieksresolutie.
constexpr float GATE_SIZE_M = 0.75f;
/// Maximale energiewaarde die de sensor rapporteert.
constexpr uint8_t MAX_ENERGY = 100;

enum class FrameType : uint8_t {
  Engineering = 0x01,
  Basic = 0x02,
};

enum class TargetState : uint8_t {
  None = 0x00,
  Moving = 0x01,
  Stationary = 0x02,
  Both = 0x03,
};

/// Eén geparseerd rapportframe ("Waarneming").
struct RadarFrame {
  FrameType type{FrameType::Basic};
  TargetState targetState{TargetState::None};
  uint16_t movingDistanceCm{0};
  uint8_t movingEnergy{0};
  uint16_t stationaryDistanceCm{0};
  uint8_t stationaryEnergy{0};
  uint16_t detectionDistanceCm{0};
  // Alleen gevuld bij engineering-frames; zones boven max*Gate zijn 0.
  uint8_t maxMovingGate{0};
  uint8_t maxStationaryGate{0};
  uint8_t movingGateEnergy[GATE_COUNT]{};
  uint8_t stationaryGateEnergy[GATE_COUNT]{};
  uint32_t timestampMs{0};

  bool hasMovingTarget() const {
    return targetState == TargetState::Moving || targetState == TargetState::Both;
  }
  bool hasStationaryTarget() const {
    return targetState == TargetState::Stationary || targetState == TargetState::Both;
  }
};

/// Maximaal aantal databytes van een ACK dat bewaard wordt.
constexpr size_t MAX_ACK_DATA = 32;

/// Antwoord van de radar op een commando.
struct AckFrame {
  uint16_t command{0};  ///< Commandowoord zoals ontvangen (verzonden commando | 0x0100).
  uint16_t status{0xFFFF};  ///< 0 = succes.
  uint8_t data[MAX_ACK_DATA]{};  ///< Retourwaarde na de status.
  uint8_t dataLength{0};
};

}  // namespace ld2410
