# Data Model: Persoonsdetectie met mmWave-radar

**Feature**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md)

Alle structuren hebben een vaste grootte; er is geen dynamische allocatie (constitutie III).
Tijd is altijd `uint32_t` milliseconden sinds opstart (ESPHome `millis()` op het apparaat, een
geïnjecteerde waarde in tests). Alle structuren hieronder behoren tot de pure laag (geen
ESPHome-headers); de koppeling met Home Assistant staat in
[contracts/ha-entities.md](contracts/ha-entities.md). Afstanden zijn in centimeters (geheel getal) in protocolstructuren
en in meters (`float`) in de detectielogica.

## RadarFrame (Waarneming)

Eén geparseerd rapportframe van de LD2410B. Bron: user manual §2.3.

| Veld | Type | Beschrijving / validatie |
|------|------|--------------------------|
| `type` | enum `Basic`, `Engineering` | Datatype-byte 0x02 resp. 0x01 |
| `targetState` | enum `None`, `Moving`, `Stationary`, `Both` | 0x00–0x03; andere waarden → frame ongeldig |
| `movingDistanceCm` | `uint16_t` | Afstand sterkste bewegende doel |
| `movingEnergy` | `uint8_t` | 0–100; >100 → frame ongeldig |
| `stationaryDistanceCm` | `uint16_t` | Afstand sterkste stilstaande doel |
| `stationaryEnergy` | `uint8_t` | 0–100 |
| `detectionDistanceCm` | `uint16_t` | Door sensor berekende detectieafstand |
| `maxMovingGate` | `uint8_t` | Alleen engineering; ≤ 8 |
| `maxStationaryGate` | `uint8_t` | Alleen engineering; ≤ 8 |
| `movingGateEnergy[9]` | `uint8_t[9]` | Alleen engineering; energie per zone 0..8 |
| `stationaryGateEnergy[9]` | `uint8_t[9]` | Alleen engineering |
| `timestampMs` | `uint32_t` | Ontvangsttijd van het laatste byte |

Een frame is alleen geldig als header, lengte, head `0xAA`, tail `0x55 0x00` en frame-end
`F8 F7 F6 F5` kloppen. Ongeldige frames worden geteld (diagnose) en genegeerd.

**Zone ↔ afstand**: zone *i* ≈ *i* × 0,75 m (zone 0 = direct voor de sensor). Te verifiëren bij
bring-up.

## DetectorConfig (Detectieparameters)

Alle drempels. Standaardwaarden staan in de apparaat-YAML (zie
[contracts/esphome-config.md](contracts/esphome-config.md)). Kolom **HA**: ✓ = instelbaar als
`number` in Home Assistant en bewaard na herstart (FR-011); — = alleen in de YAML.

| Veld | Type | Standaard | Validatie | HA | Eis |
|------|------|-----------|-----------|----|-----|
| `minRangeM` | float | 0,75 | 0 ≤ x < `maxRangeM` | ✓ | FR-006 |
| `maxRangeM` | float | 5,0 | ≤ 6,0 | ✓ | FR-006 |
| `minSpeedMps` | float | 0,3 | > 0 | ✓ | FR-003 |
| `maxSpeedMps` | float | 2,5 | > `minSpeedMps`, ≤ 5 | ✓ | FR-003 |
| `minDurationMs` | uint32 | 1000 | 200–5000 | ✓ | FR-002 |
| `minNetDisplacementM` | float | 0,5 | 0,1–3,0 | ✓ | FR-004 |
| `maxPauseMs` | uint32 | 1000 | 0–3000 | ✓ | FR-002 |
| `absentTimeoutMs` (wegtijd) | uint32 | 5000 | 1000–60000 | ✓ | FR-009 |
| `maxHoldMs` (max. vasthoudtijd) | uint32 | 60000 | ≥ `absentTimeoutMs`, ≤ 600000 | ✓ | FR-009 |
| `minGateEnergy[9]` | uint8[9] | 20 per zone | 0–100 | ✓ (9×) | FR-015 |
| `reversalHysteresisM` | float | 0,3 | > 0 | — | FR-005 |
| `evaluationWindowMs` | uint32 | 1500 | ≥ `minDurationMs` | — | FR-003 |
| `backgroundMargin` | uint8 | 15 | 0–100 | — | FR-005 |
| `backgroundTimeConstantMs` | uint32 | 30000 | > 0 | — | FR-005 |
| `startupMs` | uint32 | 5000 | ≥ 0 | — | FR-014 |
| `sensorTimeoutMs` | uint32 | 3000 | > 0 | — | FR-013 |

Validatie gebeurt op de hele configuratie tegelijk (`DetectorConfig::validate()`), zodat
onderlinge regels (bijv. `minSpeedMps < maxSpeedMps`) gecontroleerd worden:

