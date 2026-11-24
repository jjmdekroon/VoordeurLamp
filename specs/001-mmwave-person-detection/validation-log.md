# Validatielog: Persoonsdetectie met mmWave-radar

Resultaten van de kwaliteitspoorten (constitutie v2.0.0) en de scenario's uit
[quickstart.md](quickstart.md). Per hardwaretest: datum, weer/wind, aantal pogingen, resultaat.

## Build en host-tests

| Datum | Controle | Resultaat |
|-------|----------|-----------|
| 2026-11-24 | `esphome config voordeur-radar.yaml` | Geldig. Eén ESPHome-melding: OTA-wachtwoord kost flash/RAM, encryptie aanbevolen (zie opmerking) |
| 2026-11-24 | `esphome compile voordeur-radar.yaml` (ESPHome 2026.10.2, ESP-IDF 5.5.6) | Geslaagd, geen waarschuwingen in `components/door_radar` |
| 2026-11-24 | `pio test -e native` | 67 van 67 tests geslaagd (9 suites) |

## RAM en flash (constitutie V)

| Datum | Build | RAM | Flash |
|-------|-------|-----|-------|
| 2026-11-24 | Skelet (alleen basis-YAML) | 99 727 B (29,2%) | 847 723 B (46,2%) |
| 2026-11-24 | Volledige feature (alle entiteiten) | 103 143 B (30,2%) | 867 675 B (47,3%) |

Dit is de eerste release; de 10%-regel geldt vanaf de volgende.

**Opmerking OTA**: ESPHome 2026.10 raadt voor OTA een encryptiesleutel aan in plaats van een
wachtwoord. Constitutie IV eist letterlijk een wachtwoord; daarom is het wachtwoord gebruikt. Een
overstap naar `encryption` vergt een kleine constitutiewijziging.

## Hardwaretests

Nog uit te voeren op de echte hardware (taken T017, T026, T032, T036, T044, T047, T048, T049).

| # | Scenario | Datum | Omstandigheden | Pogingen | Resultaat |
|---|----------|-------|----------------|----------|-----------|
| Bring-up | UART-richting, zone-resolutie, framesnelheid | | | | |
| V1 | 40× nadering over het pad | | | | |
| V2 | Eén detectie per nadering | | | | |
| V3 | 20× weglopen | | | | |
| V4 | Stilstaan bij de deur | | | | |
| V5 | Weglopen uit het gebied | | | | |
| V6 | 24 h wind zonder personen | | | | |
| V7 | 10× nadering bij wind | | | | |
| V8 | Sensor loskoppelen en herstellen | | | | |
| V9 | Maximale afstand 4 m, ook na herstart | | | | |
| V10 | Ongeldige maximale snelheid | | | | |
| V11 | WiFi-uitval tijdens nadering | | | | |
| V12 | Detectie binnen 1 s in Home Assistant | | | | |
| V13 | 7 dagen continu | | | | |
| V14 | Stroomuitval | | | | |
| Lusduur | Geen "took a long time" voor `door_radar` | | | | |
