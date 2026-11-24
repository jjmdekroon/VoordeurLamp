// T033: richting van de beweging (spec.md US3, data-model.md DetectionEvent.direction).
#include <unity.h>

#include <cmath>

#include "../support/synthetic.h"

using presence::Direction;
using presence::EventKind;
using synth::emptyFrame;
using synth::RadarFrame;
using synth::Sim;

void setUp() {}
void tearDown() {}

void test_receding_person() {
  Sim sim;
  sim.warmUp();
  sim.walk(1.0f, 4.5f, 0.8f);
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::PersonDetected));
  const auto *e = sim.first(EventKind::PersonDetected);
  TEST_ASSERT_EQUAL(static_cast<int>(Direction::Receding), static_cast<int>(e->direction));
}

void test_approaching_person_distance_matches_position() {
  Sim sim;
  sim.warmUp();
  float detectedTruePos = NAN;
  sim.walk(4.5f, 1.0f, 0.8f, 70.0f, [&](RadarFrame &, float pos) {
    if (std::isnan(detectedTruePos) && sim.count(EventKind::PersonDetected) > 0) detectedTruePos = pos;
  });
  const auto *e = sim.first(EventKind::PersonDetected);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL(static_cast<int>(Direction::Approaching), static_cast<int>(e->direction));
  // De lambda ziet de positie van het frame ná de detectie; corrigeer één framestap.
  const float truePosAtDetection = detectedTruePos + 0.08f;
  TEST_ASSERT_FLOAT_WITHIN(0.5f, truePosAtDetection, e->distanceM);
}

void test_approach_with_bend_pause_stays_approaching() {
  Sim sim;
  sim.warmUp();
  sim.walk(4.5f, 4.1f, 0.8f);
  // Bocht: 0,8 s beweging zonder dat de afstand verandert (binnen maxPauseMs).
  for (int i = 0; i < 8; i++) {
    RadarFrame f = emptyFrame(sim.t);
    synth::addMovingTarget(f, 4.1f, 70.0f);
    sim.send(f);
  }
  sim.walk(4.1f, 1.0f, 0.8f);
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::PersonDetected));
  TEST_ASSERT_EQUAL(static_cast<int>(Direction::Approaching),
                    static_cast<int>(sim.first(EventKind::PersonDetected)->direction));
}

void test_back_and_forth_gives_two_detections_with_both_directions() {
  Sim sim;
  sim.warmUp();
  sim.walk(4.5f, 1.0f, 0.8f);
  sim.quiet(6000);  // weg
  sim.walk(1.0f, 4.5f, 0.8f);
  TEST_ASSERT_EQUAL(2, sim.count(EventKind::PersonDetected));
  int approaching = 0, receding = 0;
  for (const auto &e : sim.events) {
    if (e.kind != EventKind::PersonDetected) continue;
    if (e.direction == Direction::Approaching) approaching++;
    if (e.direction == Direction::Receding) receding++;
  }
  TEST_ASSERT_EQUAL(1, approaching);
  TEST_ASSERT_EQUAL(1, receding);
}

// FR-008 (aangepast): heen en terug binnen één aanwezigheid geeft naderend + richtingsmelding.
void test_round_trip_reports_departure_within_presence() {
  Sim sim;
  sim.warmUp();
  sim.walk(4.5f, 1.0f, 1.1f);  // naar de deur (4 km/u)
  sim.walk(1.0f, 4.5f, 1.1f);  // omdraaien en weg
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::PersonDetected));
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::DirectionChanged));
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::PersonGone));
  const auto *change = sim.first(EventKind::DirectionChanged);
  TEST_ASSERT_EQUAL(static_cast<int>(Direction::Receding), static_cast<int>(change->direction));
  sim.quiet(6000);
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::PersonGone));
}

// Twee keer heen en weer: richtingsmeldingen wisselen af.
void test_repeated_turns_alternate_direction() {
  Sim sim;
  sim.warmUp();
  sim.walk(4.5f, 1.0f, 1.1f);
  sim.walk(1.0f, 4.0f, 1.1f);
  sim.walk(4.0f, 1.0f, 1.1f);
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::PersonDetected));
  TEST_ASSERT_EQUAL(2, sim.count(EventKind::DirectionChanged));
  std::vector<Direction> changes;
  for (const auto &e : sim.events)
    if (e.kind == EventKind::DirectionChanged) changes.push_back(e.direction);
  TEST_ASSERT_EQUAL(static_cast<int>(Direction::Receding), static_cast<int>(changes[0]));
  TEST_ASSERT_EQUAL(static_cast<int>(Direction::Approaching), static_cast<int>(changes[1]));
}

// Verder lopen in dezelfde richting of kleine bewegingen bij de deur: geen richtingsmelding.
void test_no_direction_change_without_sustained_opposite_movement() {
  Sim sim;
  sim.warmUp();
  sim.walk(4.5f, 1.0f, 1.1f);
  // Bij de deur heen en weer schuifelen (sleutels zoeken): +-0,2 m.
  for (int i = 0; i < 100; i++) {
    RadarFrame f = emptyFrame(sim.t);
    synth::addMovingTarget(f, 1.0f + 0.2f * std::sin(i * 0.4f), 70.0f);
    sim.send(f);
  }
  TEST_ASSERT_EQUAL(1, sim.count(EventKind::PersonDetected));
  TEST_ASSERT_EQUAL(0, sim.count(EventKind::DirectionChanged));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_round_trip_reports_departure_within_presence);
  RUN_TEST(test_repeated_turns_alternate_direction);
  RUN_TEST(test_no_direction_change_without_sustained_opposite_movement);
  RUN_TEST(test_receding_person);
  RUN_TEST(test_approaching_person_distance_matches_position);
  RUN_TEST(test_approach_with_bend_pause_stays_approaching);
  RUN_TEST(test_back_and_forth_gives_two_detections_with_both_directions);
  return UNITY_END();
}
