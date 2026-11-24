# Research: Persoonsdetectie met mmWave-radar (ESPHome)

**Feature**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md) | **Datum**: 2026-11-24 |
**Constitutie**: v2.0.0

Bronnen: `Radar/24GHz_mmWave_for_XIAO-Datasheet-V1.00.pdf`,
`Radar/24GHz_mmWave_for_XIAO-Usermanual-V1.00.pdf`, `Radar/24GHz-mmWave-for-xiao-SCH.pdf`.

## R1. Microcontrollerbord

- **Decision**: Seeed XIAO ESP32-S3 (`esp32: board: seeed_xiao_esp32s3, variant: esp32s3`),
  bevestigd door de gebruiker. De sensor wordt direct op de XIAO-footprint gestapeld.
- **Rationale**: dit is het bord van de gebruiker; vrije UART voor 256000 baud, logging via
  native USB, ruim voldoende rekenkracht en RAM.
- **Pinnen**: de XIAO heeft 14 headerpinnen met bordnamen D0–D10; ESPHome gebruikt de
  chipnamen. De radar-UART is via de gestapelde header verbonden met D2 = GPIO3 en D3 = GPIO4
  (schema J1: pin 10 "TX" en pin 11 "RX" liggen op de XIAO-posities D2 en D3; D6 en D7 zijn op
  de radarmodule "NC", niet aangesloten). Als `substitutions` in de YAML (constitutie II). Er is
  geen extra bedrading nodig. *Correctie 2026-11-24*: eerder stond hier D6/D7; dat was een
  verkeerde lezing van het schema en gaf bij de bring-up `sensor_timeout` in beide richtingen.
- **Niet gebruiken**: de module verbindt zijn eigen aanwezigheidsuitgang (OUT) met D10 =
  GPIO9. Deze feature gebruikt die pin niet; D10 MAG niet als uitgang worden geconfigureerd
  (de module stuurt de lijn aan).

## R2. ESPHome-framework en versies

- **Decision**: ESP-IDF als onderliggend framework (`esp32: framework: type: esp-idf`). De
  ESPHome-versie wordt bij de opzet exact vastgepind in `requirements.txt` op de dan actuele
  stabiele release; de framework- en platformversie die bij die release horen, worden expliciet
  in de YAML gezet (`framework: version:` en `platform_version:`).
- **Rationale**: ESP-IDF is het door ESPHome aanbevolen framework voor ESP32-S3 en levert een
  kleinere, stabielere build. Eigen code gebruikt alleen ESPHome-API's (`uart`, `millis()`,
  `ESP_LOGx`, preferences), dus het framework is uitwisselbaar (constitutie: geen
  framework-specifieke API's).
- **Alternatives considered**: `arduino` (werkt ook, maar geen voordeel en grotere build).
- **Vastgelegd bij opzet (T002, 2026-11-24)**: `esphome==2026.10.2` (laatste patch van de vorige
  release; 2026.11.0 was net uit). Bijbehorende aanbevolen versies, expliciet in de YAML:
  ESP-IDF `5.5.6`, `platform_version: 55.3.312`. PlatformIO 6.2.0 komt mee met ESPHome en wordt
  voor de host-tests gebruikt via `.venv/Scripts/pio`.

## R3. Radarkoppeling: ingebouwde `ld2410`-component of eigen external component

- **Decision**: eigen external component `door_radar` die zelf `uart::UARTDevice` is en het
  LD2410-protocol parseert met een eigen, framework-vrije parser. De ingebouwde ESPHome
  `ld2410`-component wordt **niet** gebruikt.
- **Rationale** (rechtvaardiging conform constitutie V):
  - Het filter (R5) heeft per frame een consistente set van alle 9 zone-energieën plus
    tijdstempel nodig, op de volle framesnelheid (10–20 Hz). De ingebouwde component publiceert
    elke waarde als losse sensor (met `throttle`, standaard 1 s); daarmee gaat de
    frame-samenhang en de tijdsresolutie verloren.
  - Twee componenten kunnen niet tegelijk dezelfde UART lezen.
  - Een eigen parser zonder ESPHome-headers is host-testbaar (constitutie II); de interne API
    van de ingebouwde component is dat niet en kan per ESPHome-versie wijzigen.
- **Alternatives considered**: ingebouwde `ld2410` + template-sensoren/lambda's (verliest
  frame-samenhang, afgewezen); `ld2410` met `throttle: 0ms` en callbacks per sensor (9+ losse
  callbacks per frame zonder garantie op volgorde, afgewezen).
