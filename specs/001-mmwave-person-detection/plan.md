# Implementation Plan: Persoonsdetectie met mmWave-radar

**Branch**: `001-mmwave-person-detection` (werk op `main`) | **Date**: 2026-11-24 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/001-mmwave-person-detection/spec.md`

## Summary

Een ESPHome-apparaat (Seeed XIAO ESP32-S3 met gestapelde LD2410B-mmWave-radar) dat personen
detecteert die over het schuine pad naar de voordeur lopen, bewegende begroeiing negeert, en de
resultaten aan Home Assistant doorgeeft. Een eigen external component `door_radar` leest de
radar via UART, zet hem in engineering-modus en voert de detectie uit op de energie per
afstandszone: een adaptief achtergrondmodel per zone onderdrukt struiken, het
energie-zwaartepunt geeft de positie en lineaire regressie over 1,5 s levert snelheid, richting
en netto verplaatsing. Een toestandsmachine levert aanwezigheid, richting en een detectie-event
aan Home Assistant, plus diagnose-entiteiten en 18 instelbare, bewaarde drempels. De
detectielogica is framework-vrij en wordt op de host met Unity getest. Details:
[research.md](research.md).

## Technical Context

**Language/Version**: ESPHome-YAML; C++17 voor de external component; Python voor de
codegeneratie van de component

**Primary Dependencies**: ESPHome (exact vastgepind in `requirements.txt` bij de opzet, research
R2), ESP-IDF-framework (versie vast in de YAML); ESPHome-componenten `uart`, `api`, `ota`,
`wifi`, `captive_portal`, `logger`, `debug`; PlatformIO `native@1.2.1` alleen voor host-tests.
Geen externe bibliotheken; de ingebouwde `ld2410`-component wordt bewust niet gebruikt
(research R3).

**Storage**: ESPHome-preferences (flash) voor de 18 `number`-drempels (`restore_value`)

**Testing**: Unity via `pio test -e native` (pure logica); `esphome config`/`compile`;
handmatige hardware- en Home Assistant-tests volgens [quickstart.md](quickstart.md)

**Target Platform**: Seeed XIAO ESP32-S3 (`seeed_xiao_esp32s3`) + Seeed 24GHz mmWave for XIAO
(HLK-LD2410B), UART 256000 baud op GPIO43/GPIO44 (XIAO D6/D7); Home Assistant via native API

**Project Type**: embedded firmware als ESPHome-apparaat met één external component

**Performance Goals**: detectie binnen 2 s na binnenkomst (SC-002), zichtbaar in Home Assistant
binnen 1 s (SC-007); 10–20 frames/s verwerken; `loop()` van de component < 30 ms (constitutie
I), in de praktijk < 1 ms per frame

**Constraints**: geen heap in herhaalde paden; vaste buffers (ringbuffer 64, event-wachtrij 8,
UART-RX-buffer 1024); diagnose-entiteiten ≤ 1×/s; geen herstart bij onbereikbaar Home Assistant;
watchdog actief; buitenmontage in 24 GHz-doorlatende behuizing

**Scale/Scope**: één apparaat, één detectiegebied (0,75–5 m), ~1 persoon tegelijk; ~20
diagnose-entiteiten en 18 configuratie-entiteiten

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.* (constitutie v2.0.0)

| Principe / regel | Toetsing | Status |
|------------------|----------|--------|
| I. Niet-blokkerend | `loop()` leest UART-bytes begrensd (max. 256 per aanroep), voedt parser en detector, en publiceert events; configuratiereeks is een state machine met ACK-timeouts; diagnose via `set_interval`; geen `delay()` | ✅ |
| II. Gescheiden en testbare logica | Parser, commando's, achtergrondmodel, spoor en detector zonder ESPHome/IDF-headers; tijd geïnjecteerd; `pio test -e native` met Unity; pinnen als `substitutions` | ✅ |
| III. Betrouwbaarheid | Geen uitgangen in deze feature; status start als "geen persoon", sensorfout → "onbekend" + "Radar storing"; herinitialisatie met backoff; `api.reboot_timeout: 0s`; geen heap in herhaalde paden; watchdog actief; reset-oorzaak via `debug` | ✅ |
| IV. Geheimen en configuratie | `secrets.yaml` (git-ignored) + `secrets.yaml.example`; API-encryptie; OTA-wachtwoord; drempels als YAML-waarden of `number`-entiteiten met opslag | ✅ |
| V. Eenvoud en resources | Eén external component; **afwijking van "standaardcomponent eerst"** gerechtvaardigd in research R3 (ingebouwde `ld2410` levert geen frame-consistente zone-energie op volle snelheid); ESPHome-versie vastgepind; logging via ESPHome-logger; diagnose gethrottled | ✅ (gerechtvaardigd) |
| Build | `esphome compile voordeur-radar.yaml`; YAML is enige bron; framework- en platformversie vast; component in `components/door_radar/`; `.esphome/`, `.pio/`, `secrets.yaml` in `.gitignore` | ✅ |
| Kwaliteitspoorten | `esphome config`/`compile`, `pio test -e native`, rooktest incl. Home Assistant en WiFi-uitval, RAM/flash: [quickstart.md](quickstart.md) §6 | ✅ |

**Na Phase 1 opnieuw getoetst**: data-model en contracten voegen geen afhankelijkheden,
heap-gebruik of blokkerende stappen toe. Het enige punt van aandacht, de eigen radarkoppeling in
plaats van de ingebouwde `ld2410`, is in research R3 onderbouwd en vereist geen Complexity
Tracking-uitzondering (de constitutie staat eigen componenten toe mits gerechtvaardigd).

**Afwijking van de spec (geen constitutiekwestie)**: FR-013 vraagt "niet beschikbaar"; ESPHome
kan per entiteit alleen "onbekend" tonen (research R8). Aanbevolen: FR-013 hierop aanpassen.

## Project Structure

### Documentation (this feature)

```text
specs/001-mmwave-person-detection/
├── plan.md              # Dit bestand
├── research.md          # Phase 0
├── data-model.md        # Phase 1
├── quickstart.md        # Phase 1
├── contracts/
│   ├── ha-entities.md
│   ├── esphome-config.md
│   ├── presence-detector-api.md
│   ├── ld2410-uart-protocol.md
│   └── diagnostic-log.md
└── tasks.md             # Phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
voordeur-radar.yaml                  # apparaat-YAML: enige bron voor board, pinnen, componenten
secrets.yaml.example                 # voorbeeld; secrets.yaml zelf is git-ignored
requirements.txt                     # esphome==<vastgepinde versie>
platformio.ini                       # alleen [env:native] voor host-tests
.gitignore                           # .esphome/, .pio/, secrets.yaml, .venv/
components/
└── door_radar/
    ├── __init__.py                  # CONFIG_SCHEMA component + codegen
    ├── binary_sensor.py             # presence, sensor_fault
    ├── sensor.py                    # position, speed, gate_energy[9]
    ├── text_sensor.py               # direction, reject_reason
    ├── event.py                     # detection
    ├── number.py                    # 18 drempels
    ├── door_radar.h / .cpp          # ESPHome-lijm: UARTDevice + Component, publiceren
    ├── door_radar_number.h / .cpp   # number-entiteit met validatie via applyConfig
    ├── ld2410_types.h               # ── vanaf hier pure C++ (host-getest) ──
    ├── ld2410_frame_parser.h / .cpp
    ├── ld2410_commands.h / .cpp
    ├── detector_types.h             # DetectorConfig, validate, events, RejectReason
    ├── background_model.h / .cpp
    ├── track.h / .cpp
    └── presence_detector.h / .cpp
