// T019: scenario's voor User Story 1 (spec.md US1, FR-002..FR-009, FR-013, FR-014).
#include <unity.h>

#include "../support/synthetic.h"

using presence::DetectorConfig;
using presence::Direction;
using presence::EventKind;
using presence::PresenceStatus;
using presence::RejectReason;
using synth::emptyFrame;
using synth::RadarFrame;
using synth::Sim;

void setUp() {}
void tearDown() {}

// (a) FR-014: geen detectie tijdens de opstartperiode.
void test_no_detection_during_startup() {
  Sim sim;
  sim.walk(4.5f, 1.0f, 0.8f);  // ~4,4 s, binnen de opstartperiode van 5 s
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonDetected));
}

// (b) Naderende persoon: precies één PersonDetected (naderend) binnen 2 s (SC-002, FR-008).
void test_approaching_person_detected_once_within_2s() {
  Sim sim;
  sim.warmUp();
  const uint32_t enteredMs = sim.t;
  sim.walk(4.5f, 1.0f, 0.8f);
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::PersonDetected));
  const auto *e = sim.first(EventKind::PersonDetected);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL(static_cast<int>(Direction::Approaching), static_cast<int>(e->direction));
  TEST_ASSERT_TRUE(e->timestampMs - enteredMs <= 2000);
  TEST_ASSERT_EQUAL(static_cast<int>(PresenceStatus::PersonPresent), static_cast<int>(sim.det.status()));
}

// (c) Na verlaten volgt PersonGone na de wegtijd.
void test_person_gone_after_absent_timeout() {
  Sim sim;
  sim.warmUp();
  sim.walk(4.5f, 1.0f, 0.8f);
  const uint32_t lastMotionMs = sim.t - synth::FRAME_MS;
  sim.quiet(4500);
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonGone));
  sim.quiet(1000);
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::PersonGone));
  const auto *gone = sim.first(EventKind::PersonGone);
  TEST_ASSERT_UINT32_WITHIN(200, lastMotionMs + 5000, gone->timestampMs);
  TEST_ASSERT_EQUAL(static_cast<int>(PresenceStatus::NoPerson), static_cast<int>(sim.det.status()));
}

// (d) Stilstaand doel in dezelfde zone houdt vast, maar uiterlijk maxHoldMs (FR-009).
void test_stationary_target_holds_presence_until_max_hold() {
  Sim sim;
  sim.warmUp();
  sim.walk(4.5f, 2.0f, 0.8f);
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::PersonDetected));
  const uint32_t lastMotionMs = sim.t - synth::FRAME_MS;
  auto standing = [&](uint32_t ms) {
    const uint32_t end = sim.t + ms;
    while (sim.t < end) {
      RadarFrame f = emptyFrame(sim.t);
      synth::setStationaryTarget(f, 2.0f, 40);
      sim.send(f);
    }
  };
  standing(30000);
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonGone));
  TEST_ASSERT_EQUAL(static_cast<int>(PresenceStatus::PersonPresent), static_cast<int>(sim.det.status()));
  standing(31000);
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::PersonGone));
  TEST_ASSERT_UINT32_WITHIN(200, lastMotionMs + 60000, sim.first(EventKind::PersonGone)->timestampMs);
}

// (e) Stilstaand doel ver van de laatste positie houdt niet vast.
void test_stationary_target_far_away_does_not_hold() {
  Sim sim;
  sim.warmUp();
  sim.walk(4.5f, 2.0f, 0.8f);
  const uint32_t lastMotionMs = sim.t - synth::FRAME_MS;
  const uint32_t end = sim.t + 7000;
  while (sim.t < end) {
    RadarFrame f = emptyFrame(sim.t);
    synth::setStationaryTarget(f, 4.25f, 40);  // 3 zones verder
    sim.send(f);
  }
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::PersonGone));
  TEST_ASSERT_UINT32_WITHIN(200, lastMotionMs + 5000, sim.first(EventKind::PersonGone)->timestampMs);
}

// (f) Geen frames gedurende sensorTimeoutMs: SensorFault, geen PersonGone (FR-013).
void test_sensor_timeout_gives_fault_without_gone() {
  Sim sim;
  sim.warmUp();
  sim.walk(4.5f, 2.0f, 0.8f);
  const uint32_t lastFrameMs = sim.t - synth::FRAME_MS;
  sim.silence(3500);
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::SensorFault));
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonGone));
  TEST_ASSERT_EQUAL(static_cast<int>(PresenceStatus::SensorFault), static_cast<int>(sim.det.status()));
  TEST_ASSERT_UINT32_WITHIN(150, lastFrameMs + 3000, sim.first(EventKind::SensorFault)->timestampMs);
}

