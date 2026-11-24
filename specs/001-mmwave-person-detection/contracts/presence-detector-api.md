# Contract: PresenceDetector API (pure C++)

**Doel**: de interface tussen de ESPHome-lijm (`door_radar`-component) en de framework-vrije
detectielogica. Geen ESPHome-, Arduino- of ESP-IDF-headers (constitutie II). Types: zie
[data-model.md](../data-model.md).

## Interface

```cpp
namespace presence {

class PresenceDetector {
public:
    explicit PresenceDetector(const DetectorConfig& config);

    // Verwerk één geldig radarframe. Niet-blokkerend, geen heap.
    void onFrame(const ld2410::RadarFrame& frame);

    // Elke loop aanroepen, ook zonder frames: bewaakt wegtijd, vasthoudtijd,
    // sensortimeout en opstartperiode.
    void tick(uint32_t nowMs);

    // Volgende gebeurtenis ophalen; false als de wachtrij leeg is.
    bool pollEvent(DetectionEvent& out);

    PresenceStatus status() const;               // NoPerson | PersonPresent | SensorFault
    const Evaluation& lastEvaluation() const;    // diagnose (FR-012, FR-017)
    const DetectorConfig& config() const;

    // Nieuwe configuratie toepassen (FR-011/FR-018). Valideert eerst; bij een fout blijft
    // de huidige configuratie actief en wordt false + de reden teruggegeven.
    bool applyConfig(const DetectorConfig& candidate, ConfigError* error = nullptr);
};

// Valideert alle velden en onderlinge regels.
bool validate(const DetectorConfig& config, ConfigError* error);

}  // namespace presence
```

## Gedragsregels

1. **Tijd**: de detector leest zelf nooit de klok; tijd komt uit `frame.timestampMs` en
   `tick(nowMs)` (deterministisch testbaar).
2. **Events**: vaste wachtrij van 8; bij overloop wordt het oudste weggegooid en een teller
   verhoogd. De lijm MOET `pollEvent` elke loop leegmaken.
3. **Volgorde**: na `PersonDetected` komt precies één `PersonGone` of `SensorFault` voordat er
   een nieuw `PersonDetected` kan komen (FR-008).
4. **Opstarten**: binnen `startupMs` na constructie of na `SensorRecovered` geen
   `PersonDetected` (FR-014).
5. **Sensorfout**: na `sensorTimeoutMs` zonder frame → status `SensorFault` + één
   `SensorFault`-event; een lopende aanwezigheid vervalt zonder `PersonGone` (FR-013). Eerste
   geldige frame daarna → `SensorRecovered` en een nieuwe opstartperiode.
6. **Configuratiewissel**: een geslaagde `applyConfig` breekt een lopend spoor af (`Candidate`
   → `Idle`); `Present` blijft `Present` met de nieuwe tijden. Het achtergrondmodel blijft
   behouden.
7. **Geen allocatie** in constructor en methoden.

## Evaluation (diagnose)

| Veld | Betekenis |
|------|-----------|
| `accepted` | true als dit frame tot `PersonDetected` leidde |
| `reason` | `RejectReason` |
| `positionM` | geschatte positie; NaN als geen actieve zones |
| `speedMps` | afgeleide radiale snelheid (negatief = naderend) |
| `netDisplacementM` | netto verplaatsing in het venster |
| `durationMs` | duur van het huidige spoor |
| `activeGateMask` | bitmasker van zones boven achtergrond + marge |
| `gateEnergy[9]` | bewegingsenergie per zone uit het laatste frame |