- **Gebruikte protocolsubset**: zie [contracts/ld2410-uart-protocol.md](contracts/ld2410-uart-protocol.md).
  Testvectoren: de voorbeeldframes uit de user manual §2.3.2.

## R4. UART-instellingen en engineering-modus

- **Decision**: `uart:` op 256000 baud, 8N1, `rx_pin`/`tx_pin` via substitutions (standaard
  `rx_pin: GPIO3`, `tx_pin: GPIO4`), `rx_buffer_size: 1024`. Na opstarten en na elk herstel
  van een sensorfout stuurt de component *enable configuration* (0x00FF) → *enable engineering
  mode* (0x0062) → *end configuration* (0x00FE), elk met ACK-timeout 500 ms, max. 3 pogingen,
  daarna backoff (1, 2, 4 … max. 30 s).
- **Rationale**: het schema legt de module-TX (net `TXC`) op headerpositie D2; de ESP moet dus
  op D2 (GPIO3) ontvangen. GPIO3 is een strapping-pin, maar alleen relevant als de eFuse
  `STRAP_JTAG_SEL` is gezet (standaard niet); de ESPHome-waarschuwing is daarom uitgezet. De
  richting blijft een substitution die bij bring-up wordt gecontroleerd. Engineering-modus gaat verloren bij
  stroomverlies (user manual §2.2.5) en moet dus steeds opnieuw aan. De grotere RX-buffer
  voorkomt verlies van bytes als de hoofdlus even bezet is (WiFi, API).
- **Alternatives considered**: baudrate van de module verlagen (onnodig risico).

## R5. Detectie-algoritme (ongewijzigd t.o.v. eerdere versie)

- **Decision**: drie lagen in pure C++:
  1. **Achtergrondmodel per zone**: exponentieel gemiddelde van de bewegingsenergie per zone
     (tijdconstante 30 s), alleen bijgewerkt zonder actief spoor. Zone "actief" als
     `energie > max(min_energie[zone], achtergrond[zone] + marge)` (marge 15). Onderdrukt
     struiken die blijvend in dezelfde zone bewegen (FR-005) en zwakke signalen (FR-015).
  2. **Positie**: energie-zwaartepunt van actieve zones binnen het detectiegebied
     (`Σ i·e_i / Σ e_i × 0,75 m`); werkt ook als de sensor zelf het sterkste doel bij de struik
     legt (US2-3).
  3. **Spoor**: ringbuffer (64 samples) met lineaire regressie over 1,5 s → radiale snelheid
     (FR-003), netto verplaatsing (FR-004), duur (FR-002), omkeringen met hysterese 0,3 m
     (FR-005); pauze < 0,15 m/s tot `maxPause` breekt het spoor niet af (FR-002).
- **Rationale**: geen snelheidsmeting en één doel per frame; regressie middelt de
  0,75 m-zones uit; clutter map is de standaard radartechniek tegen stationaire clutter.
- **Haalbaarheid SC-002**: bij 0,3 m/s is 0,5 m netto na ~1,7 s bereikt, bij normaal lopen na
  ~1 s.