- **Wijziging vanuit Home Assistant**: kandidaat = huidige configuratie met één gewijzigd veld.
  Ongeldig → kandidaat verworpen, vorige waarde blijft actief en wordt opnieuw gepubliceerd
  (FR-018). Geldig → direct actief (lopend spoor wordt afgebroken naar `Idle`, achtergrond
  blijft behouden).
- **Opstarten**: herstelde waarden als geheel gevalideerd; ongeldig → YAML-standaardwaarden en
  een `ERROR`-log. De detector start nooit met ongeldige drempels.

## Track (Bewegingsspoor)

Interne toestand van de detector.

| Veld | Type | Beschrijving |
|------|------|--------------|
| `samples[64]` | ringbuffer van `{uint32_t tMs; float posM;}` | Positie per frame (energie-zwaartepunt) |
| `count` | `uint8_t` | Aantal geldige samples |
| `startMs` | `uint32_t` | Begin van het spoor |
| `lastMotionMs` | `uint32_t` | Laatste frame met actieve zones |
| `lastPosM` | float | Laatste positie |
| `direction` | enum `Unknown`, `Approaching`, `Receding` | Afgeleid uit het teken van de helling |

Afgeleide grootheden (per evaluatie, niet opgeslagen): `speedMps` (regressiehelling),
`netDisplacementM`, `durationMs`, `reversals`.

Aanvullend (implementatie): `totalSamples` (alle samples sinds de start, ook na overloop van de
ringbuffer) en een voortgangsanker voor de pauzedetectie (`PROGRESS_STEP_M` = 0,15 m binnen
`maxPauseMs`). Een spoor telt pas als "aanhoudend" bij minstens 8 samples en minstens 4 samples
per seconde; anders is de reden `TooShort`.

## Background (achtergrondmodel)

| Veld | Type | Beschrijving |
|------|------|--------------|
| `level[9]` | `float[9]` | Exponentieel gemiddelde bewegingsenergie per zone |
| `deviation[9]` | `float[9]` | Exponentieel gemiddelde van de absolute afwijking per zone |
| `lastUpdateMs` | `uint32_t` | Voor tijdgebaseerde update (stap begrensd op 500 ms) |

Drempel per zone: `level + max(backgroundMargin, 3 × deviation)`; zones met sterk schommelende
energie (begroeiing in de wind) krijgen zo vanzelf een hogere drempel.

Bijwerken per toestand (aangepast tijdens de implementatie, zie research R5):

- `Startup`: snel leren met tijdconstante `startupMs / 4` (~98% na de opstartperiode).
- `Idle` en `Candidate`: normaal leren met `backgroundTimeConstantMs`. Ook in `Candidate`, omdat
  begroeiing steeds een kandidaat start en anders nooit geleerd zou worden; een persoon schuift
  door de zones en blijft kort in elke zone.
- `Present` en `Fault`: bevroren, zodat een aanwezige persoon niet in de achtergrond "verdwijnt".

## DetectionEvent (Detectiegebeurtenis)

| Veld | Type | Beschrijving |
|------|------|--------------|
| `kind` | enum `PersonDetected`, `PersonGone`, `SensorFault`, `SensorRecovered` | |
| `direction` | enum `Approaching`, `Receding` | Alleen bij `PersonDetected` |
| `distanceM` | float | Positie op moment van detectie |
| `timestampMs` | `uint32_t` | |

## RejectReason (voor diagnose, FR-012)

`None` (geaccepteerd), `OutOfRange`, `SignalTooWeak`, `SpeedOutOfRange`, `TooShort`,
`NetDisplacementTooSmall`, `DirectionReversal`, `Startup`.

## Toestandsmachine (Detectiestatus)

Publieke status (FR-010): `NoPerson`, `PersonPresent`, `SensorFault`. Interne toestanden en
overgangen:

```text
            startupMs verstreken
 Startup ─────────────────────────► Idle
                                     │ actieve zones binnen bereik
                                     ▼
                         ┌──────► Candidate ──── afwijzing (RejectReason) ──► Idle
                         │           │ alle criteria FR-002..005, FR-015 gehaald
                         │           ▼   → event PersonDetected
                         │        Present
                         │           │ geen beweging én geen stilstaand doel in ±1 zone
                         │           │ gedurende absentTimeoutMs,
                         │           │ of maxHoldMs na laatste beweging verstreken
                         │           ▼   → event PersonGone
                         └──────── Idle

 elke toestand ── geen geldig frame gedurende sensorTimeoutMs ──► Fault (event SensorFault)
 Fault ── geldige frames na herinitialisatie ──► Startup (event SensorRecovered)
```

| Interne toestand | Publieke status | "Persoon aanwezig" in Home Assistant |
|------------------|-----------------|--------------------------------------|
| `Startup`, `Idle`, `Candidate` | `NoPerson` | uit |
| `Present` | `PersonPresent` | aan |
| `Fault` | `SensorFault` | onbekend (zie research R8) |

In `Present` worden nieuwe bewegingen niet opnieuw als detectie gemeld (FR-008); ze verlengen
alleen `lastMotionMs`.
