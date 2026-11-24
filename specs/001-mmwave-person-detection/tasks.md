---

description: "Takenlijst voor de implementatie van persoonsdetectie met mmWave-radar (ESPHome)"
---

# Tasks: Persoonsdetectie met mmWave-radar

**Input**: Ontwerpdocumenten in `specs/001-mmwave-person-detection/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md),
[data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: verplicht volgens constitutie v2.0.0 principe II (nieuwe logica gaat vergezeld van
host-tests). Testtaken staan per user story vóór de implementatie; schrijf ze eerst en zie ze
falen.

**Organisatie**: taken zijn gegroepeerd per user story, zodat elke story los te bouwen en te
testen is.

## Format: `[ID] [P?] [Story] Beschrijving`

- **[P]**: kan parallel (andere bestanden, geen afhankelijkheid van onafgeronde taken)
- **[Story]**: bij welke user story de taak hoort (US1–US4)

## Padconventies

Volgens [plan.md](plan.md): apparaat-YAML in de root (`voordeur-radar.yaml`), external
component in `components/door_radar/`, host-tests in `test/<naam>/test_main.cpp`. Bestanden met
prefix `door_radar` en de `*.py`-bestanden zijn ESPHome-specifiek; alle andere C++-bestanden in
`components/door_radar/` zijn puur (geen ESPHome/IDF-headers) en worden met
`pio test -e native` getest.

---

## Phase 1: Setup (gedeelde infrastructuur)

**Doel**: projectopzet volgens de constitutie (build met ESPHome, tests met PlatformIO native).

- [X] T001 Maak `.gitignore` in de root met `.esphome/`, `.pio/`, `.venv/`, `secrets.yaml` en `__pycache__/`
- [X] T002 Maak `requirements.txt` met `esphome==<versie>`: bepaal de actuele stabiele ESPHome-release (`pip index versions esphome`), pin die exact en noteer de gekozen versie in `specs/001-mmwave-person-detection/research.md` onder R2
- [X] T003 [P] Maak `platformio.ini` met alleen `[platformio] src_dir = components/door_radar` en `[env:native]` exact zoals in [plan.md](plan.md) (`native@1.2.1`, Unity, `test_build_src = yes`, `build_src_filter = +<*> -<door_radar*>`, `-std=gnu++17 -Wall -Wextra`)
- [X] T004 [P] Maak `secrets.yaml.example` met de sleutels `wifi_ssid`, `wifi_password`, `ap_password`, `api_encryption_key`, `ota_password` en voorbeeldwaarden (research R12)
- [X] T005 Maak het skelet van de external component: `components/door_radar/__init__.py` met `DEPENDENCIES = ["uart"]`, `CONFIG_SCHEMA` (verplicht `uart_id`; optionele alleen-YAML waarden `reversal_hysteresis`, `evaluation_window`, `background_margin`, `background_time_constant`, `startup_time`, `sensor_timeout`, `ack_timeout` volgens [contracts/esphome-config.md](contracts/esphome-config.md)), validatie dat `uart.baud_rate` 256000 is, en codegen die de waarden aan de C++-klasse doorgeeft
- [X] T006 Maak `components/door_radar/door_radar.h` en `components/door_radar/door_radar.cpp` met klasse `DoorRadar : public Component, public uart::UARTDevice` (lege `setup()`, `loop()`, `dump_config()` met `TAG = "door_radar"` en setters voor de alleen-YAML waarden)
- [X] T007 Maak `voordeur-radar.yaml` met `substitutions` (`radar_rx_pin: GPIO43`, `radar_tx_pin: GPIO44`), `esphome:` (naam, `min_version` = vastgepinde versie), `esp32:` (`board: seeed_xiao_esp32s3`, `variant: esp32s3`, `framework: type: esp-idf` met expliciete `version` en `platform_version` die bij de vastgepinde ESPHome-versie horen), `logger:`, `api:` (`encryption.key: !secret api_encryption_key`, `reboot_timeout: 0s`), `ota: platform: esphome` (`password: !secret ota_password`), `wifi:` (`!secret`, `ap:` met `!secret ap_password`), `captive_portal:`, `debug:` (reset-oorzaak als `text_sensor`), `external_components` (lokaal `components`), `uart:` (256000 baud, `rx_buffer_size: 1024`, pinnen uit substitutions) en `door_radar:` volgens [contracts/esphome-config.md](contracts/esphome-config.md)
- [X] T008 Controleer de opzet: `pip install -r requirements.txt`, `esphome config voordeur-radar.yaml` is geldig en `esphome compile voordeur-radar.yaml` slaagt zonder waarschuwingen in `components/door_radar`

---

## Phase 2: Foundational (blokkerend voor alle stories)

**Doel**: gedeelde datatypes, het radarprotocol en een werkende UART-koppeling met de radar in
engineering-modus.

**⚠️ KRITIEK**: geen user story kan beginnen voordat deze fase klaar is.

- [X] T009 [P] Maak `components/door_radar/ld2410_types.h` met `RadarFrame` en de enums `FrameType` en `TargetState` (data-model.md `RadarFrame.type`, `RadarFrame.targetState`, `RadarFrame.movingDistanceCm`, `RadarFrame.movingEnergy`, `RadarFrame.stationaryDistanceCm`, `RadarFrame.stationaryEnergy`, `RadarFrame.detectionDistanceCm`, `RadarFrame.maxMovingGate`, `RadarFrame.maxStationaryGate`, `RadarFrame.movingGateEnergy`, `RadarFrame.stationaryGateEnergy`, `RadarFrame.timestampMs`) in namespace `ld2410`, zonder framework-headers
- [X] T010 [P] Maak `components/door_radar/detector_types.h` in namespace `presence` met `DetectorConfig` inclusief standaardwaarden (data-model.md `DetectorConfig.minRangeM`, `DetectorConfig.maxRangeM`, `DetectorConfig.minSpeedMps`, `DetectorConfig.maxSpeedMps`, `DetectorConfig.minDurationMs`, `DetectorConfig.minNetDisplacementM`, `DetectorConfig.maxPauseMs`, `DetectorConfig.absentTimeoutMs`, `DetectorConfig.maxHoldMs`, `DetectorConfig.minGateEnergy`, `DetectorConfig.reversalHysteresisM`, `DetectorConfig.evaluationWindowMs`, `DetectorConfig.backgroundMargin`, `DetectorConfig.backgroundTimeConstantMs`, `DetectorConfig.startupMs`, `DetectorConfig.sensorTimeoutMs`), `DetectionEvent` (data-model.md `DetectionEvent.kind`, `DetectionEvent.direction`, `DetectionEvent.distanceM`, `DetectionEvent.timestampMs`), `RejectReason` (data-model.md RejectReason), `PresenceStatus`, `ConfigError` en `Evaluation` (velden volgens [contracts/presence-detector-api.md](contracts/presence-detector-api.md))
- [X] T011 [P] Schrijf `test/test_frame_parser/test_main.cpp`: beide testvectoren uit [contracts/ld2410-uart-protocol.md](contracts/ld2410-uart-protocol.md) byte-voor-byte, frame verspreid over meerdere aanroepen, ruis vóór de header, fout in header/lengte/tail/frame-end, en afwijzing van ongeldige waarden voor data-model.md `RadarFrame.targetState` (> 0x03), `RadarFrame.movingEnergy` en `RadarFrame.stationaryEnergy` (> 100), `RadarFrame.maxMovingGate` en `RadarFrame.maxStationaryGate` (> 8); ACK-frames worden apart herkend
- [X] T012 [P] Schrijf `test/test_commands/test_main.cpp`: de drie commando's (0x00FF, 0x0062, 0x00FE) leveren exact de bytes uit [contracts/ld2410-uart-protocol.md](contracts/ld2410-uart-protocol.md); ACK-parsing herkent succes, falen en het commandowoord `| 0x0100`
- [X] T013 Implementeer `components/door_radar/ld2410_frame_parser.h` en `.cpp`: byte-voor-byte state machine zonder heap die rapportframes (`F4 F3 F2 F1 … F8 F7 F6 F5`) en ACK-frames (`FD FC FB FA … 04 03 02 01`) uit één stroom scheidt, basis- en engineering-frames decodeert (extra M bytes negeren) en ongeldige frames telt; T011 moet slagen
- [X] T014 Implementeer `components/door_radar/ld2410_commands.h` en `.cpp`: frames bouwen voor enable configuration, enable engineering mode en end configuration in een aangeleverde vaste buffer, plus ACK-beoordeling; T012 moet slagen
- [X] T015 Implementeer in `components/door_radar/door_radar.cpp` het lezen van de UART in `loop()`: maximaal 256 bytes per aanroep (constitutie I), bytes naar de parser, tijdstempel via `millis()`, tellers voor geldige en ongeldige frames, `W`-log voor ongeldige frames hoogstens 1× per 10 s ([contracts/diagnostic-log.md](contracts/diagnostic-log.md))
- [X] T016 Implementeer in `components/door_radar/door_radar.cpp` de niet-blokkerende initialisatiereeks (research R4): enable configuration → enable engineering mode → end configuration, elk met ACK-timeout (`ack_timeout`, standaard 500 ms) en max. 3 pogingen, daarna backoff 1, 2, 4 … max. 30 s; reeks opnieuw als er 3 s lang alleen basisframes binnenkomen; publieke methode `reinitRadar()`; log `engineering_mode ok attempts=N`
- [ ] T017 Bring-up op hardware volgens [quickstart.md](quickstart.md) §4: flash, controleer `engineering_mode ok`, wissel bij `sensor_timeout` `radar_rx_pin`/`radar_tx_pin` in `voordeur-radar.yaml`, noteer framesnelheid en zone-resolutie in `specs/001-mmwave-person-detection/research.md` onder "Openstaande punten"

**Checkpoint**: de radar levert engineering-frames die correct geparsed worden; user stories
kunnen beginnen.

---

## Phase 3: User Story 1 - Naderende persoon wordt betrouwbaar gedetecteerd (Priority: P1) 🎯 MVP

**Goal**: een persoon die over het pad naar de deur loopt, levert binnen 2 s één "Detectie"
(naderend) en "Persoon aanwezig" = aan; stilstaan houdt de status begrensd vast; weglopen geeft
na de wegtijd "uit"; een sensorfout is zichtbaar.

**Independent Test**: loop 40× over het pad naar de deur (quickstart V1, V2, V4, V5) en trek de
sensor even los (V8); controleer de entiteiten in Home Assistant en de log.

### Tests voor User Story 1

- [X] T018 [P] [US1] Schrijf `test/test_track/test_main.cpp`: ringbuffer-wrap na 64 samples (data-model.md `Track.samples`, `Track.count`), regressiehelling bij constante snelheid (−0,8 m/s → naderend, data-model.md `Track.direction`), netto verplaatsing en duur over het venster `DetectorConfig.evaluationWindowMs`, pauze < 0,15 m/s tot `DetectorConfig.maxPauseMs` breekt het spoor niet af, langere pauze wel
- [X] T019 [P] [US1] Schrijf `test/test_presence_detector/test_main.cpp` met synthetische engineering-frames: (a) geen `PersonDetected` binnen `DetectorConfig.startupMs` (FR-014); (b) persoon van 4,5 m naar 1 m met 0,8 m/s → precies één `PersonDetected` met `Approaching` binnen 2 s na binnenkomst (FR-002–FR-004, FR-007, FR-008); (c) daarna geen beweging → `PersonGone` na `DetectorConfig.absentTimeoutMs`; (d) stilstaand doel binnen ±1 zone houdt `PersonPresent` vast, maar uiterlijk tot `DetectorConfig.maxHoldMs` na laatste beweging (FR-009); (e) stilstaand doel 3 zones verderop houdt niet vast; (f) geen frames gedurende `DetectorConfig.sensorTimeoutMs` → `SensorFault`-event, status `SensorFault`, geen `PersonGone`; (g) eerste frame daarna → `SensorRecovered` en nieuwe opstartperiode (FR-013); (h) persoon buiten `DetectorConfig.minRangeM`/`DetectorConfig.maxRangeM` → geen detectie, reden `OutOfRange` (FR-006); (i) snelheid buiten `DetectorConfig.minSpeedMps`/`DetectorConfig.maxSpeedMps` → reden `SpeedOutOfRange`; (j) te korte beweging → `TooShort`; (k) te weinig verplaatsing (`DetectorConfig.minNetDisplacementM`) → `NetDisplacementTooSmall`; (l) event-wachtrij van 8 loopt over zonder crash

### Implementatie voor User Story 1

- [X] T020 [US1] Implementeer `components/door_radar/track.h` en `.cpp`: vaste ringbuffer van 64 `{tMs, posM}` (data-model.md `Track.samples`, `Track.count`, `Track.startMs`, `Track.lastMotionMs`, `Track.lastPosM`, `Track.direction`), lineaire regressie, netto verplaatsing en duur over het venster, pauzetolerantie; T018 moet slagen
- [X] T021 [US1] Implementeer `components/door_radar/presence_detector.h` en `.cpp` volgens [contracts/presence-detector-api.md](contracts/presence-detector-api.md) (`onFrame`, `tick`, `pollEvent`, `status`, `lastEvaluation`, `config`): positie als energie-zwaartepunt van zones met energie > data-model.md `DetectorConfig.minGateEnergy` binnen het detectiegebied (research R5 laag 2), beoordeling van het spoor tegen FR-002–FR-004 en FR-006, toestandsmachine `Startup → Idle → Candidate → Present → Idle` en `Fault` (data-model.md Toestandsmachine), vasthouden via stilstaand doel binnen ±1 zone, event-wachtrij van 8, geen heap; T019 moet slagen
- [X] T022 [US1] Maak `components/door_radar/binary_sensor.py` met de sub-entiteiten `presence` (`device_class: presence`) en `sensor_fault` (`device_class: problem`, `entity_category: diagnostic`) volgens [contracts/esphome-config.md](contracts/esphome-config.md) en [contracts/ha-entities.md](contracts/ha-entities.md), plus setters in `components/door_radar/door_radar.h`
- [X] T023 [US1] Maak `components/door_radar/event.py` met sub-entiteit `detection` (`event_types: [naderend, verwijderend]`) en setter in `components/door_radar/door_radar.h`
- [X] T024 [US1] Koppel in `components/door_radar/door_radar.cpp` de detector aan ESPHome: `PresenceDetector` aanmaken met YAML-waarden en standaarddrempels in `setup()`, elk geldig frame naar `onFrame`, elke `loop()` `tick(millis())` en `pollEvent` leegmaken; publiceer "Persoon aanwezig" alleen bij verandering, trigger "Detectie" bij `PersonDetected`; bij `SensorFault` "Radar storing" aan, "Persoon aanwezig" ongeldig maken via `invalidate_state()` (controleer of dit in de vastgepinde ESPHome-versie bestaat; zo niet: uit publiceren, research R8) en `reinitRadar()` aanroepen; logs `person_detected`, `person_gone`, `sensor_timeout`, `sensor_recovered`, `ready` volgens [contracts/diagnostic-log.md](contracts/diagnostic-log.md)
- [X] T025 [US1] Voeg in `voordeur-radar.yaml` de entiteiten "Persoon aanwezig", "Radar storing" en "Detectie" toe onder `binary_sensor:`/`event:` met `platform: door_radar`; `esphome compile` zonder nieuwe waarschuwingen
- [ ] T026 [US1] Hardwarevalidatie: voer quickstart V1, V2, V4, V5, V8 en V12 uit en leg de resultaten vast in `specs/001-mmwave-person-detection/validation-log.md`

**Checkpoint**: MVP — aanwezigheid en naderingen zijn zichtbaar in Home Assistant.

---

## Phase 4: User Story 2 - Geen valse detecties door bewegende begroeiing (Priority: P1)

**Goal**: struiken en takken die in de wind bewegen, regen en kleine dieren leiden niet tot een
detectie, terwijl een persoon tussen bewegende begroeiing wél gedetecteerd wordt.

**Independent Test**: een winderige dag zonder personen (V6, ≤ 1 detectie per 24 h) en 10
naderingen bij wind (V7, ≥ 9 gedetecteerd).

### Tests voor User Story 2

- [X] T027 [P] [US2] Schrijf `test/test_background_model/test_main.cpp`: achtergrond per zone convergeert naar een constante energie met tijdconstante `DetectorConfig.backgroundTimeConstantMs` (data-model.md `Background.level`, `Background.lastUpdateMs`), wordt niet bijgewerkt als het model "bevroren" is, en een zone is pas actief boven `max(DetectorConfig.minGateEnergy[i], Background.level[i] + DetectorConfig.backgroundMargin)`
- [X] T028 [P] [US2] Schrijf `test/test_vegetation_rejection/test_main.cpp` met synthetische frames: (a) struik die 10 min op 2,25 m heen en weer beweegt → geen `PersonDetected`; (b) windstoot met snelle heen-en-weer beweging → geen detectie, reden `DirectionReversal` (hysterese `DetectorConfig.reversalHysteresisM`); (c) struik op 2,25 m + persoon van 4,5 m naar 1 m, ook als de struik de sterkste bewegingsenergie heeft → wél `PersonDetected` (US2-3); (d) willekeurige korte pieken in losse zones (regen) → geen detectie; (e) zwak signaal onder `DetectorConfig.minGateEnergy` → reden `SignalTooWeak` (FR-015); (f) achtergrond wordt niet bijgewerkt tijdens `Present`, zodat een aanwezige persoon niet "verdwijnt"; (g) persoon loopt dwars op ~4 m (de afstand verandert minder dan 0,3 m in 3 s) → geen detectie, reden `NetDisplacementTooSmall` (randgeval "Persoon loopt dwars", `DetectorConfig.minNetDisplacementM`)

### Implementatie voor User Story 2

- [X] T029 [US2] Implementeer `components/door_radar/background_model.h` en `.cpp`: exponentieel gemiddelde per zone (data-model.md `Background.level`, `Background.lastUpdateMs`), tijdgebaseerde update, actief-test per zone; T027 moet slagen
- [X] T030 [US2] Integreer het achtergrondmodel in `components/door_radar/presence_detector.cpp`: actieve zones volgens research R5 laag 1 (`DetectorConfig.minGateEnergy`, `DetectorConfig.backgroundMargin`), achtergrond bijwerken in `Startup` (snel), `Idle` en `Candidate` maar niet in `Present` (data-model.md Background), reden `SignalTooWeak` als alleen de minimale energie niet gehaald wordt
- [X] T031 [US2] Voeg richtingsomkering toe in `components/door_radar/track.cpp` en de afwijzing `DirectionReversal` in `components/door_radar/presence_detector.cpp` (omkering groter dan `DetectorConfig.reversalHysteresisM` breekt het spoor af, FR-002/FR-005); T028 en de bestaande tests T018/T019 moeten slagen
- [ ] T032 [US2] Hardwarevalidatie: voer quickstart V6 (minimaal één nacht voor de rooktest) en V7 uit en leg de resultaten vast in `specs/001-mmwave-person-detection/validation-log.md`

**Checkpoint**: US1 en US2 werken samen; de detectie is bruikbaar buiten.

---

## Phase 5: User Story 3 - Richting van de beweging wordt onderscheiden (Priority: P2)

**Goal**: aankomst en vertrek worden onderscheiden en als "Richting" en als event-type in Home
Assistant zichtbaar.

**Independent Test**: 20× van de deur weglopen (V3, ≥ 19× `verwijderend`) en de naderingen van
V1 tonen `naderend`.

### Tests voor User Story 3

- [X] T033 [P] [US3] Schrijf `test/test_direction/test_main.cpp`: persoon van 1 m naar 4,5 m → `PersonDetected` met data-model.md `DetectionEvent.direction` = `Receding`; nadering → `Approaching`; nadering met bocht (pauze binnen `DetectorConfig.maxPauseMs`) blijft `Approaching`; `DetectionEvent.distanceM` ligt binnen 0,5 m van de gesimuleerde positie

### Implementatie voor User Story 3

- [X] T034 [US3] Zorg dat `components/door_radar/presence_detector.cpp` bij `PersonDetected` de richting en afstand uit het spoor invult en ook verwijderende bewegingen accepteert; T033 moet slagen
- [X] T035 [US3] Maak `components/door_radar/text_sensor.py` met sub-entiteit `direction` en publiceer in `components/door_radar/door_radar.cpp` bij elke `PersonDetected` `naderend` of `verwijderend` op "Richting" en als event-type op "Detectie"
- [ ] T036 [US3] Voeg "Richting" toe aan `voordeur-radar.yaml`, voer quickstart V3 uit en leg het resultaat vast in `specs/001-mmwave-person-detection/validation-log.md`

**Checkpoint**: US1–US3 werken; automatiseringen kunnen op richting reageren.

---

## Phase 6: User Story 4 - Instellen en afstemmen op de locatie (Priority: P3)

**Goal**: alle drempels uit FR-011 zijn vanuit Home Assistant instelbaar, worden bewaard en
gevalideerd; diagnose-entiteiten en debuglogs tonen waarom bewegingen worden geaccepteerd of
afgewezen.

**Independent Test**: quickstart V9 (maximale afstand 4 m, ook na herstart) en V10 (ongeldige
maximale snelheid wordt geweigerd); diagnose-entiteiten tonen positie, snelheid, reden en
zone-energieën.

### Tests voor User Story 4

- [X] T037 [P] [US4] Schrijf `test/test_config_validation/test_main.cpp`: voor elk veld de grenzen uit data-model.md (`DetectorConfig.minRangeM`, `DetectorConfig.maxRangeM`, `DetectorConfig.minSpeedMps`, `DetectorConfig.maxSpeedMps`, `DetectorConfig.minDurationMs`, `DetectorConfig.minNetDisplacementM`, `DetectorConfig.maxPauseMs`, `DetectorConfig.absentTimeoutMs`, `DetectorConfig.maxHoldMs`, `DetectorConfig.minGateEnergy`, `DetectorConfig.reversalHysteresisM`, `DetectorConfig.evaluationWindowMs`, `DetectorConfig.backgroundMargin`, `DetectorConfig.backgroundTimeConstantMs`, `DetectorConfig.startupMs`, `DetectorConfig.sensorTimeoutMs`) net binnen en net buiten de grens, plus de onderlinge regels (min < max afstand en snelheid, `maxHoldMs ≥ absentTimeoutMs`, `evaluationWindowMs ≥ minDurationMs`); standaardconfiguratie is geldig
- [X] T038 [P] [US4] Schrijf `test/test_reconfigure/test_main.cpp`: `applyConfig` met ongeldige kandidaat → false, `ConfigError` gevuld, oude configuratie actief; geldige kandidaat tijdens `Candidate` → `Idle`; tijdens `Present` → blijft `Present` met nieuwe tijden; achtergrondmodel blijft behouden (contract-regel 6)

### Implementatie voor User Story 4

- [X] T039 [US4] Implementeer `presence::validate()` in een nieuw bestand `components/door_radar/detector_config.cpp` en `PresenceDetector::applyConfig()` in `components/door_radar/presence_detector.cpp` volgens [contracts/presence-detector-api.md](contracts/presence-detector-api.md); T037 en T038 moeten slagen
- [X] T040 [US4] Maak `components/door_radar/number.py` en `components/door_radar/door_radar_number.h`/`.cpp`: 18 `number`-entiteiten (9 algemene + `gate_min_energy` met precies 9 items) met bereiken, stappen en eenheden uit [contracts/ha-entities.md](contracts/ha-entities.md), `entity_category: config`, `restore_value`, `mode: box`; elke `DoorRadarNumber` bewaart zijn waarde zelf in een `ESPPreferenceObject` (sleutel op basis van de object-id), herstelt die in zijn eigen `setup()` (standaardprioriteit `DATA`) en geeft de herstelde waarde zonder validatie door aan `DoorRadar` (bijv. `set_restored_value(field, v)`); `control()` bouwt een kandidaat (eenheden omrekenen naar data-model.md-velden, bijv. s → ms), roept `applyConfig` aan en publiceert bij weigering de vorige waarde opnieuw met `W`-log `config_rejected` (FR-018)
- [X] T041 [US4] Laat `DoorRadar::get_setup_priority()` `setup_priority::LATE` teruggeven, zodat alle numbers eerst hun waarde hebben hersteld; bouw in `DoorRadar::setup()` de configuratie uit YAML-standaarden plus herstelde waarden en valideer die als geheel (data-model.md DetectorConfig, validatie bij opstarten); ongeldig → standaardwaarden toepassen, alle numbers met de standaardwaarde publiceren én hun opgeslagen waarde overschrijven, `E`-log in `components/door_radar/door_radar.cpp`
- [X] T042 [US4] Breid `components/door_radar/sensor.py` (nieuw: `position`, `speed`, `gate_energy` met precies 9 items) en `components/door_radar/text_sensor.py` (`reject_reason`) uit; publiceer ze in `components/door_radar/door_radar.cpp` via één `set_interval(1000)` en alleen bij verandering, met `entity_category: diagnostic`, eenheden en nauwkeurigheid uit [contracts/ha-entities.md](contracts/ha-entities.md); positie NaN als er geen actieve zones zijn (FR-017)
- [X] T043 [US4] Voeg in `components/door_radar/door_radar.cpp` de `D`-logregel `eval …` toe met de velden uit [contracts/diagnostic-log.md](contracts/diagnostic-log.md), begrensd op 10 regels per seconde (FR-012)
- [ ] T044 [US4] Voeg alle diagnose- en configuratie-entiteiten toe aan `voordeur-radar.yaml` (namen uit [contracts/ha-entities.md](contracts/ha-entities.md)), voer quickstart V9 en V10 uit en leg de resultaten vast in `specs/001-mmwave-person-detection/validation-log.md`

**Checkpoint**: alle user stories werken; de detectie is vanuit Home Assistant af te stemmen.

---

## Phase 7: Polish & cross-cutting

**Doel**: kwaliteitspoorten van de constitutie en de resterende validaties.

- [X] T045 [P] Maak `README.md` met een korte projectbeschrijving, hardware (XIAO ESP32-S3 + 24GHz mmWave for XIAO, pinnen D6/D7, D10 niet gebruiken), opzet en verwijzing naar `specs/001-mmwave-person-detection/quickstart.md`
- [X] T046 Controleer dat het in T024 gebouwde gedrag overeenkomt met FR-013 in `specs/001-mmwave-person-detection/spec.md`; als `invalidate_state()` in de vastgepinde ESPHome-versie niet bestaat (fallback "uit"), pas FR-013 daarop aan en vermeld dit in de Clarifications
- [ ] T047 Controleer dat de ESPHome-log tijdens normaal gebruik geen waarschuwing "took a long time" voor `door_radar` toont (constitutie I) en dat `esphome compile` geen nieuwe waarschuwingen geeft; noteer RAM- en flashgebruik in `specs/001-mmwave-person-detection/validation-log.md` (constitutie V)
- [ ] T048 Voer quickstart V11 (WiFi-uitval tijdens nadering, geen herstart) en V14 (stroomuitval) uit en leg de resultaten vast in `specs/001-mmwave-person-detection/validation-log.md`
- [ ] T049 Start de 7-daagse duurtest V13 (SC-003 en SC-005) en leg aantal valse detecties per dag en eventuele resets vast in `specs/001-mmwave-person-detection/validation-log.md`
- [ ] T050 Draai als eindcontrole `pio test -e native`, `esphome config voordeur-radar.yaml` en `esphome compile voordeur-radar.yaml` en loop de afrondingslijst van [quickstart.md](quickstart.md) §6 af

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: geen afhankelijkheden
- **Foundational (Phase 2)**: na Setup; blokkeert alle user stories
- **US1 (Phase 3)**: na Foundational
- **US2 (Phase 4)**: na US1 (werkt in `presence_detector.cpp` en `track.cpp` die in US1 ontstaan)
- **US3 (Phase 5)**: na US1; onafhankelijk van US2
- **US4 (Phase 6)**: na US1; `text_sensor.py` wordt na T035 (US3) uitgebreid, dus T042 na T035
- **Polish (Phase 7)**: na de gewenste stories; T049 kan pas als US1+US2 buiten hangen

### User Story Dependencies

- **US1 (P1)**: alleen Foundational
- **US2 (P1)**: bouwt voort op de detector van US1 (zelfde bestanden), apart testbaar via V6/V7
- **US3 (P2)**: bouwt voort op US1, apart testbaar via V3
- **US4 (P3)**: bouwt voort op US1; T042 na T035

### Within Each User Story

- Tests eerst schrijven en zien falen, dan implementeren
- Pure logica vóór ESPHome-lijm, lijm vóór YAML, YAML vóór hardwarevalidatie

### Parallel Opportunities

- Setup: T003 en T004 parallel na T001
- Foundational: T009, T010, T011 en T012 parallel; daarna T013 en T014 parallel (verschillende bestanden)
- US1: T018 en T019 parallel; T022 en T023 parallel na T021
- US2: T027 en T028 parallel
- US3 en US4: na US1 kunnen de tests T033, T037 en T038 parallel worden geschreven
- Polish: T045 parallel met de rest

---

## Parallel Example: User Story 1

```bash
# Tests voor User Story 1 tegelijk schrijven:
Task: "Schrijf test/test_track/test_main.cpp (T018)"
Task: "Schrijf test/test_presence_detector/test_main.cpp (T019)"

# Na T021 de ESPHome-platforms tegelijk:
Task: "Maak components/door_radar/binary_sensor.py (T022)"
Task: "Maak components/door_radar/event.py (T023)"
```

---

## Implementation Strategy

### MVP First (User Story 1)

1. Phase 1: Setup
2. Phase 2: Foundational (inclusief bring-up T017)
3. Phase 3: US1
4. **STOP en VALIDEER**: V1, V2, V4, V5, V8, V12; de sensor werkt binnen en buiten bij windstil weer

### Incrementele oplevering

1. Setup + Foundational → radar levert frames
2. US1 → aanwezigheid in Home Assistant (MVP)
3. US2 → betrouwbaar bij wind (vereist voor definitieve buitenmontage)
4. US3 → richting voor automatiseringen
5. US4 → afstemmen vanuit Home Assistant
6. Polish → duurtest en kwaliteitspoorten

---

## Notes

- [P] = andere bestanden, geen afhankelijkheden
- Commit na elke taak of logische groep (kleine, beschrijvende commits, constitutie)
- `secrets.yaml` en `.esphome/` nooit committen
- Bij elke checkpoint: `pio test -e native` en `esphome compile voordeur-radar.yaml` moeten slagen
