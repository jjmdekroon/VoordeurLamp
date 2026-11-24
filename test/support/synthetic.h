#pragma once
// Hulpcode voor host-tests: synthetische LD2410-engineering-frames en een kleine simulator
// rond PresenceDetector. Alleen voor tests.

#include <cmath>
#include <cstdint>
#include <vector>

#include "ld2410_types.h"
#include "presence_detector.h"

namespace synth {

using ld2410::RadarFrame;
using presence::DetectionEvent;
using presence::EventKind;
using presence::PresenceDetector;

/// Interval tussen frames (10 Hz, ondergrens van de verwachte framesnelheid).
constexpr uint32_t FRAME_MS = 100;
/// Halve breedte van het energieprofiel van één doel (1,5 zone).
constexpr float SPREAD_M = 1.125f;

inline RadarFrame emptyFrame(uint32_t tMs) {
  RadarFrame f;
  f.type = ld2410::FrameType::Engineering;
  f.maxMovingGate = ld2410::MAX_GATE_INDEX;
  f.maxStationaryGate = ld2410::MAX_GATE_INDEX;
  f.timestampMs = tMs;
  return f;
}

/// Voeg een bewegend doel toe met een driehoekig energieprofiel rond posM.
inline void addMovingTarget(RadarFrame &f, float posM, float peak) {
  for (uint8_t g = 0; g < ld2410::GATE_COUNT; g++) {
    const float d = std::fabs(g * ld2410::GATE_SIZE_M - posM);
    const float w = 1.0f - d / SPREAD_M;
    if (w <= 0.0f) continue;
    float e = peak * w;
    if (e > 100.0f) e = 100.0f;
    const uint8_t energy = static_cast<uint8_t>(e);
    if (energy > f.movingGateEnergy[g]) f.movingGateEnergy[g] = energy;
  }
  if (peak > f.movingEnergy) {
    f.movingEnergy = static_cast<uint8_t>(peak > 100.0f ? 100.0f : peak);
    f.movingDistanceCm = static_cast<uint16_t>(posM * 100.0f);
  }
  f.targetState = f.hasStationaryTarget() ? ld2410::TargetState::Both : ld2410::TargetState::Moving;
}

inline void setStationaryTarget(RadarFrame &f, float posM, uint8_t energy) {
  f.stationaryDistanceCm = static_cast<uint16_t>(posM * 100.0f);
  f.stationaryEnergy = energy;
  f.targetState = f.hasMovingTarget() ? ld2410::TargetState::Both : ld2410::TargetState::Stationary;
}

/// Deterministische pseudo-random generator (LCG) voor herhaalbare tests.
struct Lcg {
  uint32_t state{12345};
  uint32_t next() {
    state = state * 1664525u + 1013904223u;
    return state >> 8;
  }
  float uniform() { return static_cast<float>(next() % 10000) / 10000.0f; }
};

/// Simulator: stuurt frames met vaste tussentijd en verzamelt events.
struct Sim {
  PresenceDetector det;
  uint32_t t{1000};
  std::vector<DetectionEvent> events;

  explicit Sim(const presence::DetectorConfig &cfg = presence::DetectorConfig{}) : det(cfg) {}

  void drain() {
    DetectionEvent e;
    while (det.pollEvent(e)) events.push_back(e);
  }

  void send(const RadarFrame &f) {
    det.onFrame(f);
    det.tick(t);
    drain();
    t += FRAME_MS;
  }

  /// Lege frames (niemand in beeld) gedurende ms.
  void quiet(uint32_t ms) {
    const uint32_t end = t + ms;
    while (t < end) send(emptyFrame(t));
  }

  /// Opstartperiode laten verstrijken met lege frames.
  void warmUp() { quiet(det.config().startupMs + FRAME_MS); }

  /// Geen frames, alleen tick (sensor stil) gedurende ms.
  void silence(uint32_t ms) {
    const uint32_t end = t + ms;
    while (t < end) {
      det.tick(t);
      drain();
      t += FRAME_MS;
    }
  }

  /// Persoon loopt van fromM naar toM met speedMps; extra(frame, pos) mag het frame aanvullen.
  template <typename Extra>
  void walk(float fromM, float toM, float speedMps, float peak, Extra extra) {
    const float dir = toM < fromM ? -1.0f : 1.0f;
    float pos = fromM;
    const float step = speedMps * FRAME_MS / 1000.0f;
    while ((dir < 0 && pos >= toM) || (dir > 0 && pos <= toM)) {
      RadarFrame f = emptyFrame(t);
      addMovingTarget(f, pos, peak);
      extra(f, pos);
      send(f);
      pos += dir * step;
    }
  }
  void walk(float fromM, float toM, float speedMps, float peak = 70.0f) {
    walk(fromM, toM, speedMps, peak, [](RadarFrame &, float) {});
  }

  int count(EventKind kind) const {
    int n = 0;
    for (const auto &e : events)
      if (e.kind == kind) n++;
    return n;
  }

  const DetectionEvent *first(EventKind kind) const {
    for (const auto &e : events)
      if (e.kind == kind) return &e;
    return nullptr;
  }
};

}  // namespace synth
