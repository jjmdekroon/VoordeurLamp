#pragma once
// Persoonsdetector: toestandsmachine bovenop achtergrondmodel en bewegingsspoor.
// Contract: specs/001-mmwave-person-detection/contracts/presence-detector-api.md.
// Geen heap, geen framework-headers; tijd wordt geïnjecteerd.

#include <cstdint>

#include "background_model.h"
#include "detector_types.h"
#include "ld2410_types.h"
#include "track.h"

namespace presence {

class PresenceDetector {
 public:
  /// Vaste grootte van de event-wachtrij.
  static constexpr uint8_t EVENT_QUEUE_SIZE = 8;

  /// Interne toestand (data-model.md Toestandsmachine).
  enum class State : uint8_t { Startup, Idle, Candidate, Present, Fault };

  /// Een ongeldige configuratie wordt vervangen door de standaardwaarden.
  explicit PresenceDetector(const DetectorConfig &config);

  void onFrame(const ld2410::RadarFrame &frame);
  void tick(uint32_t nowMs);
  bool pollEvent(DetectionEvent &out);

  PresenceStatus status() const;
  const Evaluation &lastEvaluation() const { return evaluation_; }
  const DetectorConfig &config() const { return config_; }
  bool applyConfig(const DetectorConfig &candidate, ConfigError *error = nullptr);

  State state() const { return state_; }
  const BackgroundModel &background() const { return background_; }
  uint32_t droppedEventCount() const { return droppedEvents_; }
  /// true als de constructor een ongeldige configuratie kreeg en standaardwaarden gebruikt.
  bool usedDefaultConfig() const { return usedDefaultConfig_; }

 private:
  struct GateAnalysis {
    bool hasMotion{false};
    float positionM{0.0f};
    bool outOfRange{false};
    bool weak{false};
    uint16_t activeMask{0};
  };

  void ensureStarted(uint32_t nowMs);
  void checkTimeouts(uint32_t nowMs);
  GateAnalysis analyseGates(const ld2410::RadarFrame &frame) const;
  void updateBackground(const ld2410::RadarFrame &frame);
  void handleCandidate(uint32_t nowMs, const GateAnalysis &gates);
  void handlePresent(const ld2410::RadarFrame &frame, const GateAnalysis &gates);
  void enterIdle();
  void pushEvent(EventKind kind, uint32_t nowMs, Direction direction = Direction::Unknown, float distanceM = 0.0f);
  bool isInRange(uint8_t gate) const;
  static float gateDistanceM(uint8_t gate);
  static int gateIndexForDistanceM(float distanceM);

  DetectorConfig config_{};
  bool usedDefaultConfig_{false};
  State state_{State::Startup};
  bool started_{false};
  uint32_t stateSinceMs_{0};
  uint32_t lastFrameMs_{0};

  Track track_{};
  BackgroundModel background_{};
  Evaluation evaluation_{};

  // Toestand in Present.
  uint32_t presentLastMotionMs_{0};
  uint32_t presentLastSeenMs_{0};
  float presentPosM_{0.0f};

  DetectionEvent events_[EVENT_QUEUE_SIZE]{};
  uint8_t eventHead_{0};
  uint8_t eventCount_{0};
  uint32_t droppedEvents_{0};
};

}  // namespace presence
