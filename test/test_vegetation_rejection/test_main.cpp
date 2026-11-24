// T028: begroeiing, windstoten, regen en dwars lopen (spec.md US2, FR-005, FR-015).
#include <unity.h>

#include <cmath>

#include "../support/synthetic.h"

using presence::EventKind;
using presence::PresenceDetector;
using presence::RejectReason;
using synth::emptyFrame;
using synth::RadarFrame;
using synth::Sim;

namespace {

constexpr float PI = 3.14159265f;
constexpr float BUSH_POS_M = 2.25f;

/// Struik: positie slingert rond BUSH_POS_M, energie fluctueert met de wind.
void addBush(RadarFrame &f, uint32_t tMs, float amplitudeM, float swingHz, float meanEnergy, float energySwing) {
  const float s = tMs / 1000.0f;
  const float pos = BUSH_POS_M + amplitudeM * std::sin(2 * PI * swingHz * s);
  const float energy = meanEnergy + energySwing * std::sin(2 * PI * 0.23f * s + 1.0f);
  synth::addMovingTarget(f, pos, energy);
}

void runBush(Sim &sim, uint32_t ms, float amplitudeM, float swingHz, float meanEnergy, float energySwing) {
  const uint32_t end = sim.t + ms;
  while (sim.t < end) {
    RadarFrame f = emptyFrame(sim.t);
    addBush(f, sim.t, amplitudeM, swingHz, meanEnergy, energySwing);
    sim.send(f);
  }
}

}  // namespace

void setUp() {}
void tearDown() {}

// (a) Struik die 10 minuten heen en weer beweegt: geen detectie.
void test_swaying_bush_is_not_detected() {
  Sim sim;
  runBush(sim, 600000, 0.3f, 0.7f, 55.0f, 15.0f);
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonDetected));
}

// (b) Windstoot: snelle, grote heen-en-weer beweging; afwijzing op richtingsomkering.
void test_gust_is_rejected_on_reversal() {
  Sim sim;
  runBush(sim, 60000, 0.05f, 0.3f, 40.0f, 3.0f);  // eerst een minuut rustig
  bool sawReversal = false;
  const uint32_t end = sim.t + 20000;
  while (sim.t < end) {
    RadarFrame f = emptyFrame(sim.t);
    addBush(f, sim.t, 0.6f, 1.2f, 75.0f, 20.0f);
    sim.send(f);
    if (sim.det.lastEvaluation().reason == RejectReason::DirectionReversal) sawReversal = true;
  }
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonDetected));
  TEST_ASSERT_TRUE(sawReversal);
}

// (c) Struik + persoon, ook als de struik het sterkste signaal geeft: persoon wordt gedetecteerd.
void test_person_detected_next_to_stronger_bush() {
  Sim sim;
  runBush(sim, 180000, 0.3f, 0.7f, 85.0f, 10.0f);  // achtergrond leert de struik
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonDetected));
  sim.walk(4.5f, 1.0f, 0.8f, 60.0f, [](RadarFrame &f, float) { addBush(f, f.timestampMs, 0.3f, 0.7f, 85.0f, 10.0f); });
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::PersonDetected));
  TEST_ASSERT_EQUAL(static_cast<int>(presence::Direction::Approaching),
                    static_cast<int>(sim.first(EventKind::PersonDetected)->direction));
}

// (d) Regen: diffuse ruis in alle zones plus losse korte pieken.
void test_rain_is_not_detected() {
  Sim sim;
  synth::Lcg rng;
  const uint32_t end = sim.t + 600000;
  while (sim.t < end) {
    RadarFrame f = emptyFrame(sim.t);
    for (uint8_t g = 0; g < ld2410::GATE_COUNT; g++) {
      f.movingGateEnergy[g] = static_cast<uint8_t>(rng.next() % 31);  // 0..30
    }
    if (rng.uniform() < 0.05f) {
      const uint8_t g = static_cast<uint8_t>(1 + rng.next() % 6);
      f.movingGateEnergy[g] = static_cast<uint8_t>(40 + rng.next() % 31);
    }
    sim.send(f);
  }
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonDetected));
}

// (e) FR-015: zwak signaal (klein dier) wordt afgewezen met "signaal te zwak".
void test_weak_signal_is_rejected() {
  Sim sim;
  sim.warmUp();
  bool sawWeak = false;
  sim.walk(4.5f, 1.0f, 0.8f, 19.0f, [&](RadarFrame &, float) {
    if (sim.det.lastEvaluation().reason == RejectReason::SignalTooWeak) sawWeak = true;
  });
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonDetected));
  TEST_ASSERT_TRUE(sawWeak);
}

// (f) Achtergrond wordt niet bijgewerkt in Present: een aanwezige persoon "verdwijnt" niet.
void test_background_frozen_while_present() {
  Sim sim;
  sim.warmUp();
  sim.walk(4.5f, 2.0f, 0.8f);
  TEST_ASSERT_EQUAL(static_cast<int>(PresenceDetector::State::Present), static_cast<int>(sim.det.state()));
  const float before = sim.det.background().level(3);
  // Persoon blijft 20 s bewegen op ~2 m (bijv. bij de deur).
  const uint32_t end = sim.t + 20000;
  while (sim.t < end) {
    RadarFrame f = emptyFrame(sim.t);
    synth::addMovingTarget(f, 2.0f + 0.05f * std::sin(sim.t / 300.0f), 70.0f);
    sim.send(f);
  }
  TEST_ASSERT_FLOAT_WITHIN(0.001f, before, sim.det.background().level(3));
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonGone));
}

// (g) Persoon loopt dwars op ~4 m: afstand verandert < 0,3 m in 3 s.
void test_walking_across_is_rejected() {
  Sim sim;
  sim.warmUp();
  bool sawReason = false;
  sim.walk(4.0f, 3.8f, 0.067f, 70.0f, [&](RadarFrame &, float) {
    if (sim.det.lastEvaluation().reason == RejectReason::NetDisplacementTooSmall) sawReason = true;
  });
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonDetected));
  TEST_ASSERT_TRUE(sawReason);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_swaying_bush_is_not_detected);
  RUN_TEST(test_gust_is_rejected_on_reversal);
  RUN_TEST(test_person_detected_next_to_stronger_bush);
  RUN_TEST(test_rain_is_not_detected);
  RUN_TEST(test_weak_signal_is_rejected);
  RUN_TEST(test_background_frozen_while_present);
  RUN_TEST(test_walking_across_is_rejected);
  return UNITY_END();
}
