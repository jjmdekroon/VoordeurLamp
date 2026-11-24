# Contract: Home Assistant-entiteiten

**Doel**: wat Home Assistant van het apparaat ziet (FR-010, FR-016, FR-017, FR-011). Namen zijn
de standaardnamen; in de YAML aanpasbaar. Alle entiteiten worden in de YAML onder de
`door_radar`-platforms gedeclareerd (zie [esphome-config.md](esphome-config.md)).

## Bediening en automatisering (direct gepubliceerd, SC-007)

| Entiteit | ESPHome-platform | Waarden | Wanneer gepubliceerd |
|----------|------------------|---------|----------------------|
| Persoon aanwezig | `binary_sensor`, `device_class: presence` | aan / uit / onbekend | bij elke statuswijziging; onbekend tijdens sensorfout (research R8) |
| Richting | `text_sensor` | `naderend` / `verwijderend` | bij elke `PersonDetected` en `DirectionChanged` |
| Detectie | `event`, `event_types: [naderend, verwijderend]` | event | bij elke `PersonDetected` (één per aanwezigheid) en bij elke richtingswissel binnen die aanwezigheid (FR-008) |

"Persoon weg" (FR-009) = "Persoon aanwezig" gaat van aan naar uit.

Na herstel van de verbinding met Home Assistant stuurt ESPHome de actuele toestand van alle
entiteiten opnieuw (FR-016); events uit de onderbreking worden niet nagestuurd.

## Diagnose (`entity_category: diagnostic`, hoogstens 1×/s, alleen bij verandering)

| Entiteit | Platform | Eenheid / waarden |
|----------|----------|-------------------|
| Radar storing | `binary_sensor`, `device_class: problem` | aan = sensorfout |
| Positie | `sensor` | m, 2 decimalen; leeg (NaN) als er geen actieve zones zijn |
| Snelheid | `sensor` | m/s, 2 decimalen; negatief = naderend |
| Afwijzingsreden | `text_sensor` | zie redenen in [diagnostic-log.md](diagnostic-log.md) |
| Zone 0 … Zone 8 energie | 9× `sensor` | 0–100 (bewegingsenergie per zone, 0,75 m per zone); in de YAML uitgemiddeld over 5 s |
| Zone 0 … Zone 8 drempel | 9× `sensor` | 0–100+: energie waarboven de zone als beweging telt = max(minimale energie, achtergrond + max(marge, 3 × schommeling)); hele getallen, alleen bij verandering |
| Reset-oorzaak | `text_sensor` via ESPHome `debug` | tekst (constitutie III) |

## Configuratie (`number`, `entity_category: config`, `restore_value`, `mode: box`)

| Entiteit | Bereik | Stap | Eenheid | Veld (data-model) |
|----------|--------|------|---------|-------------------|
| Minimale afstand | 0,0–6,0 | 0,05 | m | `minRangeM` |
| Maximale afstand | 0,5–6,0 | 0,05 | m | `maxRangeM` |
| Minimale snelheid | 0,05–3,0 | 0,05 | m/s | `minSpeedMps` |
| Maximale snelheid | 0,1–5,0 | 0,05 | m/s | `maxSpeedMps` |
| Minimale duur | 0,2–5,0 | 0,1 | s | `minDurationMs` |
| Minimale verplaatsing | 0,1–3,0 | 0,05 | m | `minNetDisplacementM` |
| Toegestane pauze | 0,0–3,0 | 0,1 | s | `maxPauseMs` |
| Wegtijd | 1–60 | 1 | s | `absentTimeoutMs` |
| Maximale vasthoudtijd | 5–600 | 5 | s | `maxHoldMs` |
| Zone 0 … Zone 8 minimale energie | 0–100 | 1 | — | `minGateEnergy[i]` |

Gedrag (FR-011, FR-018):

- Een geldige wijziging gaat direct in en wordt bewaard (ESPHome schrijft binnen ~1 minuut naar
  flash).
- Een ongeldige combinatie wordt geweigerd: de entiteit springt terug naar de vorige waarde en
  er verschijnt een `WARN`-regel in de log.
- De bereiken hierboven zijn de grenzen per veld; onderlinge regels (min < max, vasthoudtijd ≥
  wegtijd) controleert de detector.
