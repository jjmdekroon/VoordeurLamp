#include "door_radar_number.h"

#ifdef USE_NUMBER

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::door_radar {

static const char *const TAG = "door_radar.number";

void DoorRadarNumber::setup() {
  this->pref_ = this->make_entity_preference<float>();
  float value;
  if (this->pref_.load(&value) && std::isfinite(value)) {
    this->has_restored_value_ = true;
    this->restored_value_ = value;
  }
  // DoorRadar::setup() valideert alle waarden samen en publiceert daarna de actieve waarde.
}

void DoorRadarNumber::store(float value) { this->pref_.save(&value); }

void DoorRadarNumber::control(float value) {
  if (this->parent_->control_number(this->field_, this->gate_, value)) {
    this->publish_state(value);
    this->store(value);
  } else {
    // FR-018: geweigerd; terug naar de actieve waarde.
    this->publish_state(this->parent_->number_value(this->field_, this->gate_));
  }
}

void DoorRadarNumber::dump_config() {
  LOG_NUMBER("", "Door radar number", this);
  ESP_LOGCONFIG(TAG, "  Restored: %s", YESNO(this->has_restored_value_));
}

}  // namespace esphome::door_radar

#endif  // USE_NUMBER