- **Verfijningen tijdens de implementatie** (gevonden met de host-tests in
  `test/test_vegetation_rejection`; daar zijn de scenario's reproduceerbaar):
  1. *Drempel met spreiding*: het achtergrondmodel leert per zone ook de gemiddelde absolute
     afwijking; drempel = gemiddelde + max(marge, 3 × afwijking). Een slingerende struik gaf met
     alleen een vaste marge van 15 nog valse detecties.
  2. *Positie uit actieve zones plus buurzones*, gewogen met de energie boven het gemiddelde.
     Alleen de actieve zones gebruiken liet de positie naar zonemiddens "springen", waardoor een
     struik van 0,6 m slingerbreedte 1,3 m leek te bewegen.
  3. *Achtergrond leert ook in `Candidate`* (niet in `Present`) en *snel tijdens `Startup`*
     (tijdconstante = opstartperiode / 4); anders werd begroeiing die steeds een kandidaat start
     nooit geleerd, en gaf de eerste minuut na opstarten valse detecties.
  4. *Dichtheidseis*: minstens 8 samples en minstens 4 samples/s in een spoor. Losse korte pieken
     (regen) gaven anders een spoor van 3 punten dat als persoon telde.
- **Alternatives considered**: alleen gerapporteerde bewegingsafstand (springt naar struik),
  Kalman-filter (complexer, geen meetbaar voordeel bij deze resolutie), statische zonemaskers
  (maskeren ook personen).

## R6. Toestandsmachine, vasthouden en sensorfout

- **Decision**: `Startup → Idle → Candidate → Present → Idle`, `Fault` vanuit elke toestand
  (zie [data-model.md](data-model.md)). Vasthouden in `Present` via stilstaand doel binnen ±1
  zone van de laatste positie, tot `maxHold` (FR-009). Fout na 3 s zonder geldig frame
  (FR-013).
- **Alternatives considered**: "no-one duration" van de sensor (kent geen onderscheid
  persoon/struik).

## R7. Home Assistant-entiteiten

- **Decision**: de component levert eigen sub-platforms (`binary_sensor`, `sensor`,
  `text_sensor`, `event`, `number`) zodat alle entiteiten in de YAML onder `door_radar`
  gedeclareerd worden. Volledige lijst: [contracts/ha-entities.md](contracts/ha-entities.md).
  - Aanwezigheid, richting en het detectie-event worden direct bij verandering gepubliceerd
    (SC-007).
  - Diagnose-entiteiten (`entity_category: diagnostic`) worden via één `set_interval(1000)`
    gepubliceerd, en alleen als de waarde veranderd is (FR-017, constitutie V).
  - "Persoon weg" is zichtbaar als het uitgaan van "Persoon aanwezig"; het event-entiteit
    "Detectie" heeft de typen `naderend` en `verwijderend`.
- **Rationale**: dit is het gangbare patroon voor ESPHome external components (zoals de
  ingebouwde `ld2410`) en houdt de YAML overzichtelijk.
- **Alternatives considered**: template-entiteiten met lambda's in de YAML (logica verspreid
  over YAML en C++, lastiger te testen).

## R8. "Niet beschikbaar" bij sensorfout (FR-013)

- **Decision**: bij `Fault` wordt de toestand van "Persoon aanwezig" ongeldig gemaakt
  (`invalidate_state()`, waardoor ESPHome een ontbrekende toestand naar Home Assistant stuurt)
  en gaat "Radar storing" aan. Home Assistant toont "Persoon aanwezig" dan als **Onbekend**.
- **Rationale**: de ESPHome native API kent geen beschikbaarheid per entiteit; "niet
  beschikbaar" geldt alleen voor het hele apparaat als de verbinding wegvalt. Een ontbrekende
  toestand ("onbekend") heeft voor automatiseringen hetzelfde effect: een trigger op "aan" gaat
  niet af. Te verifiëren in de vastgepinde ESPHome-versie; als `invalidate_state()` daar niet
  bestaat, wordt "Persoon aanwezig" uit en is de fout alleen via "Radar storing" zichtbaar.
- **Gevolg voor de spec**: FR-013 noemt "niet beschikbaar"; in Home Assistant wordt dat
  "onbekend". Aanbevolen: de spec hierop aanpassen (via `/speckit-clarify` of handmatig).

## R9. Instelbare drempels en opslag (FR-011, FR-018)

