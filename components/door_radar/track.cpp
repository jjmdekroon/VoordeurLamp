#include "track.h"

#include <cmath>

namespace presence {

void Track::reset() {
  head_ = 0;
  count_ = 0;
  total_ = 0;
  startMs_ = 0;
  startPosM_ = 0.0f;
  lastMotionMs_ = 0;
  lastPosM_ = 0.0f;
  direction_ = Direction::Unknown;
  extremePosM_ = 0.0f;
  reversed_ = false;
  lastProgressMs_ = 0;
  progressPosM_ = 0.0f;
}

void Track::start(uint32_t tMs, float posM) {
  reset();
  startMs_ = tMs;
  startPosM_ = posM;
  lastProgressMs_ = tMs;
  progressPosM_ = posM;
  extremePosM_ = posM;
  samples_[0] = {tMs, posM};
  head_ = 1;
  count_ = 1;
  total_ = 1;
  lastMotionMs_ = tMs;
  lastPosM_ = posM;
}

void Track::add(uint32_t tMs, float posM, float reversalHysteresisM) {
  if (count_ == 0) {
    start(tMs, posM);
    return;
  }
  samples_[head_] = {tMs, posM};
  head_ = static_cast<uint8_t>((head_ + 1) % CAPACITY);
  if (count_ < CAPACITY) count_++;
  total_++;
  lastMotionMs_ = tMs;
  lastPosM_ = posM;

  // Richting en omkering met hysterese (FR-005).
  switch (direction_) {
    case Direction::Unknown:
      if (posM - startPosM_ >= reversalHysteresisM) {
        direction_ = Direction::Receding;
        extremePosM_ = posM;
      } else if (startPosM_ - posM >= reversalHysteresisM) {
        direction_ = Direction::Approaching;
        extremePosM_ = posM;
      }
      break;
    case Direction::Approaching:
      if (posM < extremePosM_) extremePosM_ = posM;
      if (posM - extremePosM_ >= reversalHysteresisM) reversed_ = true;
      break;
    case Direction::Receding:
      if (posM > extremePosM_) extremePosM_ = posM;
      if (extremePosM_ - posM >= reversalHysteresisM) reversed_ = true;
      break;
  }

  // Voortgang voor pauzedetectie (FR-002).
  if (std::fabs(posM - progressPosM_) >= PROGRESS_STEP_M) {
    progressPosM_ = posM;
    lastProgressMs_ = tMs;
  }
}

const TrackSample &Track::sample(uint8_t i) const {
  const uint8_t oldest = static_cast<uint8_t>((head_ + CAPACITY - count_) % CAPACITY);
  return samples_[(oldest + i) % CAPACITY];
}

float Track::speedMps(uint32_t windowMs) const {
  if (count_ < 2) return 0.0f;
  const TrackSample &newest = sample(static_cast<uint8_t>(count_ - 1));
  // Samples binnen het venster, relatief aan het nieuwste sample (in seconden).
  float sumT = 0.0f, sumP = 0.0f;
  uint8_t n = 0;
  for (int i = count_ - 1; i >= 0; i--) {
    const TrackSample &s = sample(static_cast<uint8_t>(i));
    const uint32_t age = newest.tMs - s.tMs;
    if (age > windowMs) break;
    sumT += -static_cast<float>(age) / 1000.0f;
    sumP += s.posM;
    n++;
  }
  if (n < 2) return 0.0f;
  const float meanT = sumT / n;
  const float meanP = sumP / n;
  float sxy = 0.0f, sxx = 0.0f;
  for (int i = count_ - 1; i >= count_ - n; i--) {
    const TrackSample &s = sample(static_cast<uint8_t>(i));
    const float t = -static_cast<float>(newest.tMs - s.tMs) / 1000.0f - meanT;
    sxy += t * (s.posM - meanP);
    sxx += t * t;
  }
  if (sxx <= 0.0f) return 0.0f;
  return sxy / sxx;
}

bool Track::isStale(uint32_t nowMs, uint32_t maxPauseMs) const {
  if (count_ == 0) return false;
  return nowMs - lastProgressMs_ > maxPauseMs;
}

}  // namespace presence
