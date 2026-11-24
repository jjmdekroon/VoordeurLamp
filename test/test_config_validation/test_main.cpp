// T037: validatie van DetectorConfig (data-model.md DetectorConfig, kolom Validatie; FR-018).
#include <unity.h>

#include <cmath>

#include "detector_types.h"

using presence::ConfigError;
using presence::ConfigErrorCode;
using presence::ConfigField;
using presence::DetectorConfig;
using presence::validate;

namespace {

void expectValid(const DetectorConfig &c) {
  ConfigError err;
  TEST_ASSERT_TRUE_MESSAGE(validate(c, &err), presence::toString(err.field));
}

void expectInvalid(const DetectorConfig &c, ConfigField field, ConfigErrorCode code) {
  ConfigError err;
  TEST_ASSERT_FALSE(validate(c, &err));
  TEST_ASSERT_EQUAL_STRING(presence::toString(field), presence::toString(err.field));
  TEST_ASSERT_EQUAL_STRING(presence::toString(code), presence::toString(err.code));
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_defaults_are_valid() { expectValid(DetectorConfig{}); }

void test_range_bounds() {
  DetectorConfig c;
  c.maxRangeM = 6.0f;
  expectValid(c);
  c.maxRangeM = 6.01f;
  expectInvalid(c, ConfigField::MaxRange, ConfigErrorCode::OutOfRange);
  c = DetectorConfig{};
  c.minRangeM = 0.0f;
  expectValid(c);
  c.minRangeM = -0.1f;
  expectInvalid(c, ConfigField::MinRange, ConfigErrorCode::OutOfRange);
  c = DetectorConfig{};
  c.minRangeM = 5.0f;  // == maxRangeM
  expectInvalid(c, ConfigField::MinRange, ConfigErrorCode::MinRangeNotBelowMaxRange);
}

void test_speed_bounds() {
  DetectorConfig c;
  c.minSpeedMps = 0.0f;
  expectInvalid(c, ConfigField::MinSpeed, ConfigErrorCode::OutOfRange);
  c = DetectorConfig{};
  c.maxSpeedMps = 5.0f;
  expectValid(c);
  c.maxSpeedMps = 5.01f;
  expectInvalid(c, ConfigField::MaxSpeed, ConfigErrorCode::OutOfRange);
  c = DetectorConfig{};
  c.maxSpeedMps = 0.2f;  // < minSpeedMps 0.3 (quickstart V10)
  expectInvalid(c, ConfigField::MinSpeed, ConfigErrorCode::MinSpeedNotBelowMaxSpeed);
}

void test_duration_and_displacement_bounds() {
  DetectorConfig c;
  c.minDurationMs = 200;
  expectValid(c);
  c.minDurationMs = 199;
  expectInvalid(c, ConfigField::MinDuration, ConfigErrorCode::OutOfRange);
  c.minDurationMs = 5001;
  expectInvalid(c, ConfigField::MinDuration, ConfigErrorCode::OutOfRange);
  c = DetectorConfig{};
  c.minNetDisplacementM = 0.1f;
  expectValid(c);
  c.minNetDisplacementM = 0.09f;
  expectInvalid(c, ConfigField::MinNetDisplacement, ConfigErrorCode::OutOfRange);
  c.minNetDisplacementM = 3.01f;
  expectInvalid(c, ConfigField::MinNetDisplacement, ConfigErrorCode::OutOfRange);
}

void test_pause_timeout_and_hold_bounds() {
  DetectorConfig c;
  c.maxPauseMs = 3000;
  expectValid(c);
  c.maxPauseMs = 3001;
  expectInvalid(c, ConfigField::MaxPause, ConfigErrorCode::OutOfRange);
  c = DetectorConfig{};
  c.absentTimeoutMs = 999;
  expectInvalid(c, ConfigField::AbsentTimeout, ConfigErrorCode::OutOfRange);
  c.absentTimeoutMs = 60001;
  expectInvalid(c, ConfigField::AbsentTimeout, ConfigErrorCode::OutOfRange);
  c = DetectorConfig{};
  c.maxHoldMs = 600001;
  expectInvalid(c, ConfigField::MaxHold, ConfigErrorCode::OutOfRange);
  c.maxHoldMs = 4000;  // < absentTimeoutMs 5000
  expectInvalid(c, ConfigField::MaxHold, ConfigErrorCode::HoldBelowAbsentTimeout);
}

void test_gate_energy_bounds() {
  DetectorConfig c;
  c.minGateEnergy[4] = 100;
  expectValid(c);
  c.minGateEnergy[4] = 101;
  ConfigError err;
  TEST_ASSERT_FALSE(validate(c, &err));
  TEST_ASSERT_EQUAL_STRING("gate_min_energy", presence::toString(err.field));
  TEST_ASSERT_EQUAL_UINT8(4, err.gateIndex);
}

void test_yaml_only_fields() {
  DetectorConfig c;
  c.reversalHysteresisM = 0.0f;
  expectInvalid(c, ConfigField::ReversalHysteresis, ConfigErrorCode::OutOfRange);
  c = DetectorConfig{};
  c.backgroundMargin = 101;
  expectInvalid(c, ConfigField::BackgroundMargin, ConfigErrorCode::OutOfRange);
  c = DetectorConfig{};
  c.backgroundTimeConstantMs = 0;
  expectInvalid(c, ConfigField::BackgroundTimeConstant, ConfigErrorCode::OutOfRange);
  c = DetectorConfig{};
  c.sensorTimeoutMs = 0;
  expectInvalid(c, ConfigField::SensorTimeout, ConfigErrorCode::OutOfRange);
  c = DetectorConfig{};
  c.startupMs = 0;
  expectValid(c);
  c = DetectorConfig{};
  c.evaluationWindowMs = 900;  // < minDurationMs 1000
  expectInvalid(c, ConfigField::EvaluationWindow, ConfigErrorCode::WindowBelowMinDuration);
}

void test_nan_is_rejected() {
  DetectorConfig c;
  c.maxSpeedMps = NAN;
  TEST_ASSERT_FALSE(validate(c, nullptr));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_are_valid);
  RUN_TEST(test_range_bounds);
  RUN_TEST(test_speed_bounds);
  RUN_TEST(test_duration_and_displacement_bounds);
  RUN_TEST(test_pause_timeout_and_hold_bounds);
  RUN_TEST(test_gate_energy_bounds);
  RUN_TEST(test_yaml_only_fields);
  RUN_TEST(test_nan_is_rejected);
  return UNITY_END();
}
