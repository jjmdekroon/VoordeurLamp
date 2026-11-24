#pragma once
// Instelbare drempel als Home Assistant-number met eigen opslag (FR-011, FR-018, T040).

#include "esphome/core/defines.h"

#ifdef USE_NUMBER

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

#include "door_radar.h"

namespace esphome::door_radar {

class DoorRadarNumber : public number::Number, public Component, public Parented<DoorRadar> {
 public:
  DoorRadarNumber(NumberField field, uint8_t gate) : field_(field), gate_(gate) {}

  /// Herstelt de opgeslagen waarde (standaardprioriteit DATA, dus vóór DoorRadar::setup()).
  void setup() override;
  void dump_config() override;

  void set_initial_value(float value) { initial_value_ = value; }
  float initial_value() const { return initial_value_; }
  bool has_restored_value() const { return has_restored_value_; }
  float restored_value() const { return restored_value_; }
  NumberField field() const { return field_; }
  uint8_t gate() const { return gate_; }

  /// Waarde opslaan in flash (ESPHome schrijft binnen flash_write_interval weg).
  void store(float value);

 protected:
  void control(float value) override;

  NumberField field_;
  uint8_t gate_;
  float initial_value_{0.0f};
  bool has_restored_value_{false};
  float restored_value_{0.0f};
  ESPPreferenceObject pref_;
};

}  // namespace esphome::door_radar

#endif  // USE_NUMBER
