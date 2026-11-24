#pragma once
// Framework-vrije types voor de persoonsdetector.
// Zie specs/001-mmwave-person-detection/data-model.md (DetectorConfig, DetectionEvent,
// RejectReason, Toestandsmachine) en contracts/presence-detector-api.md (Evaluation).

#include <cstdint>

#include "ld2410_types.h"

namespace presence {

constexpr uint8_t GATE_COUNT = ld2410::GATE_COUNT;

/// Alle drempels van de detector met standaardwaarden (data-model.md DetectorConfig).
struct DetectorConfig {
  float minRangeM{0.75f};
  float maxRangeM{5.0f};
  float minSpeedMps{0.3f};
  float maxSpeedMps{2.5f};
  uint32_t minDurationMs{1000};
  float minNetDisplacementM{0.5f};
  uint32_t maxPauseMs{1000};
  uint32_t absentTimeoutMs{5000};
  uint32_t maxHoldMs{60000};
  uint8_t minGateEnergy[GATE_COUNT]{20, 20, 20, 20, 20, 20, 20, 20, 20};
  float reversalHysteresisM{0.3f};
  uint32_t evaluationWindowMs{1500};
  uint8_t backgroundMargin{15};
  uint32_t backgroundTimeConstantMs{30000};
  uint32_t startupMs{5000};
  uint32_t sensorTimeoutMs{3000};
};

/// Welk veld een validatiefout veroorzaakte.
enum class ConfigField : uint8_t {
  None,
  MinRange,
  MaxRange,
  MinSpeed,
  MaxSpeed,
  MinDuration,
  MinNetDisplacement,
  MaxPause,
  AbsentTimeout,
  MaxHold,
  MinGateEnergy,
  ReversalHysteresis,
  EvaluationWindow,
  BackgroundMargin,
  BackgroundTimeConstant,
  Startup,
  SensorTimeout,
};

enum class ConfigErrorCode : uint8_t {
  None,
  OutOfRange,                ///< Veld buiten zijn eigen grenzen.
  MinRangeNotBelowMaxRange,  ///< minRangeM >= maxRangeM.
  MinSpeedNotBelowMaxSpeed,  ///< minSpeedMps >= maxSpeedMps.
  HoldBelowAbsentTimeout,    ///< maxHoldMs < absentTimeoutMs.
  WindowBelowMinDuration,    ///< evaluationWindowMs < minDurationMs.
};

struct ConfigError {
  ConfigField field{ConfigField::None};
  ConfigErrorCode code{ConfigErrorCode::None};
  uint8_t gateIndex{0};  ///< Alleen zinvol bij ConfigField::MinGateEnergy.
};

/// Valideert alle velden en de onderlinge regels (data-model.md DetectorConfig, kolom Validatie).
bool validate(const DetectorConfig &config, ConfigError *error);

const char *toString(ConfigField field);
const char *toString(ConfigErrorCode code);

enum class Direction : uint8_t {
  Unknown,
  Approaching,
  Receding,
};

enum class EventKind : uint8_t {
  PersonDetected,
  PersonGone,
  SensorFault,
  SensorRecovered,
};

/// data-model.md DetectionEvent.
struct DetectionEvent {
  EventKind kind{EventKind::PersonGone};
  Direction direction{Direction::Unknown};  ///< Alleen bij PersonDetected.
  float distanceM{0.0f};
  uint32_t timestampMs{0};
};

/// data-model.md RejectReason.
enum class RejectReason : uint8_t {
  None,
  OutOfRange,
  SignalTooWeak,
  SpeedOutOfRange,
  TooShort,
  NetDisplacementTooSmall,
  DirectionReversal,
  Startup,
};

/// Waarde zoals in de log en de entiteit "Afwijzingsreden" (contracts/diagnostic-log.md).
const char *toString(RejectReason reason);
const char *toString(Direction direction);

/// Publieke status (FR-010).
enum class PresenceStatus : uint8_t {
  NoPerson,
  PersonPresent,
  SensorFault,
};

/// Diagnose van de laatste beoordeling (contracts/presence-detector-api.md).
struct Evaluation {
  bool accepted{false};
  RejectReason reason{RejectReason::Startup};
  float positionM{0.0f};  ///< NaN als er geen actieve zones zijn.
  float speedMps{0.0f};   ///< Negatief = naderend.
  float netDisplacementM{0.0f};
  uint32_t durationMs{0};
  uint16_t activeGateMask{0};
  uint8_t gateEnergy[GATE_COUNT]{};
};

}  // namespace presence