// (g) Eerste frame na een fout: SensorRecovered en een nieuwe opstartperiode.
void test_recovery_restarts_startup_period() {
  Sim sim;
  sim.warmUp();
  sim.silence(3500);
  sim.send(emptyFrame(sim.t));
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::SensorRecovered));
  TEST_ASSERT_EQUAL(static_cast<int>(PresenceStatus::NoPerson), static_cast<int>(sim.det.status()));
  sim.walk(4.5f, 1.5f, 0.8f);  // binnen de nieuwe opstartperiode
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonDetected));
  sim.quiet(2000);
  sim.walk(4.5f, 1.0f, 0.8f);
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::PersonDetected));
}

// Geen frames vanaf de start (sensor ontbreekt): ook een fout.
void test_missing_sensor_at_boot_gives_fault() {
  Sim sim;
  sim.silence(3500);
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::SensorFault));
}

// (h) FR-006: beweging buiten het detectiegebied.
void test_out_of_range_is_rejected() {
  Sim sim;
  sim.warmUp();
  sim.walk(6.0f, 5.5f, 0.3f);  // ~1,7 s op 5,5-6 m (maxRange 5 m)
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonDetected));
  TEST_ASSERT_EQUAL(static_cast<int>(RejectReason::OutOfRange), static_cast<int>(sim.det.lastEvaluation().reason));
}

// (i) FR-003: te snel (hardlopen) en te langzaam.
void test_running_is_rejected() {
  Sim sim;
  sim.warmUp();
  sim.walk(4.5f, 1.0f, 3.2f);
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonDetected));
}

void test_too_slow_is_rejected() {
  Sim sim;
  sim.warmUp();
  bool sawSpeedReason = false;
  sim.walk(4.5f, 3.3f, 0.2f, 70.0f, [&](RadarFrame &, float) {
    if (sim.det.lastEvaluation().reason == RejectReason::SpeedOutOfRange) sawSpeedReason = true;
  });
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonDetected));
  TEST_ASSERT_TRUE(sawSpeedReason);
}

// (j) Te korte beweging.
void test_too_short_is_rejected() {
  Sim sim;
  sim.warmUp();
  sim.walk(4.5f, 4.0f, 0.8f);  // 0,6 s
  sim.quiet(2000);
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonDetected));
  TEST_ASSERT_EQUAL(static_cast<int>(RejectReason::TooShort), static_cast<int>(sim.det.lastEvaluation().reason));
}

// (k) Te weinig netto verplaatsing (FR-004).
void test_small_net_displacement_is_rejected() {
  Sim sim;
  sim.warmUp();
  sim.walk(4.5f, 4.1f, 0.8f);
  // Daarna blijft de persoon beweging geven op dezelfde plek.
  bool sawReason = false;
  for (int i = 0; i < 20; i++) {
    RadarFrame f = emptyFrame(sim.t);
    synth::addMovingTarget(f, 4.1f, 70.0f);
    sim.send(f);
    if (sim.det.lastEvaluation().reason == RejectReason::NetDisplacementTooSmall) sawReason = true;
  }
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonDetected));
  TEST_ASSERT_TRUE(sawReason);
}

// (l) Event-wachtrij van 8 loopt over zonder fout; oudste events vervallen.
void test_event_queue_overflow() {
  presence::PresenceDetector det{DetectorConfig{}};
  uint32_t t = 1000;
  det.tick(t);
  for (int i = 0; i < 10; i++) {
    t += 3500;
    det.tick(t);  // SensorFault
    det.onFrame(emptyFrame(t));  // SensorRecovered
  }
  TEST_ASSERT_TRUE(det.droppedEventCount() > 0);
  presence::DetectionEvent e;
  int n = 0;
  while (det.pollEvent(e)) n++;
  TEST_ASSERT_EQUAL(presence::PresenceDetector::EVENT_QUEUE_SIZE, n);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_no_detection_during_startup);
  RUN_TEST(test_approaching_person_detected_once_within_2s);
  RUN_TEST(test_person_gone_after_absent_timeout);
  RUN_TEST(test_stationary_target_holds_presence_until_max_hold);
  RUN_TEST(test_stationary_target_far_away_does_not_hold);
  RUN_TEST(test_sensor_timeout_gives_fault_without_gone);
  RUN_TEST(test_recovery_restarts_startup_period);
  RUN_TEST(test_missing_sensor_at_boot_gives_fault);
  RUN_TEST(test_out_of_range_is_rejected);
  RUN_TEST(test_running_is_rejected);
  RUN_TEST(test_too_slow_is_rejected);
  RUN_TEST(test_too_short_is_rejected);
  RUN_TEST(test_small_net_displacement_is_rejected);
  RUN_TEST(test_event_queue_overflow);
  return UNITY_END();
}