- **Decision**: de 18 drempels uit FR-011 (9 algemene + 9 minimale zone-energieën) zijn
  `number`-entiteiten (`entity_category: config`) met opslag via ESPHome-preferences
  (`restore_value`). Overige afstemwaarden (venster, hysterese, achtergrondmarge en
  -tijdconstante, opstartperiode, sensortimeout, ACK-timeout) staan alleen in de YAML.
  - Bij een wijziging stelt de component een kandidaat-configuratie samen en laat de pure
    detector die valideren. Ongeldig → vorige waarde opnieuw publiceren + `WARN`-log (FR-018).
  - Bij het opstarten worden de herstelde waarden als geheel gevalideerd; ongeldig → YAML-
    standaardwaarden + `ERROR`-log.
  - ESPHome schrijft preferences standaard na hoogstens 1 minuut naar flash
    (`flash_write_interval`); een wijziging vlak voor stroomuitval kan dus verloren gaan.
    Dat is acceptabel en wordt in de quickstart vermeld.
- **Rationale**: voldoet aan FR-011 en constitutie IV; validatie in de pure laag is testbaar.
- **Alternatives considered**: template `number`s met lambda's (validatie verspreid), alles in
  Home Assistant zonder YAML-standaarden (geen bruikbare start na flashen).

## R10. Host-tests naast ESPHome

- **Decision**: `platformio.ini` bevat alleen een `native`-omgeving (`platform = native@1.2.1`,
  `test_framework = unity`) met `src_dir = components/door_radar` en een `build_src_filter`
  die de ESPHome-afhankelijke bestanden (`door_radar*.cpp`) uitsluit. Pure bestanden en
  ESPHome-lijm staan in dezelfde componentmap (ESPHome compileert alle bronbestanden van de
  map), onderscheiden via de naam.
- **Rationale**: constitutie II (`pio test -e native`); PlatformIO is al geïnstalleerd als
  afhankelijkheid van ESPHome, dus geen extra gereedschap.
- **Alternatives considered**: submap voor pure code (kan wel; de ingebouwde `ld2410` heeft een
  `number/`-submap, maar een platte map houdt de native `build_src_filter` eenvoudig), CMake +
  losse testframework (extra gereedschap).
- **Uitvoering**: de globale `pio` op de ontwikkel-pc bleek kapot; de tests draaien met de
  PlatformIO uit de virtuele omgeving (`.venv/Scripts/pio`). Testprogramma's worden statisch
  gelinkt (`test/support/static_link.py`), omdat Windows anders soms een verkeerde
  `libstdc++`-DLL uit het PATH laadt (foutcode 0xC0000139).

## R11. Logging en diagnose

- **Decision**: ESPHome-logger met `TAG = "door_radar"`; `INFO` voor opstarten en
  detectiegebeurtenissen, `WARN` voor sensorfouten en geweigerde configuratie, `DEBUG` (max.
  10/s) voor per-evaluatie diagnose (FR-012). Formaat: [contracts/diagnostic-log.md](contracts/diagnostic-log.md).
  De `debug`-component toont de reset-oorzaak (constitutie III).

## R12. Netwerk en beveiliging

- **Decision**: `wifi` met `!secret`, fallback-hotspot (`ap:`) + `captive_portal`; `api` met
  `encryption: key: !secret` en `reboot_timeout: 0s`; `ota: platform: esphome` met
  `password: !secret`. Geheimen in `secrets.yaml` (git-ignored), voorbeeld in
  `secrets.yaml.example`.
- **Rationale**: constitutie III en IV; `reboot_timeout: 0s` voorkomt herstarts als Home
  Assistant onbereikbaar is, zodat de detectie doorloopt (FR-016).

## Openstaande punten

Geen NEEDS CLARIFICATION. Te verifiëren tijdens implementatie/bring-up (geen ontwerpkeuze):
exacte ESPHome-versie bij opzet (R2), `invalidate_state()` in die versie (R8), UART-richting
D2/D3 (R4), zone-resolutie 0,75 m en werkelijke framesnelheid.
