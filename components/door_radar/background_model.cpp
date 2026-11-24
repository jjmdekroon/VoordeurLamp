#include "background_model.h"

#include <cmath>

namespace presence {

void BackgroundModel::reset() {
  for (float &l : level_) l = 0.0f;
  for (float &d : deviation_) d = 0.0f;
  lastUpdateMs_ = 0;
  hasTime_ = false;
}

void BackgroundModel::update(const uint8_t energy[GATE_COUNT], uint32_t nowMs, uint32_t timeConstantMs) {
  if (!hasTime_) {
    lastUpdateMs_ = nowMs;
    hasTime_ = true;
    return;
  }
  uint32_t dt = nowMs - lastUpdateMs_;
  lastUpdateMs_ = nowMs;
  if (dt == 0 || timeConstantMs == 0) return;
  if (dt > MAX_STEP_MS) dt = MAX_STEP_MS;
  float alpha = static_cast<float>(dt) / static_cast<float>(timeConstantMs);
  if (alpha > 1.0f) alpha = 1.0f;
  for (uint8_t g = 0; g < GATE_COUNT; g++) {
    const float e = static_cast<float>(energy[g]);
    const float diff = e - level_[g];
    deviation_[g] += (std::fabs(diff) - deviation_[g]) * alpha;
    level_[g] += diff * alpha;
  }
}

void BackgroundModel::hold(uint32_t nowMs) {
  lastUpdateMs_ = nowMs;
  hasTime_ = true;
}

float BackgroundModel::threshold(uint8_t gate, uint8_t margin) const {
  float spread = DEVIATION_FACTOR * deviation(gate);
  if (spread < static_cast<float>(margin)) spread = static_cast<float>(margin);
  return level(gate) + spread;
}

bool BackgroundModel::isAboveBackground(uint8_t gate, uint8_t energy, uint8_t margin) const {
  return static_cast<float>(energy) > threshold(gate, margin);
}

bool BackgroundModel::isActive(uint8_t gate, uint8_t energy, uint8_t minEnergy, uint8_t margin) const {
  return energy > minEnergy && isAboveBackground(gate, energy, margin);
}

}  // namespace presence
