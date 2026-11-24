// T038: applyConfig tijdens bedrijf (contracts/presence-detector-api.md, regel 6; FR-011, FR-018).
#include <unity.h>

#include "../support/synthetic.h"

using presence::ConfigError;
using presence::DetectorConfig;
using presence::EventKind;
using presence::PresenceDetector;
using synth::emptyFrame;
using synth::RadarFrame;
using synth::Sim;

void setUp() {}
void tearDown() {}

void test_invalid_candidate_keeps_old_config() {
  Sim sim;
  DetectorConfig candidate = sim.det.config();
  candidate.maxSpeedMps = 0.2f;
  ConfigError err;
  TEST_ASSERT_FALSE(sim.det.applyConfig(candidate, &err));
  TEST_ASSERT_EQUAL_STRING("min_speed_ge_max_speed", presence::toString(err.code));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, sim.det.config().maxSpeedMps);
}

void test_valid_candidate_is_applied_immediately() {
  Sim sim;
  sim.warmUp();
  DetectorConfig candidate = sim.det.config();
  candidate.maxRangeM = 4.0f;
  TEST_ASSERT_TRUE(sim.det.applyConfig(candidate));
  sim.walk(5.8f, 4.6f, 0.8f);  // alleen buiten 4 m
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonDetected));
}

void test_candidate_track_is_aborted() {
  Sim sim;
  sim.warmUp();
  sim.walk(4.5f, 4.2f, 0.8f);
  TEST_ASSERT_EQUAL(static_cast<int>(PresenceDetector::State::Candidate), static_cast<int>(sim.det.state()));
  DetectorConfig candidate = sim.det.config();
  candidate.absentTimeoutMs = 8000;
  TEST_ASSERT_TRUE(sim.det.applyConfig(candidate));
  TEST_ASSERT_EQUAL(static_cast<int>(PresenceDetector::State::Idle), static_cast<int>(sim.det.state()));
}

void test_present_stays_present_with_new_times() {
  Sim sim;
  sim.warmUp();
  sim.walk(4.5f, 1.0f, 0.8f);
  TEST_ASSERT_EQUAL(static_cast<int>(PresenceDetector::State::Present), static_cast<int>(sim.det.state()));
  const float bgBefore = sim.det.background().level(2);
  DetectorConfig candidate = sim.det.config();
  candidate.absentTimeoutMs = 10000;
  TEST_ASSERT_TRUE(sim.det.applyConfig(candidate));
  TEST_ASSERT_EQUAL(static_cast<int>(PresenceDetector::State::Present), static_cast<int>(sim.det.state()));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, bgBefore, sim.det.background().level(2));
  sim.quiet(7000);  // langer dan de oude wegtijd, korter dan de nieuwe
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonGone));
  sim.quiet(4000);
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::PersonGone));
}

void test_constructor_falls_back_to_defaults() {
  DetectorConfig invalid;
  invalid.minRangeM = 5.5f;  // >= maxRangeM
  PresenceDetector det(invalid);
  TEST_ASSERT_TRUE(det.usedDefaultConfig());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.75f, det.config().minRangeM);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_invalid_candidate_keeps_old_config);
  RUN_TEST(test_valid_candidate_is_applied_immediately);
  RUN_TEST(test_candidate_track_is_aborted);
  RUN_TEST(test_present_stays_present_with_new_times);
  RUN_TEST(test_constructor_falls_back_to_defaults);
  return UNITY_END();
}
