// T027: tests voor het achtergrondmodel (data-model.md Background).
#include <unity.h>

#include <cmath>

#include "background_model.h"
#include "presence_detector.h"

using presence::BackgroundModel;
using presence::GATE_COUNT;

namespace {

constexpr uint32_t STEP_MS = 100;
constexpr uint32_t TAU_MS = 30000;  // DetectorConfig.backgroundTimeConstantMs standaard

void fill(uint8_t (&energy)[GATE_COUNT], uint8_t value) {
  for (auto &e : energy) e = value;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_converges_to_constant_energy() {
  BackgroundModel model;
  uint8_t energy[GATE_COUNT];
  fill(energy, 60);
  uint32_t t = 0;
  // Na 1 tijdconstante ~63%, na 5 tijdconstanten > 99%.
  for (; t <= TAU_MS; t += STEP_MS) model.update(energy, t, TAU_MS);
  TEST_ASSERT_FLOAT_WITHIN(3.0f, 60.0f * 0.632f, model.level(3));
  for (; t <= 5 * TAU_MS; t += STEP_MS) model.update(energy, t, TAU_MS);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 60.0f, model.level(3));
  TEST_ASSERT_EQUAL_UINT32(t - STEP_MS, model.lastUpdateMs());
}

void test_hold_freezes_model() {
  BackgroundModel model;
  uint8_t energy[GATE_COUNT];
  fill(energy, 0);
  model.update(energy, 0, TAU_MS);
  fill(energy, 80);
  uint32_t t = 0;
  for (t = STEP_MS; t <= 60000; t += STEP_MS) model.hold(t);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, model.level(2));
  // Na het bevriezen geen sprong: de eerste update telt maar één stap.
  model.update(energy, t, TAU_MS);
  TEST_ASSERT_TRUE(model.level(2) < 1.0f);
}

void test_large_gap_is_limited() {
  BackgroundModel model;
  uint8_t energy[GATE_COUNT];
  fill(energy, 0);
  model.update(energy, 0, TAU_MS);
  fill(energy, 90);
  model.update(energy, 60000, TAU_MS);  // gat van 60 s telt als MAX_STEP_MS
  const float expected = 90.0f * BackgroundModel::MAX_STEP_MS / TAU_MS;
  TEST_ASSERT_FLOAT_WITHIN(0.01f, expected, model.level(0));
}

void test_active_requires_both_minimum_and_background_margin() {
  BackgroundModel model;
  uint8_t energy[GATE_COUNT];
  fill(energy, 50);
  uint32_t t = 0;
  for (; t <= 10 * TAU_MS; t += STEP_MS) model.update(energy, t, TAU_MS);  // achtergrond ~50
  const uint8_t minEnergy = 20;   // DetectorConfig.minGateEnergy
  const uint8_t margin = 15;      // DetectorConfig.backgroundMargin
  TEST_ASSERT_FALSE(model.isActive(3, 60, minEnergy, margin));  // 60 <= 50 + 15
  TEST_ASSERT_TRUE(model.isActive(3, 70, minEnergy, margin));
  BackgroundModel empty;
  TEST_ASSERT_FALSE(empty.isActive(3, 18, minEnergy, margin));  // onder de minimale energie
  TEST_ASSERT_TRUE(empty.isAboveBackground(3, 18, margin));
  TEST_ASSERT_TRUE(empty.isActive(3, 21, minEnergy, margin));
}

// Diagnose "Zone N drempel": max(minimale energie, achtergrond + marge/spreiding).
void test_detector_gate_threshold() {
  presence::DetectorConfig cfg;
  presence::PresenceDetector det(cfg);
  // Lege achtergrond: drempel = max(minGateEnergy 20, 0 + marge 15) = 20.
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, det.gateThreshold(3));
  // Na een minuut constante energie 50 in alle zones: ~50 + 15 = ~65.
  ld2410::RadarFrame f;
  f.type = ld2410::FrameType::Engineering;
  f.maxMovingGate = ld2410::MAX_GATE_INDEX;
  for (auto &e : f.movingGateEnergy) e = 50;
  for (uint32_t t = 0; t <= 120000; t += STEP_MS) {
    f.timestampMs = t;
    det.onFrame(f);
  }
  TEST_ASSERT_FLOAT_WITHIN(2.0f, 65.0f, det.gateThreshold(3));
  TEST_ASSERT_TRUE(std::isnan(det.gateThreshold(presence::GATE_COUNT)));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_detector_gate_threshold);
  RUN_TEST(test_converges_to_constant_energy);
  RUN_TEST(test_hold_freezes_model);
  RUN_TEST(test_large_gap_is_limited);
  RUN_TEST(test_active_requires_both_minimum_and_background_margin);
  return UNITY_END();
}