test/
├── test_frame_parser/test_main.cpp
├── test_commands/test_main.cpp
├── test_config_validation/test_main.cpp
├── test_background_model/test_main.cpp
├── test_track/test_main.cpp
└── test_presence_detector/test_main.cpp   # scenario's met synthetische frame-reeksen
```

**Structure Decision**: één ESPHome-apparaat met één lokale external component, conform de
projectindeling van constitutie v2.0.0. Pure logica en ESPHome-lijm staan in dezelfde
componentmap (ESPHome compileert alle bronbestanden van de map); de bestanden met prefix
`door_radar` zijn de enige met ESPHome-afhankelijkheden en worden in de `native`-omgeving
uitgesloten (research R10). Detectortests bouwen synthetische frame-reeksen op (bijv. een
persoon die met 0,8 m/s van 4,5 m naar 1 m loopt, een struik die op 2,25 m heen en weer
beweegt).

### platformio.ini (alleen host-tests)

```ini
[platformio]
src_dir = components/door_radar

[env:native]
platform = native@1.2.1
test_framework = unity
test_build_src = yes
build_src_filter = +<*> -<door_radar*>
build_flags = -std=gnu++17 -Wall -Wextra
```

## Complexity Tracking

Geen schendingen van constitutie v2.0.0; niets te rechtvaardigen buiten de onderbouwing in
research R3.
