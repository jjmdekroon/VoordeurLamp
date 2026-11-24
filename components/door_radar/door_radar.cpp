#include "door_radar.h"

#include <cinttypes>
#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_NUMBER
#include "door_radar_number.h"
#endif

namespace esphome::door_radar {

static const char *const TAG = "door_radar";

namespace {

/// Hoogstens zoveel UART-bytes per loop() (constitutie I: loop < 30 ms).
constexpr size_t MAX_BYTES_PER_LOOP = 256;
constexpr size_t READ_CHUNK = 64;
/// Pogingen per commando voordat de reeks als mislukt geldt (research R4).
constexpr uint8_t MAX_COMMAND_ATTEMPTS = 3;
constexpr uint32_t FIRST_BACKOFF_MS = 1000;
constexpr uint32_t MAX_BACKOFF_MS = 30000;
/// Komen er zolang alleen basisframes binnen, dan opnieuw engineering-modus aanzetten.
constexpr uint32_t ENGINEERING_TIMEOUT_MS = 3000;
/// Basisframes tellen als "recent" binnen dit venster.
constexpr uint32_t RECENT_FRAME_MS = 1000;
/// Diagnose-entiteiten hoogstens zo vaak bijwerken (FR-017).
constexpr uint32_t DIAGNOSTICS_INTERVAL_MS = 1000;
/// DEBUG-evaluatieregels per seconde (contracts/diagnostic-log.md).
constexpr uint8_t MAX_EVAL_LOGS_PER_SECOND = 10;
constexpr uint32_t EVAL_LOG_WINDOW_MS = 1000;
/// Ongeldige frames hoogstens zo vaak melden.
constexpr uint32_t INVALID_FRAME_WARNING_INTERVAL_MS = 10000;
constexpr float MS_PER_S = 1000.0f;

const char *number_field_name(NumberField field) {
  switch (field) {
    case NumberField::MIN_RANGE: return "min_range";
    case NumberField::MAX_RANGE: return "max_range";
    case NumberField::MIN_SPEED: return "min_speed";
    case NumberField::MAX_SPEED: return "max_speed";
    case NumberField::MIN_DURATION: return "min_duration";
    case NumberField::MIN_DISPLACEMENT: return "min_displacement";
    case NumberField::MAX_PAUSE: return "max_pause";
    case NumberField::ABSENT_TIMEOUT: return "absent_timeout";
    case NumberField::MAX_HOLD: return "max_hold";
    case NumberField::GATE_MIN_ENERGY: return "gate_min_energy";
  }
  return "unknown";
}

uint32_t seconds_to_ms(float seconds) {
  if (!(seconds > 0.0f)) return 0;
  return static_cast<uint32_t>(std::lround(seconds * MS_PER_S));
}

bool same_float(float a, float b) {
  if (std::isnan(a) && std::isnan(b)) return true;
  return a == b;
}

/// Afronden voor publicatie, zodat kleine schommelingen geen nieuwe publicatie geven.
float round2(float value) {
  if (std::isnan(value)) return value;
  return std::round(value * 100.0f) / 100.0f;
}

}  // namespace

// ---------------------------------------------------------------- configuratie

void DoorRadar::apply_number_value_(presence::DetectorConfig &c, NumberField field, uint8_t gate, float value) {
  switch (field) {
    case NumberField::MIN_RANGE: c.minRangeM = value; break;
    case NumberField::MAX_RANGE: c.maxRangeM = value; break;
    case NumberField::MIN_SPEED: c.minSpeedMps = value; break;
    case NumberField::MAX_SPEED: c.maxSpeedMps = value; break;
    case NumberField::MIN_DURATION: c.minDurationMs = seconds_to_ms(value); break;
    case NumberField::MIN_DISPLACEMENT: c.minNetDisplacementM = value; break;
    case NumberField::MAX_PAUSE: c.maxPauseMs = seconds_to_ms(value); break;
    case NumberField::ABSENT_TIMEOUT: c.absentTimeoutMs = seconds_to_ms(value); break;
    case NumberField::MAX_HOLD: c.maxHoldMs = seconds_to_ms(value); break;
    case NumberField::GATE_MIN_ENERGY:
      if (gate < presence::GATE_COUNT) {
        long rounded = std::lround(value);
        if (rounded < 0) rounded = 0;
        if (rounded > 255) rounded = 255;  // validate() weigert > 100
        c.minGateEnergy[gate] = static_cast<uint8_t>(rounded);
      }
      break;
  }
}

float DoorRadar::read_number_value_(const presence::DetectorConfig &c, NumberField field, uint8_t gate) {
  switch (field) {
    case NumberField::MIN_RANGE: return c.minRangeM;
    case NumberField::MAX_RANGE: return c.maxRangeM;
    case NumberField::MIN_SPEED: return c.minSpeedMps;
    case NumberField::MAX_SPEED: return c.maxSpeedMps;
    case NumberField::MIN_DURATION: return c.minDurationMs / MS_PER_S;
    case NumberField::MIN_DISPLACEMENT: return c.minNetDisplacementM;
    case NumberField::MAX_PAUSE: return c.maxPauseMs / MS_PER_S;
    case NumberField::ABSENT_TIMEOUT: return c.absentTimeoutMs / MS_PER_S;
    case NumberField::MAX_HOLD: return c.maxHoldMs / MS_PER_S;
    case NumberField::GATE_MIN_ENERGY: return gate < presence::GATE_COUNT ? c.minGateEnergy[gate] : 0.0f;
  }
  return NAN;
}

void DoorRadar::register_number(DoorRadarNumber *number) {
  if (number_count_ < MAX_NUMBERS) numbers_[number_count_++] = number;
}

float DoorRadar::number_value(NumberField field, uint8_t gate) const {
  return read_number_value_(detector_.config(), field, gate);
}

bool DoorRadar::control_number(NumberField field, uint8_t gate, float value) {
  presence::DetectorConfig candidate = detector_.config();
  apply_number_value_(candidate, field, gate, value);
  presence::ConfigError error;
  if (detector_.applyConfig(candidate, &error)) {
    ESP_LOGI(TAG, "config_changed field=%s value=%.2f", number_field_name(field), value);
    return true;
  }
  ESP_LOGW(TAG, "config_rejected field=%s value=%.2f reason=%s", number_field_name(field), value,
           presence::toString(error.code));
  return false;
}

// ---------------------------------------------------------------- levenscyclus

void DoorRadar::setup() {
  // T041: YAML-standaarden + herstelde waarden samen valideren.
  presence::DetectorConfig defaults = yaml_config_;
  presence::DetectorConfig active = yaml_config_;
  bool restored_any = false;
#ifdef USE_NUMBER
  for (uint8_t i = 0; i < number_count_; i++) {
    DoorRadarNumber *n = numbers_[i];
    apply_number_value_(defaults, n->field(), n->gate(), n->initial_value());
    apply_number_value_(active, n->field(), n->gate(),
                        n->has_restored_value() ? n->restored_value() : n->initial_value());
    restored_any |= n->has_restored_value();
  }
#endif

  presence::ConfigError error;
  bool overwrite_restored = false;
  if (!presence::validate(active, &error)) {
    ESP_LOGE(TAG, "config_restored_invalid field=%s reason=%s -> standaardwaarden", presence::toString(error.field),
             presence::toString(error.code));
    active = defaults;
    overwrite_restored = restored_any;
    if (!presence::validate(active, &error)) {
      ESP_LOGE(TAG, "config_defaults_invalid field=%s reason=%s -> ingebouwde standaardwaarden",
               presence::toString(error.field), presence::toString(error.code));
      active = presence::DetectorConfig{};
    }
  }
  detector_.applyConfig(active);

#ifdef USE_NUMBER
  for (uint8_t i = 0; i < number_count_; i++) {
    DoorRadarNumber *n = numbers_[i];
    const float value = read_number_value_(detector_.config(), n->field(), n->gate());
    n->publish_state(value);
    if (overwrite_restored && n->has_restored_value()) n->store(value);
  }
#endif

#ifdef USE_BINARY_SENSOR
  if (presence_binary_sensor_ != nullptr) presence_binary_sensor_->publish_initial_state(false);
  if (sensor_fault_binary_sensor_ != nullptr) sensor_fault_binary_sensor_->publish_initial_state(false);
#endif
  status_published_ = true;
  published_status_ = presence::PresenceStatus::NoPerson;

  this->set_interval(DIAGNOSTICS_INTERVAL_MS, [this]() { this->publish_diagnostics_(); });

  init_state_ = InitState::SEND_ENABLE_CONFIG;
  command_attempts_ = 0;
}

void DoorRadar::loop() {
  const uint32_t now = millis();
  read_uart_(now);
  step_init_(now);

  // Alleen basisframes: engineering-modus is (na een herstart van de radar) uit.
  if (init_state_ == InitState::DONE && last_basic_frame_ms_ != 0 && now - last_basic_frame_ms_ < RECENT_FRAME_MS &&
      now - last_engineering_frame_ms_ > ENGINEERING_TIMEOUT_MS) {
    ESP_LOGW(TAG, "only_basic_frames -> engineering_mode opnieuw");
    reinit_radar();
  }

  detector_.tick(now);
  handle_events_();
  publish_status_();

  const auto state = detector_.state();
  if (last_state_ == presence::PresenceDetector::State::Startup && state == presence::PresenceDetector::State::Idle) {
    ESP_LOGI(TAG, "ready startup_ms=%" PRIu32, detector_.config().startupMs);
  }
  last_state_ = state;
}

void DoorRadar::dump_config() {
  ESP_LOGCONFIG(TAG, "Door radar:");
  const auto &c = detector_.config();
  ESP_LOGCONFIG(TAG, "  Range: %.2f - %.2f m", c.minRangeM, c.maxRangeM);
  ESP_LOGCONFIG(TAG, "  Speed: %.2f - %.2f m/s", c.minSpeedMps, c.maxSpeedMps);
  ESP_LOGCONFIG(TAG, "  Min duration: %" PRIu32 " ms, min displacement: %.2f m", c.minDurationMs,
                c.minNetDisplacementM);
  ESP_LOGCONFIG(TAG, "  Max pause: %" PRIu32 " ms, absent timeout: %" PRIu32 " ms, max hold: %" PRIu32 " ms",
                c.maxPauseMs, c.absentTimeoutMs, c.maxHoldMs);
  ESP_LOGCONFIG(TAG, "  Reversal hysteresis: %.2f m, evaluation window: %" PRIu32 " ms", c.reversalHysteresisM,
                c.evaluationWindowMs);
  ESP_LOGCONFIG(TAG, "  Background margin: %u, time constant: %" PRIu32 " ms", c.backgroundMargin,
                c.backgroundTimeConstantMs);
  ESP_LOGCONFIG(TAG, "  Startup: %" PRIu32 " ms, sensor timeout: %" PRIu32 " ms, ACK timeout: %" PRIu32 " ms",
                c.startupMs, c.sensorTimeoutMs, ack_timeout_ms_);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Presence", presence_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Sensor fault", sensor_fault_binary_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Direction", direction_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Reject reason", reject_reason_text_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Position", position_sensor_);
  LOG_SENSOR("  ", "Speed", speed_sensor_);
#endif
}

// ---------------------------------------------------------------- UART en radar

void DoorRadar::read_uart_(uint32_t now) {
  uint8_t buffer[READ_CHUNK];
  size_t budget = MAX_BYTES_PER_LOOP;
  while (budget > 0) {
    size_t available = this->available();
    if (available == 0) break;
    size_t n = available;
    if (n > sizeof(buffer)) n = sizeof(buffer);
    if (n > budget) n = budget;
    if (!this->read_array(buffer, n)) break;
    budget -= n;
    for (size_t i = 0; i < n; i++) {
      switch (parser_.feed(buffer[i], now)) {
        case ld2410::FrameParser::Result::Report:
          handle_frame_(parser_.frame(), now);
          break;
        case ld2410::FrameParser::Result::Ack:
          handle_ack_(parser_.ack(), now);
          break;
        default:
          break;
      }
    }
  }

  const uint32_t invalid = parser_.invalidFrameCount();
  if (invalid != reported_invalid_frames_ && now - last_invalid_warning_ms_ >= INVALID_FRAME_WARNING_INTERVAL_MS) {
    ESP_LOGW(TAG, "invalid_frames count=%" PRIu32 " valid=%" PRIu32, invalid - reported_invalid_frames_,
             valid_frames_);
    reported_invalid_frames_ = invalid;
    last_invalid_warning_ms_ = now;
  }
}

void DoorRadar::handle_frame_(const ld2410::RadarFrame &frame, uint32_t now) {
  valid_frames_++;
  if (frame.type == ld2410::FrameType::Engineering) {
    last_engineering_frame_ms_ = now;
    engineering_confirmed_ = true;
  } else {
    last_basic_frame_ms_ = now;
  }
  detector_.onFrame(frame);
  log_evaluation_(now);
}

void DoorRadar::send_command_(uint16_t command, uint32_t now) {
  uint8_t buffer[ld2410::MAX_COMMAND_FRAME];
  size_t length = 0;
  switch (command) {
    case ld2410::CMD_ENABLE_CONFIGURATION:
      length = ld2410::buildEnableConfiguration(buffer, sizeof(buffer));
      break;
    case ld2410::CMD_ENABLE_ENGINEERING_MODE:
      length = ld2410::buildEnableEngineeringMode(buffer, sizeof(buffer));
      break;
    case ld2410::CMD_END_CONFIGURATION:
      length = ld2410::buildEndConfiguration(buffer, sizeof(buffer));
      break;
    default:
      return;
  }
  if (length == 0) return;
  this->write_array(buffer, length);
  pending_command_ = command;
  command_attempts_++;
  init_deadline_ms_ = now + ack_timeout_ms_;
}

void DoorRadar::step_init_(uint32_t now) {
  switch (init_state_) {
    case InitState::DONE:
      return;
    case InitState::SEND_ENABLE_CONFIG:
      send_command_(ld2410::CMD_ENABLE_CONFIGURATION, now);
      init_state_ = InitState::WAIT_ENABLE_CONFIG;
      return;
    case InitState::SEND_ENGINEERING:
      send_command_(ld2410::CMD_ENABLE_ENGINEERING_MODE, now);
      init_state_ = InitState::WAIT_ENGINEERING;
      return;
    case InitState::SEND_END_CONFIG:
      send_command_(ld2410::CMD_END_CONFIGURATION, now);
      init_state_ = InitState::WAIT_END_CONFIG;
      return;
    case InitState::WAIT_ENABLE_CONFIG:
    case InitState::WAIT_ENGINEERING:
    case InitState::WAIT_END_CONFIG:
      if (static_cast<int32_t>(now - init_deadline_ms_) >= 0) {
        if (command_attempts_ < MAX_COMMAND_ATTEMPTS) {
          send_command_(pending_command_, now);
        } else {
          init_command_failed_(now);
        }
      }
      return;
    case InitState::BACKOFF:
      if (static_cast<int32_t>(now - init_deadline_ms_) >= 0) {
        command_attempts_ = 0;
        init_state_ = InitState::SEND_ENABLE_CONFIG;
      }
      return;
  }
}

void DoorRadar::init_command_failed_(uint32_t now) {
  ESP_LOGW(TAG, "engineering_mode failed command=0x%04X attempts=%u", pending_command_, command_attempts_);
  // Best effort: configuratiemodus verlaten, anders stuurt de radar geen rapporten meer.
  uint8_t buffer[ld2410::MAX_COMMAND_FRAME];
  const size_t length = ld2410::buildEndConfiguration(buffer, sizeof(buffer));
  if (length > 0) this->write_array(buffer, length);

  backoff_ms_ = backoff_ms_ == 0 ? FIRST_BACKOFF_MS : backoff_ms_ * 2;
  if (backoff_ms_ > MAX_BACKOFF_MS) backoff_ms_ = MAX_BACKOFF_MS;
  init_deadline_ms_ = now + backoff_ms_;
  init_state_ = InitState::BACKOFF;
  init_cycles_++;
}

void DoorRadar::handle_ack_(const ld2410::AckFrame &ack, uint32_t now) {
  const bool waiting = init_state_ == InitState::WAIT_ENABLE_CONFIG || init_state_ == InitState::WAIT_ENGINEERING ||
                       init_state_ == InitState::WAIT_END_CONFIG;
  if (!waiting || !ld2410::isAckFor(ack, pending_command_)) return;

  if (!ld2410::isAckSuccess(ack, pending_command_)) {
    // Mislukte ACK telt als poging; na de timeout volgt een nieuwe poging of de backoff.
    init_deadline_ms_ = now;
    return;
  }

  command_attempts_ = 0;
  switch (init_state_) {
    case InitState::WAIT_ENABLE_CONFIG:
      init_state_ = InitState::SEND_ENGINEERING;
      break;
    case InitState::WAIT_ENGINEERING:
      init_state_ = InitState::SEND_END_CONFIG;
      break;
    case InitState::WAIT_END_CONFIG:
      ESP_LOGI(TAG, "engineering_mode ok attempts=%u", static_cast<unsigned>(init_cycles_ + 1));
      init_state_ = InitState::DONE;
      init_cycles_ = 0;
      backoff_ms_ = 0;
      last_engineering_frame_ms_ = now;  // respijt voordat "alleen basisframes" telt
      break;
    default:
      break;
  }
}

void DoorRadar::reinit_radar() {
  if (init_state_ != InitState::DONE) return;  // loopt al, of wacht op backoff
  command_attempts_ = 0;
  init_state_ = InitState::SEND_ENABLE_CONFIG;
}

// ---------------------------------------------------------------- publiceren

void DoorRadar::handle_events_() {
  presence::DetectionEvent event;
  while (detector_.pollEvent(event)) {
    switch (event.kind) {
      case presence::EventKind::PersonDetected: {
        const char *direction = presence::toString(event.direction);
        ESP_LOGI(TAG, "person_detected dir=%s dist_m=%.2f", direction, event.distanceM);
#ifdef USE_TEXT_SENSOR
        if (direction_text_sensor_ != nullptr) direction_text_sensor_->publish_state(direction);
#endif
#ifdef USE_EVENT
        if (detection_event_ != nullptr) detection_event_->trigger(direction);
#endif
        break;
      }
      case presence::EventKind::PersonGone:
        ESP_LOGI(TAG, "person_gone");
        break;
      case presence::EventKind::SensorFault:
        ESP_LOGW(TAG, "sensor_timeout no_frame_ms=%" PRIu32, detector_.config().sensorTimeoutMs);
        reinit_radar();
        break;
      case presence::EventKind::SensorRecovered:
        ESP_LOGI(TAG, "sensor_recovered");
        break;
    }
  }
  const uint32_t dropped = detector_.droppedEventCount();
  if (dropped != reported_dropped_events_) {
    ESP_LOGW(TAG, "event_queue_overflow dropped=%" PRIu32, dropped - reported_dropped_events_);
    reported_dropped_events_ = dropped;
  }
}

void DoorRadar::publish_status_() {
  const presence::PresenceStatus status = detector_.status();
  if (status_published_ && status == published_status_) return;
#ifdef USE_BINARY_SENSOR
  if (presence_binary_sensor_ != nullptr) {
    switch (status) {
      case presence::PresenceStatus::PersonPresent:
        presence_binary_sensor_->publish_state(true);
        break;
      case presence::PresenceStatus::NoPerson:
        presence_binary_sensor_->publish_state(false);
        break;
      case presence::PresenceStatus::SensorFault:
        // FR-013 / research R8: geen geldige toestand -> "onbekend" in Home Assistant.
        presence_binary_sensor_->invalidate_state();
        break;
    }
  }
  if (sensor_fault_binary_sensor_ != nullptr) {
    sensor_fault_binary_sensor_->publish_state(status == presence::PresenceStatus::SensorFault);
  }
#endif
  published_status_ = status;
  status_published_ = true;
}

void DoorRadar::publish_diagnostics_() {
  const presence::Evaluation &eval = detector_.lastEvaluation();
#ifdef USE_SENSOR
  const float position = round2(eval.positionM);
  if (position_sensor_ != nullptr && (!diagnostics_published_ || !same_float(position, published_position_))) {
    position_sensor_->publish_state(position);
    published_position_ = position;
  }
  const float speed = round2(std::isnan(eval.positionM) ? 0.0f : eval.speedMps);
  if (speed_sensor_ != nullptr && (!diagnostics_published_ || !same_float(speed, published_speed_))) {
    speed_sensor_->publish_state(speed);
    published_speed_ = speed;
  }
  for (uint8_t g = 0; g < presence::GATE_COUNT; g++) {
    sensor::Sensor *s = gate_energy_sensors_[g];
    if (s == nullptr) continue;
    const int16_t energy = eval.gateEnergy[g];
    if (energy != published_gate_energy_[g]) {
      s->publish_state(energy);
      published_gate_energy_[g] = energy;
    }
  }
#endif
#ifdef USE_TEXT_SENSOR
  if (reject_reason_text_sensor_ != nullptr && (!reason_published_ || eval.reason != published_reason_)) {
    reject_reason_text_sensor_->publish_state(presence::toString(eval.reason));
    published_reason_ = eval.reason;
    reason_published_ = true;
  }
#endif
  diagnostics_published_ = true;
}

void DoorRadar::log_evaluation_(uint32_t now) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
  const presence::Evaluation &eval = detector_.lastEvaluation();
  // Alleen frames waarin iets gebeurt, en hoogstens MAX_EVAL_LOGS_PER_SECOND per seconde.
  if (eval.activeGateMask == 0 && !eval.accepted && eval.reason == last_logged_reason_) return;
  if (now - eval_log_window_start_ms_ >= EVAL_LOG_WINDOW_MS) {
    eval_log_window_start_ms_ = now;
    eval_log_count_ = 0;
  }
  if (eval_log_count_ >= MAX_EVAL_LOGS_PER_SECOND) return;
  eval_log_count_++;
  last_logged_reason_ = eval.reason;

  char gates[presence::GATE_COUNT + 1];
  for (uint8_t g = 0; g < presence::GATE_COUNT; g++) {
    // Zone 8 links, zone 0 rechts (zoals een binair getal).
    gates[presence::GATE_COUNT - 1 - g] = ((eval.activeGateMask >> g) & 1u) ? '1' : '0';
  }
  gates[presence::GATE_COUNT] = '\0';
  ESP_LOGD(TAG, "eval accepted=%d reason=%s pos_m=%.2f speed_mps=%.2f net_m=%.2f dur_ms=%" PRIu32 " gates=0b%s",
           eval.accepted ? 1 : 0, presence::toString(eval.reason), eval.positionM, eval.speedMps,
           eval.netDisplacementM, eval.durationMs, gates);
#else
  (void) now;
#endif
}

}  // namespace esphome::door_radar
