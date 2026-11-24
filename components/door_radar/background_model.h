#pragma once
// Adaptief achtergrondmodel per afstandszone (clutter map), data-model.md Background.
// Begroeiing die blijvend in dezelfde zone beweegt, verhoogt zijn eigen achtergrond en valt weg
// (FR-005). Geen heap, geen framework-headers.

#include <cstdint>

#include "detector_types.h"

namespace presence {

class BackgroundModel {
 public:
  /// Grootste tijdstap die in één update meetelt; voorkomt sprongen na een onderbreking.
  static constexpr uint32_t MAX_STEP_MS = 500;
  /// Drempel = gemiddelde + max(marge, DEVIATION_FACTOR x gemiddelde afwijking). Zones met sterk
  /// schommelende energie (begroeiing in de wind) krijgen zo vanzelf een hogere drempel.
  static constexpr float DEVIATION_FACTOR = 3.0f;

  void reset();
  /// Exponentieel gemiddelde per zone bijwerken met tijdconstante timeConstantMs.
  void update(const uint8_t energy[GATE_COUNT], uint32_t nowMs, uint32_t timeConstantMs);
  /// Tijd laten doorlopen zonder bij te werken (model "bevroren").
  void hold(uint32_t nowMs);
  /// data-model.md Background.level.
  float level(uint8_t gate) const { return gate < GATE_COUNT ? level_[gate] : 0.0f; }
  /// Gemiddelde absolute afwijking van de energie per zone.
  float deviation(uint8_t gate) const { return gate < GATE_COUNT ? deviation_[gate] : 0.0f; }
  /// Energie waarboven een zone boven de achtergrond uitkomt.
  float threshold(uint8_t gate, uint8_t margin) const;
  /// data-model.md Background.lastUpdateMs.
  uint32_t lastUpdateMs() const { return lastUpdateMs_; }
  /// Zone actief als energie > max(minEnergy, achtergrond + marge) (research R5, FR-005, FR-015).
  bool isActive(uint8_t gate, uint8_t energy, uint8_t minEnergy, uint8_t margin) const;
  /// Energie boven achtergrond + marge, ongeacht de minimale energie.
  bool isAboveBackground(uint8_t gate, uint8_t energy, uint8_t margin) const;

 private:
  float level_[GATE_COUNT]{};
  float deviation_[GATE_COUNT]{};
  uint32_t lastUpdateMs_{0};
  bool hasTime_{false};
};

}  // namespace presence
