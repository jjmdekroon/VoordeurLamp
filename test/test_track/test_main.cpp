// T018: tests voor het bewegingsspoor (data-model.md Track).
#include <unity.h>

#include "track.h"

using presence::Direction;
using presence::Track;

namespace {
constexpr float HYSTERESIS = 0.3f;
constexpr uint32_t STEP_MS = 100;
}  // namespace

void setUp() {}
void tearDown() {}

void test_ring_buffer_wraps_after_64_samples() {
  Track track;
  track.start(0, 5.0f);
  for (uint32_t i = 1; i < 100; i++) track.add(i * STEP_MS, 5.0f - i * 0.01f, HYSTERESIS);
  TEST_ASSERT_EQUAL_UINT8(Track::CAPACITY, track.count());
  // Oudste aanwezige sample is nummer 36 (100 - 64).
  TEST_ASSERT_EQUAL_UINT32(36 * STEP_MS, track.sample(0).tMs);
  TEST_ASSERT_EQUAL_UINT32(99 * STEP_MS, track.sample(Track::CAPACITY - 1).tMs);
  // Startpositie blijft behouden voor de netto verplaatsing.
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, track.startPosM());
}

void test_constant_speed_regression() {
  Track track;
  track.start(0, 4.0f);
  for (uint32_t i = 1; i <= 20; i++) track.add(i * STEP_MS, 4.0f - 0.08f * i, HYSTERESIS);  // -0,8 m/s
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.8f, track.speedMps(1500));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.6f, track.netDisplacementM());
  TEST_ASSERT_EQUAL_UINT32(2000, track.durationMs());
  TEST_ASSERT_EQUAL(static_cast<int>(Direction::Approaching), static_cast<int>(track.direction()));
  TEST_ASSERT_FALSE(track.reversed());
}

void test_speed_uses_window_only() {
  Track track;
  track.start(0, 4.0f);
  // Eerst 2 s stilstand, daarna 1 s met -1 m/s; venster 1 s ziet alleen de beweging.
  for (uint32_t i = 1; i <= 20; i++) track.add(i * STEP_MS, 4.0f, HYSTERESIS);
  for (uint32_t i = 1; i <= 10; i++) track.add((20 + i) * STEP_MS, 4.0f - 0.1f * i, HYSTERESIS);
  TEST_ASSERT_FLOAT_WITHIN(0.02f, -1.0f, track.speedMps(1000));
}

void test_receding_direction() {
  Track track;
  track.start(0, 1.0f);
  for (uint32_t i = 1; i <= 10; i++) track.add(i * STEP_MS, 1.0f + 0.1f * i, HYSTERESIS);
  TEST_ASSERT_EQUAL(static_cast<int>(Direction::Receding), static_cast<int>(track.direction()));
  TEST_ASSERT_TRUE(track.speedMps(1500) > 0.9f);
}

void test_reversal_after_hysteresis() {
  Track track;
  track.start(0, 3.0f);
  track.add(100, 2.6f, HYSTERESIS);  // richting naderend vastgesteld
  track.add(200, 2.5f, HYSTERESIS);
  track.add(300, 2.7f, HYSTERESIS);  // 0,2 terug: nog geen omkering
  TEST_ASSERT_FALSE(track.reversed());
  track.add(400, 2.85f, HYSTERESIS);  // 0,35 terug vanaf 2,5
  TEST_ASSERT_TRUE(track.reversed());
}

void test_pause_within_max_pause_is_not_stale() {
  Track track;
  track.start(0, 4.0f);
  for (uint32_t i = 1; i <= 5; i++) track.add(i * STEP_MS, 4.0f - 0.08f * i, HYSTERESIS);
  // 0,9 s nagenoeg stil (bocht in het pad).
  for (uint32_t i = 6; i <= 14; i++) track.add(i * STEP_MS, 3.6f, HYSTERESIS);
  TEST_ASSERT_FALSE(track.isStale(14 * STEP_MS, 1000));
}

void test_pause_longer_than_max_pause_is_stale() {
  Track track;
  track.start(0, 4.0f);
  for (uint32_t i = 1; i <= 5; i++) track.add(i * STEP_MS, 4.0f - 0.08f * i, HYSTERESIS);
  for (uint32_t i = 6; i <= 20; i++) track.add(i * STEP_MS, 3.6f, HYSTERESIS);
  TEST_ASSERT_TRUE(track.isStale(20 * STEP_MS, 1000));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_ring_buffer_wraps_after_64_samples);
  RUN_TEST(test_constant_speed_regression);
  RUN_TEST(test_speed_uses_window_only);
  RUN_TEST(test_receding_direction);
  RUN_TEST(test_reversal_after_hysteresis);
  RUN_TEST(test_pause_within_max_pause_is_not_stale);
  RUN_TEST(test_pause_longer_than_max_pause_is_stale);
  return UNITY_END();
}
