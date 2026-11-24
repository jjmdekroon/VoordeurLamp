#pragma once
// Bewegingsspoor: vaste ringbuffer met positie per frame, regressiesnelheid, netto verplaatsing,
// richting met hysterese en pauzedetectie. data-model.md Track. Geen heap, geen framework-headers.

#include <cstdint>

#include "detector_types.h"

namespace presence {

struct TrackSample {
  uint32_t tMs{0};
  float posM{0.0f};
};

class Track {
 public:
  /// data-model.md Track.samples: vaste ringbuffer van 64 samples.
  static constexpr uint8_t CAPACITY = 64;
  /// Verplaatsing die als "voortgang" telt; binnen maxPauseMs niet gehaald = pauze (FR-002).
  static constexpr float PROGRESS_STEP_M = 0.15f;

  void reset();
  /// Begin een nieuw spoor op dit sample.
  void start(uint32_t tMs, float posM);
  /// Voeg een sample toe; werkt richting, omkering en voortgang bij.
  void add(uint32_t tMs, float posM, float reversalHysteresisM);

  bool empty() const { return count_ == 0; }
  uint8_t count() const { return count_; }
  /// Alle samples sinds start(), ook als de ringbuffer al is overgelopen.
  uint32_t totalSamples() const { return total_; }
  uint32_t startMs() const { return startMs_; }
  uint32_t lastMotionMs() const { return lastMotionMs_; }
  float startPosM() const { return startPosM_; }
  float lastPosM() const { return lastPosM_; }
  /// Tijd tussen eerste en laatste sample.
  uint32_t durationMs() const { return lastMotionMs_ - startMs_; }
  /// Laatste positie min startpositie (negatief = naderend).
  float netDisplacementM() const { return lastPosM_ - startPosM_; }
  /// Helling van de lineaire regressie over de samples in het venster (m/s).
  float speedMps(uint32_t windowMs) const;
  Direction direction() const { return direction_; }
  /// true als de beweging na het vaststellen van de richting meer dan de hysterese terugging.
  bool reversed() const { return reversed_; }
  /// true als de positie langer dan maxPauseMs niet PROGRESS_STEP_M is veranderd.
  bool isStale(uint32_t nowMs, uint32_t maxPauseMs) const;
  /// Sample i (0 = oudste nog aanwezige).
  const TrackSample &sample(uint8_t i) const;

 private:
  TrackSample samples_[CAPACITY]{};
  uint8_t head_{0};  ///< Index waar het volgende sample komt.
  uint8_t count_{0};
  uint32_t total_{0};
  uint32_t startMs_{0};
  float startPosM_{0.0f};
  uint32_t lastMotionMs_{0};
  float lastPosM_{0.0f};
  Direction direction_{Direction::Unknown};
  float extremePosM_{0.0f};
  bool reversed_{false};
  uint32_t lastProgressMs_{0};
  float progressPosM_{0.0f};
};

}  // namespace presence
