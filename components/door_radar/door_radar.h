#pragma once
// ESPHome-lijm rond de framework-vrije LD2410-parser en persoonsdetector.
// Contracten: specs/001-mmwave-person-detection/contracts/ (esphome-config, ha-entities,
// diagnostic-log). ESPHome-naamgeving (snake_case) volgens constitutie v2.0.0.

#include <cstdint>

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_EVENT
#include "esphome/components/event/event.h"
#endif

#include "ld2410_commands.h"
#include "ld2410_frame_parser.h"
#include "presence_detector.h"

namespace esphome::door_radar {

/// Welke drempel een DoorRadarNumber instelt (contracts/ha-entities.md, Configuratie).
enum class NumberField : uint8_t {
  MIN_RANGE,
  MAX_RANGE,
  MIN_SPEED,
  MAX_SPEED,
  MIN_DURATION,
  MIN_DISPLACEMENT,
  MAX_PAUSE,
  ABSENT_TIMEOUT,
  MAX_HOLD,
  GATE_MIN_ENERGY,
};

class DoorRadarNumber;

class DoorRadar : public Component, public uart::UARTDevice {
 public:
  /// Na alle numbers opstarten, zodat hun herstelde waarden beschikbaar zijn (T041).
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void loop() override;
  void dump_config() override;

  // Alleen-YAML afstemwaarden (contracts/esphome-config.md).
  void set_reversal_hysteresis_m(float value) { yaml_config_.reversalHysteresisM = value; }
  void set_evaluation_window_ms(uint32_t value) { yaml_config_.evaluationWindowMs = value; }
  void set_background_margin(uint8_t value) { yaml_config_.backgroundMargin = value; }
  void set_background_time_constant_ms(uint32_t value) { yaml_config_.backgroundTimeConstantMs = value; }
  void set_startup_ms(uint32_t value) { yaml_config_.startupMs = value; }
  void set_sensor_timeout_ms(uint32_t value) { yaml_config_.sensorTimeoutMs = value; }
  void set_ack_timeout_ms(uint32_t value) { ack_timeout_ms_ = value; }

#ifdef USE_BINARY_SENSOR
  void set_presence_binary_sensor(binary_sensor::BinarySensor *s) { presence_binary_sensor_ = s; }
  void set_sensor_fault_binary_sensor(binary_sensor::BinarySensor *s) { sensor_fault_binary_sensor_ = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_direction_text_sensor(text_sensor::TextSensor *s) { direction_text_sensor_ = s; }
  void set_reject_reason_text_sensor(text_sensor::TextSensor *s) { reject_reason_text_sensor_ = s; }
#endif
#ifdef USE_EVENT
  void set_detection_event(event::Event *e) { detection_event_ = e; }
#endif
#ifdef USE_SENSOR
  void set_position_sensor(sensor::Sensor *s) { position_sensor_ = s; }
  void set_speed_sensor(sensor::Sensor *s) { speed_sensor_ = s; }
  void set_gate_energy_sensor(uint8_t gate, sensor::Sensor *s) {
    if (gate < presence::GATE_COUNT) gate_energy_sensors_[gate] = s;
  }
#endif

  /// Door number::setup() aangeroepen (vóór DoorRadar::setup()).
  void register_number(DoorRadarNumber *number);
  /// Wijziging vanuit Home Assistant; true = geaccepteerd en actief (FR-011, FR-018).
  bool control_number(NumberField field, uint8_t gate, float value);
  /// Huidige actieve waarde van een veld, in de eenheid van de entiteit.
  float number_value(NumberField field, uint8_t gate) const;

  /// Radar opnieuw in engineering-modus zetten (research R4).
  void reinit_radar();

 protected:
  enum class InitState : uint8_t {
    DONE,
    SEND_ENABLE_CONFIG,
    WAIT_ENABLE_CONFIG,
    SEND_ENGINEERING,
    WAIT_ENGINEERING,
    SEND_END_CONFIG,
    WAIT_END_CONFIG,
    BACKOFF,
  };

  void read_uart_(uint32_t now);
  void handle_frame_(const ld2410::RadarFrame &frame, uint32_t now);
  void handle_ack_(const ld2410::AckFrame &ack, uint32_t now);
  void step_init_(uint32_t now);
  void send_command_(uint16_t command, uint32_t now);
  void init_command_failed_(uint32_t now);
  void handle_events_();
  void publish_status_();
  void publish_diagnostics_();
  void log_evaluation_(uint32_t now);
  static void apply_number_value_(presence::DetectorConfig &config, NumberField field, uint8_t gate, float value);
  static float read_number_value_(const presence::DetectorConfig &config, NumberField field, uint8_t gate);

  presence::DetectorConfig yaml_config_{};
  ld2410::FrameParser parser_{};
  presence::PresenceDetector detector_{presence::DetectorConfig{}};

  static constexpr uint8_t MAX_NUMBERS = 18;
  DoorRadarNumber *numbers_[MAX_NUMBERS]{};
  uint8_t number_count_{0};

  // Initialisatie van de radar (engineering-modus).
  InitState init_state_{InitState::SEND_ENABLE_CONFIG};
  uint16_t pending_command_{0};
  uint8_t command_attempts_{0};
  uint8_t init_cycles_{0};
  uint32_t init_deadline_ms_{0};
  uint32_t backoff_ms_{0};
  uint32_t ack_timeout_ms_{500};
  bool engineering_confirmed_{false};
  uint32_t last_engineering_frame_ms_{0};
  uint32_t last_basic_frame_ms_{0};

  // Tellers en logbegrenzing.
  presence::PresenceDetector::State last_state_{presence::PresenceDetector::State::Startup};
  uint32_t valid_frames_{0};
  uint32_t reported_invalid_frames_{0};
  uint32_t last_invalid_warning_ms_{0};
  uint32_t eval_log_window_start_ms_{0};
  uint8_t eval_log_count_{0};
  presence::RejectReason last_logged_reason_{presence::RejectReason::Startup};
  uint32_t reported_dropped_events_{0};

  // Laatst gepubliceerde waarden (alleen bij verandering publiceren).
  presence::PresenceStatus published_status_{presence::PresenceStatus::NoPerson};
  bool status_published_{false};
  presence::RejectReason published_reason_{presence::RejectReason::Startup};
  bool reason_published_{false};
  float published_position_{0.0f};
  float published_speed_{0.0f};
  int16_t published_gate_energy_[presence::GATE_COUNT]{-1, -1, -1, -1, -1, -1, -1, -1, -1};
  bool diagnostics_published_{false};

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *presence_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *sensor_fault_binary_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *direction_text_sensor_{nullptr};
  text_sensor::TextSensor *reject_reason_text_sensor_{nullptr};
#endif
#ifdef USE_EVENT
  event::Event *detection_event_{nullptr};
#endif
#ifdef USE_SENSOR
  sensor::Sensor *position_sensor_{nullptr};
  sensor::Sensor *speed_sensor_{nullptr};
  sensor::Sensor *gate_energy_sensors_[presence::GATE_COUNT]{};
#endif
};

}  // namespace esphome::door_radar
