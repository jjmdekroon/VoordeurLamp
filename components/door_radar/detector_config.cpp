// Validatie van DetectorConfig en tekstrepresentaties (data-model.md DetectorConfig, RejectReason).
#include <cmath>

#include "detector_types.h"

namespace presence {

namespace {

// Grenzen uit data-model.md DetectorConfig (kolom Validatie).
constexpr float MAX_SENSOR_RANGE_M = 6.0f;
constexpr float MAX_SPEED_LIMIT_MPS = 5.0f;
constexpr uint32_t MIN_DURATION_LOWER_MS = 200;
constexpr uint32_t MIN_DURATION_UPPER_MS = 5000;
constexpr float MIN_NET_DISPLACEMENT_LOWER_M = 0.1f;
constexpr float MIN_NET_DISPLACEMENT_UPPER_M = 3.0f;
constexpr uint32_t MAX_PAUSE_UPPER_MS = 3000;
constexpr uint32_t ABSENT_TIMEOUT_LOWER_MS = 1000;
constexpr uint32_t ABSENT_TIMEOUT_UPPER_MS = 60000;
constexpr uint32_t MAX_HOLD_UPPER_MS = 600000;
constexpr uint8_t MAX_ENERGY = ld2410::MAX_ENERGY;

bool fail(ConfigError *error, ConfigField field, ConfigErrorCode code, uint8_t gate = 0) {
  if (error != nullptr) {
    error->field = field;
    error->code = code;
    error->gateIndex = gate;
  }
  return false;
}

bool finiteIn(float value, float lower, float upper) {
  return std::isfinite(value) && value >= lower && value <= upper;
}

}  // namespace

bool validate(const DetectorConfig &c, ConfigError *error) {
  if (error != nullptr) *error = ConfigError{};

  // Velden afzonderlijk.
  if (!finiteIn(c.maxRangeM, 0.0f, MAX_SENSOR_RANGE_M) || c.maxRangeM <= 0.0f)
    return fail(error, ConfigField::MaxRange, ConfigErrorCode::OutOfRange);
  if (!finiteIn(c.minRangeM, 0.0f, MAX_SENSOR_RANGE_M))
    return fail(error, ConfigField::MinRange, ConfigErrorCode::OutOfRange);
  if (!std::isfinite(c.minSpeedMps) || c.minSpeedMps <= 0.0f)
    return fail(error, ConfigField::MinSpeed, ConfigErrorCode::OutOfRange);
  if (!std::isfinite(c.maxSpeedMps) || c.maxSpeedMps <= 0.0f || c.maxSpeedMps > MAX_SPEED_LIMIT_MPS)
    return fail(error, ConfigField::MaxSpeed, ConfigErrorCode::OutOfRange);
  if (c.minDurationMs < MIN_DURATION_LOWER_MS || c.minDurationMs > MIN_DURATION_UPPER_MS)
    return fail(error, ConfigField::MinDuration, ConfigErrorCode::OutOfRange);
  if (!finiteIn(c.minNetDisplacementM, MIN_NET_DISPLACEMENT_LOWER_M, MIN_NET_DISPLACEMENT_UPPER_M))
    return fail(error, ConfigField::MinNetDisplacement, ConfigErrorCode::OutOfRange);
  if (c.maxPauseMs > MAX_PAUSE_UPPER_MS)
    return fail(error, ConfigField::MaxPause, ConfigErrorCode::OutOfRange);
  if (c.absentTimeoutMs < ABSENT_TIMEOUT_LOWER_MS || c.absentTimeoutMs > ABSENT_TIMEOUT_UPPER_MS)
    return fail(error, ConfigField::AbsentTimeout, ConfigErrorCode::OutOfRange);
  if (c.maxHoldMs > MAX_HOLD_UPPER_MS)
    return fail(error, ConfigField::MaxHold, ConfigErrorCode::OutOfRange);
  for (uint8_t g = 0; g < GATE_COUNT; g++) {
    if (c.minGateEnergy[g] > MAX_ENERGY)
      return fail(error, ConfigField::MinGateEnergy, ConfigErrorCode::OutOfRange, g);
  }
  if (!std::isfinite(c.reversalHysteresisM) || c.reversalHysteresisM <= 0.0f)
    return fail(error, ConfigField::ReversalHysteresis, ConfigErrorCode::OutOfRange);
  if (c.backgroundMargin > MAX_ENERGY)
    return fail(error, ConfigField::BackgroundMargin, ConfigErrorCode::OutOfRange);
  if (c.backgroundTimeConstantMs == 0)
    return fail(error, ConfigField::BackgroundTimeConstant, ConfigErrorCode::OutOfRange);
  if (c.sensorTimeoutMs == 0)
    return fail(error, ConfigField::SensorTimeout, ConfigErrorCode::OutOfRange);

  // Onderlinge regels.
  if (c.minRangeM >= c.maxRangeM)
    return fail(error, ConfigField::MinRange, ConfigErrorCode::MinRangeNotBelowMaxRange);
  if (c.minSpeedMps >= c.maxSpeedMps)
    return fail(error, ConfigField::MinSpeed, ConfigErrorCode::MinSpeedNotBelowMaxSpeed);
  if (c.maxHoldMs < c.absentTimeoutMs)
    return fail(error, ConfigField::MaxHold, ConfigErrorCode::HoldBelowAbsentTimeout);
  if (c.evaluationWindowMs < c.minDurationMs)
    return fail(error, ConfigField::EvaluationWindow, ConfigErrorCode::WindowBelowMinDuration);

  return true;
}

const char *toString(ConfigField field) {
  switch (field) {
    case ConfigField::None: return "none";
    case ConfigField::MinRange: return "min_range";
    case ConfigField::MaxRange: return "max_range";
    case ConfigField::MinSpeed: return "min_speed";
    case ConfigField::MaxSpeed: return "max_speed";
    case ConfigField::MinDuration: return "min_duration";
    case ConfigField::MinNetDisplacement: return "min_displacement";
    case ConfigField::MaxPause: return "max_pause";
    case ConfigField::AbsentTimeout: return "absent_timeout";
    case ConfigField::MaxHold: return "max_hold";
    case ConfigField::MinGateEnergy: return "gate_min_energy";
    case ConfigField::ReversalHysteresis: return "reversal_hysteresis";
    case ConfigField::EvaluationWindow: return "evaluation_window";
    case ConfigField::BackgroundMargin: return "background_margin";
    case ConfigField::BackgroundTimeConstant: return "background_time_constant";
    case ConfigField::Startup: return "startup_time";
    case ConfigField::SensorTimeout: return "sensor_timeout";
  }
  return "unknown";
}

const char *toString(ConfigErrorCode code) {
  switch (code) {
    case ConfigErrorCode::None: return "none";
    case ConfigErrorCode::OutOfRange: return "out_of_range";
    case ConfigErrorCode::MinRangeNotBelowMaxRange: return "min_range_ge_max_range";
    case ConfigErrorCode::MinSpeedNotBelowMaxSpeed: return "min_speed_ge_max_speed";
    case ConfigErrorCode::HoldBelowAbsentTimeout: return "max_hold_lt_absent_timeout";
    case ConfigErrorCode::WindowBelowMinDuration: return "evaluation_window_lt_min_duration";
  }
  return "unknown";
}

const char *toString(RejectReason reason) {
  switch (reason) {
    case RejectReason::None: return "none";
    case RejectReason::OutOfRange: return "out_of_range";
    case RejectReason::SignalTooWeak: return "signal_too_weak";
    case RejectReason::SpeedOutOfRange: return "speed_out_of_range";
    case RejectReason::TooShort: return "too_short";
    case RejectReason::NetDisplacementTooSmall: return "net_displacement_too_small";
    case RejectReason::DirectionReversal: return "direction_reversal";
    case RejectReason::Startup: return "startup";
  }
  return "unknown";
}

const char *toString(Direction direction) {
  switch (direction) {
    case Direction::Approaching: return "naderend";
    case Direction::Receding: return "verwijderend";
    case Direction::Unknown: return "onbekend";
  }
  return "onbekend";
}

}  // namespace presence
