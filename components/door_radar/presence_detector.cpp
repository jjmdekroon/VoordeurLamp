#include "presence_detector.h"

#include <cmath>

namespace presence {

namespace {

constexpr float GATE_SIZE_M = ld2410::GATE_SIZE_M;
/// Kleine tolerantie bij het vergelijken van zone-afstanden met het detectiegebied.
constexpr float RANGE_EPSILON_M = 0.001f;
/// Tijdens de opstartperiode leert de achtergrond met tijdconstante startupMs / 4 (~98% na de
/// opstartperiode), zodat begroeiing direct na het opstarten al grotendeels geleerd is.
constexpr uint32_t STARTUP_LEARNING_DIVISOR = 4;
constexpr uint32_t MIN_STARTUP_TIME_CONSTANT_MS = 500;
/// Stilstaand doel houdt de aanwezigheid vast binnen zoveel zones van de laatste positie (FR-009).
constexpr int HOLD_GATE_TOLERANCE = 1;
constexpr float CM_PER_M = 100.0f;
/// Minimaal aantal frames met beweging in een spoor voordat het als persoon telt.
constexpr uint32_t MIN_TRACK_SAMPLES = 8;
/// Minimale dichtheid van frames met beweging in een spoor (per seconde).
constexpr uint32_t MIN_SAMPLE_RATE_HZ = 4;

}  // namespace

PresenceDetector::PresenceDetector(const DetectorConfig &config) {
  if (validate(config, nullptr)) {
    config_ = config;
  } else {
    config_ = DetectorConfig{};
    usedDefaultConfig_ = true;
  }
  evaluation_.positionM = NAN;
}

float PresenceDetector::gateDistanceM(uint8_t gate) { return static_cast<float>(gate) * GATE_SIZE_M; }

int PresenceDetector::gateIndexForDistanceM(float distanceM) {
  return static_cast<int>(std::lround(distanceM / GATE_SIZE_M));
}

bool PresenceDetector::isInRange(uint8_t gate) const {
  const float d = gateDistanceM(gate);
  return d >= config_.minRangeM - RANGE_EPSILON_M && d <= config_.maxRangeM + RANGE_EPSILON_M;
}

PresenceStatus PresenceDetector::status() const {
  switch (state_) {
    case State::Present:
      return PresenceStatus::PersonPresent;
    case State::Fault:
      return PresenceStatus::SensorFault;
    default:
      return PresenceStatus::NoPerson;
  }
}

void PresenceDetector::ensureStarted(uint32_t nowMs) {
  if (started_) return;
  started_ = true;
  stateSinceMs_ = nowMs;
  lastFrameMs_ = nowMs;
}

void PresenceDetector::pushEvent(EventKind kind, uint32_t nowMs, Direction direction, float distanceM) {
  if (eventCount_ == EVENT_QUEUE_SIZE) {
    // Oudste weggooien (contract-regel 2).
    eventHead_ = static_cast<uint8_t>((eventHead_ + 1) % EVENT_QUEUE_SIZE);
    eventCount_--;
    droppedEvents_++;
  }
  const uint8_t index = static_cast<uint8_t>((eventHead_ + eventCount_) % EVENT_QUEUE_SIZE);
  events_[index] = DetectionEvent{kind, direction, distanceM, nowMs};
  eventCount_++;
}

bool PresenceDetector::pollEvent(DetectionEvent &out) {
  if (eventCount_ == 0) return false;
  out = events_[eventHead_];
  eventHead_ = static_cast<uint8_t>((eventHead_ + 1) % EVENT_QUEUE_SIZE);
  eventCount_--;
  return true;
}

void PresenceDetector::enterIdle() {
  state_ = State::Idle;
  track_.reset();
}

void PresenceDetector::tick(uint32_t nowMs) {
  ensureStarted(nowMs);
  checkTimeouts(nowMs);
}

void PresenceDetector::checkTimeouts(uint32_t nowMs) {
  if (state_ != State::Fault && nowMs - lastFrameMs_ >= config_.sensorTimeoutMs) {
    // FR-013: geen PersonGone; aanwezigheid vervalt via de foutstatus.
    state_ = State::Fault;
    track_.reset();
    pushEvent(EventKind::SensorFault, nowMs);
    return;
  }

  switch (state_) {
    case State::Startup:
      if (nowMs - stateSinceMs_ >= config_.startupMs) enterIdle();
      break;
    case State::Candidate:
      if (nowMs - track_.lastMotionMs() > config_.maxPauseMs) {
        evaluation_.reason = track_.durationMs() >= config_.minDurationMs ? RejectReason::NetDisplacementTooSmall
                                                                          : RejectReason::TooShort;
        enterIdle();
      }
      break;
    case State::Present:
      if (nowMs - presentLastSeenMs_ >= config_.absentTimeoutMs ||
          nowMs - presentLastMotionMs_ >= config_.maxHoldMs) {
        pushEvent(EventKind::PersonGone, nowMs);
        enterIdle();
      }
      break;
    default:
      break;
  }
}

PresenceDetector::GateAnalysis PresenceDetector::analyseGates(const ld2410::RadarFrame &frame) const {
  GateAnalysis result;
  // Stap 1: welke zones zijn actief (FR-005, FR-006, FR-015)?
  for (uint8_t g = 0; g < GATE_COUNT; g++) {
    const uint8_t energy = frame.movingGateEnergy[g];
    const bool aboveBackground = background_.isAboveBackground(g, energy, config_.backgroundMargin);
    const bool aboveMinimum = energy > config_.minGateEnergy[g];
    const bool inRange = isInRange(g);
    if (aboveBackground && aboveMinimum) {
      if (inRange) {
        result.activeMask = static_cast<uint16_t>(result.activeMask | (1u << g));
      } else {
        result.outOfRange = true;
      }
    } else if (aboveBackground && inRange) {
      result.weak = true;
    }
  }
  if (result.activeMask == 0) return result;

  // Stap 2: positie = energie-zwaartepunt boven de achtergrond over de actieve zones en hun
  // buurzones (research R5 laag 2). De buurzones voorkomen dat de positie naar het midden van
  // één zone springt, waardoor een slingerende struik een grote verplaatsing zou lijken.
  float sumWeight = 0.0f;
  float sumWeighted = 0.0f;
  for (uint8_t g = 0; g < GATE_COUNT; g++) {
    if (!isInRange(g)) continue;
    const bool self = (result.activeMask >> g) & 1u;
    const bool left = g > 0 && ((result.activeMask >> (g - 1)) & 1u);
    const bool right = g + 1 < GATE_COUNT && ((result.activeMask >> (g + 1)) & 1u);
    if (!self && !left && !right) continue;
    const float weight = static_cast<float>(frame.movingGateEnergy[g]) - background_.level(g);
    if (weight <= 0.0f) continue;
    sumWeight += weight;
    sumWeighted += weight * gateDistanceM(g);
  }
  if (sumWeight > 0.0f) {
    result.hasMotion = true;
    result.positionM = sumWeighted / sumWeight;
  }
  return result;
}

void PresenceDetector::updateBackground(const ld2410::RadarFrame &frame) {
  // Niet bijwerken in Present: een aanwezige persoon mag niet in de achtergrond opgaan.
  // Wel in Candidate, zodat begroeiing die steeds een kandidaat start toch geleerd wordt;
  // een persoon schuift door de zones en blijft daardoor kort in elke zone.
  switch (state_) {
    case State::Startup: {
      uint32_t tau = config_.startupMs / STARTUP_LEARNING_DIVISOR;
      if (tau < MIN_STARTUP_TIME_CONSTANT_MS) tau = MIN_STARTUP_TIME_CONSTANT_MS;
      if (tau > config_.backgroundTimeConstantMs) tau = config_.backgroundTimeConstantMs;
      background_.update(frame.movingGateEnergy, frame.timestampMs, tau);
      break;
    }
    case State::Idle:
    case State::Candidate:
      background_.update(frame.movingGateEnergy, frame.timestampMs, config_.backgroundTimeConstantMs);
      break;
    default:
      background_.hold(frame.timestampMs);
      break;
  }
}

void PresenceDetector::onFrame(const ld2410::RadarFrame &frame) {
  const uint32_t nowMs = frame.timestampMs;
  ensureStarted(nowMs);
  lastFrameMs_ = nowMs;

  if (state_ == State::Fault) {
    pushEvent(EventKind::SensorRecovered, nowMs);
    state_ = State::Startup;
    stateSinceMs_ = nowMs;
    track_.reset();
  }

  // Alleen engineering-frames bevatten zone-energie; basisframes houden de sensor "levend".
  if (frame.type != ld2410::FrameType::Engineering) {
    checkTimeouts(nowMs);
    return;
  }

  const GateAnalysis gates = analyseGates(frame);
  updateBackground(frame);

  evaluation_.accepted = false;
  evaluation_.activeGateMask = gates.activeMask;
  for (uint8_t g = 0; g < GATE_COUNT; g++) evaluation_.gateEnergy[g] = frame.movingGateEnergy[g];
  evaluation_.positionM = gates.hasMotion ? gates.positionM : NAN;

  switch (state_) {
    case State::Startup:
      evaluation_.reason = RejectReason::Startup;
      break;
    case State::Idle:
      if (gates.hasMotion) {
        state_ = State::Candidate;
        track_.start(nowMs, gates.positionM);
        handleCandidate(nowMs, gates);
      } else if (gates.outOfRange) {
        evaluation_.reason = RejectReason::OutOfRange;
      } else if (gates.weak) {
        evaluation_.reason = RejectReason::SignalTooWeak;
      }
      break;
    case State::Candidate:
      if (gates.hasMotion) {
        track_.add(nowMs, gates.positionM, config_.reversalHysteresisM);
        handleCandidate(nowMs, gates);
      }
      break;
    case State::Present:
      handlePresent(frame, gates);
      break;
    case State::Fault:
      break;
  }

  checkTimeouts(nowMs);
}

void PresenceDetector::handleCandidate(uint32_t nowMs, const GateAnalysis &gates) {
  // Omkering of te lange pauze: spoor opnieuw beginnen vanaf deze positie (FR-002, FR-005).
  if (track_.reversed()) {
    evaluation_.reason = RejectReason::DirectionReversal;
    track_.start(nowMs, gates.positionM);
    return;
  }
  if (track_.isStale(nowMs, config_.maxPauseMs)) {
    evaluation_.reason = RejectReason::NetDisplacementTooSmall;
    track_.start(nowMs, gates.positionM);
    return;
  }

  const uint32_t duration = track_.durationMs();
  const float net = track_.netDisplacementM();
  const float speed = track_.speedMps(config_.evaluationWindowMs);
  evaluation_.durationMs = duration;
  evaluation_.netDisplacementM = net;
  evaluation_.speedMps = speed;

  // "Aanhoudend" (FR-002): genoeg meetpunten en een minimale dichtheid. Een persoon geeft in
  // vrijwel elk frame beweging; losse pieken (regen, insecten) niet.
  const bool sparse = track_.totalSamples() < MIN_TRACK_SAMPLES ||
                      track_.totalSamples() * 1000u < MIN_SAMPLE_RATE_HZ * duration;
  if (duration < config_.minDurationMs || sparse) {
    evaluation_.reason = RejectReason::TooShort;
    return;
  }
  if (std::fabs(net) < config_.minNetDisplacementM) {
    evaluation_.reason = RejectReason::NetDisplacementTooSmall;
    return;
  }
  const float absSpeed = std::fabs(speed);
  if (absSpeed < config_.minSpeedMps || absSpeed > config_.maxSpeedMps) {
    evaluation_.reason = RejectReason::SpeedOutOfRange;
    return;
  }

  // Geaccepteerd: precies één PersonDetected per aanwezigheid (FR-007, FR-008).
  const Direction direction = net < 0.0f ? Direction::Approaching : Direction::Receding;
  evaluation_.accepted = true;
  evaluation_.reason = RejectReason::None;
  state_ = State::Present;
  presentLastMotionMs_ = nowMs;
  presentLastSeenMs_ = nowMs;
  presentPosM_ = gates.positionM;
  pushEvent(EventKind::PersonDetected, nowMs, direction, gates.positionM);
  track_.reset();
}

void PresenceDetector::handlePresent(const ld2410::RadarFrame &frame, const GateAnalysis &gates) {
  const uint32_t nowMs = frame.timestampMs;
  if (gates.hasMotion) {
    presentLastMotionMs_ = nowMs;
    presentLastSeenMs_ = nowMs;
    presentPosM_ = gates.positionM;
    return;
  }
  // Begrensd vasthouden met een stilstaand doel in de buurt van de laatste positie (FR-009).
  if (frame.hasStationaryTarget() && frame.stationaryEnergy > 0 &&
      nowMs - presentLastMotionMs_ < config_.maxHoldMs) {
    const int stationaryGate = gateIndexForDistanceM(static_cast<float>(frame.stationaryDistanceCm) / CM_PER_M);
    const int lastGate = gateIndexForDistanceM(presentPosM_);
    const int diff = stationaryGate - lastGate;
    if (diff >= -HOLD_GATE_TOLERANCE && diff <= HOLD_GATE_TOLERANCE) {
      presentLastSeenMs_ = nowMs;
    }
  }
}

bool PresenceDetector::applyConfig(const DetectorConfig &candidate, ConfigError *error) {
  if (!validate(candidate, error)) return false;
  config_ = candidate;
  // Contract-regel 6: lopend spoor afbreken, Present blijft, achtergrond blijft.
  if (state_ == State::Candidate) enterIdle();
  return true;
}

}  // namespace presence
