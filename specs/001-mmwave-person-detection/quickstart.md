# Quickstart: Persoonsdetectie met mmWave-radar valideren (ESPHome)

**Feature**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md)

## Benodigdheden

- Seeed XIAO ESP32-S3 met daarop gestapeld de Seeed 24GHz mmWave for XIAO (LD2410B)
- USB-C-kabel; Python 3 met een virtuele omgeving
- Home Assistant met de ESPHome-integratie, op hetzelfde netwerk
- Voor de buitentest: behuizing zonder metaal (datasheet §8), stevige montage bij de voordeur

## 1. Omgeving opzetten

```bash
python -m venv .venv
. .venv/Scripts/activate          # Windows (Git Bash); Linux/macOS: . .venv/bin/activate
pip install -r requirements.txt   # vastgepinde ESPHome-versie, installeert ook PlatformIO
cp secrets.yaml.example secrets.yaml   # en invullen: wifi, api-sleutel, ota-wachtwoord
```

## 2. Host-tests (geen hardware nodig)

```bash
.venv/Scripts/pio test -e native   # PlatformIO uit de virtuele omgeving
```

Verwacht: alle tests slagen, waaronder de parser met de testvectoren uit
[contracts/ld2410-uart-protocol.md](contracts/ld2410-uart-protocol.md), de configuratievalidatie
(FR-018) en de detectorscenario's (naderend, verwijderend, struik, struik + persoon, bocht met
pauze, stilstand met vasthouden, sensortimeout).

## 3. Valideren, bouwen en flashen

```bash
.venv/Scripts/esphome config voordeur-radar.yaml
.venv/Scripts/esphome compile voordeur-radar.yaml
.venv/Scripts/esphome run voordeur-radar.yaml          # eerste keer via USB, daarna via WiFi (OTA)
.venv/Scripts/esphome logs voordeur-radar.yaml
```

Verwacht: configuratie geldig; build zonder nieuwe waarschuwingen in `components/door_radar`;
de buildoutput toont RAM- en flashgebruik (noteren, constitutie V).

## 4. Bring-up (eenmalig, binnen)

| Controle | Verwacht ([logformaat](contracts/diagnostic-log.md)) |
|----------|------------------------------------------------------|
| Reset-oorzaak zichtbaar | regel van de `debug`-component bij opstarten |
| UART-richting D2/D3 klopt (GPIO3/GPIO4, via de gestapelde header) | `engineering_mode ok`; zo niet (`sensor_timeout`), `radar_rx_pin`/`radar_tx_pin` in de substitutions omwisselen; geen bedrading nodig |
| Zone-resolutie 0,75 m | met `logger: level: DEBUG` en een persoon op ~3 m is de actieve zone ≈ 4 |
| Framesnelheid | noteer frames/s (verwacht 10–20) |
| Apparaat in Home Assistant | alle entiteiten uit [contracts/ha-entities.md](contracts/ha-entities.md) zichtbaar; diagnose- en configuratie-entiteiten onder de juiste categorie |

## 5. Validatiescenario's

Gebruik `DEBUG` voor afstemmen en `INFO` voor de duurtest. Leg per scenario het resultaat vast
(datum, weer/wind, aantal pogingen, resultaat). De geschiedenis in Home Assistant is daarbij
een handige bron.

| # | Scenario | Verwacht | Spec |
|---|----------|----------|------|
| V1 | Loop 40× in normaal tempo over het pad (met bocht) naar de deur | ≥ 38× "Detectie" = `naderend`, binnen 2 s na binnenkomst | US1, SC-001, SC-002 |
| V2 | Loop door naar de deur na detectie | precies één "Detectie"-event per nadering | FR-008 |
| V3 | Loop 20× van de deur weg: 10× vanuit een leeg gebied (bijv. het huis uit) en 10× als omkeer na een nadering | ≥ 19× `verwijderend` (bij omkeer als `direction_changed`, zonder nieuwe `person_detected`) | US3, SC-004, FR-008 |
| V4 | Blijf 30 s stilstaan bij de deur na detectie | "Persoon aanwezig" blijft aan; gaat uiterlijk 60 s + wegtijd na laatste beweging uit | FR-009 |
| V5 | Loop weg uit het gebied | "Persoon aanwezig" ~5 s na verlaten uit | FR-009 |
| V6 | Winderige dag, niemand op het pad, 24 h | ≤ 1 "Detectie"-event | US2, SC-003 |
| V7 | Winderig, loop 10× naar de deur | ≥ 9× gedetecteerd | US2-3 |
| V8 | Onderbreek de sensor (bijv. module even loskoppelen) en herstel | binnen 3 s "Radar storing" aan en "Persoon aanwezig" onbekend; binnen 10 s na herstel weer werkend | FR-013, SC-006 |
| V9 | Zet "Maximale afstand" op 4 m en loop op 5 m | geen detectie; afwijzingsreden `out_of_range`; waarde blijft na herstart 4 m | US4, FR-011 |
| V10 | Zet "Maximale snelheid" op 0,2 m/s (lager dan minimale snelheid) | waarde springt terug; `config_rejected` in de log | FR-018 |
| V11 | Schakel WiFi/router uit tijdens een nadering, daarna weer aan | detectie loopt door (log via USB); na herstel klopt "Persoon aanwezig" direct; apparaat herstart niet | FR-016 |
| V12 | Detectie tijdens normale WiFi | event zichtbaar in Home Assistant binnen 1 s | SC-007 |
| V13 | 7 dagen continu draaien | geen herstart nodig; geen onverwachte reset-oorzaak | SC-005 |
| V14 | Haal de voeding 10 s weg terwijl "Persoon aanwezig" aan is, en sluit weer aan | na opstarten is "Persoon aanwezig" uit; de in V9 gewijzigde drempel is behouden; geen "Detectie" binnen de opstartperiode (5 s); reset-oorzaak toont een herstart na stroomuitval | constitutie poort 3, FR-011, FR-014 |

Let op: een wijziging van een `number` wordt binnen ongeveer een minuut naar flash geschreven;
wacht dat af voordat je bij V9 de stroom onderbreekt.

## 6. Afronden

Klaar volgens constitutie v2.0.0 als: `esphome config` en `esphome compile` zonder nieuwe
waarschuwingen, `pio test -e native` (met `.venv/Scripts/pio`) groen, rooktest V1, V6 (verkort tot 1 nacht), V8, V11, V12
en V14 op echte hardware vastgelegd, en RAM/flashgebruik genoteerd.
