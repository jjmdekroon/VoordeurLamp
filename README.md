# VoordeurLamp: persoonsdetectie bij de voordeur

ESPHome-apparaat voor Home Assistant dat met een 24 GHz mmWave-radar personen detecteert die naar
de voordeur lopen (of ervan weg), en bewegende begroeiing in de wind negeert.

- **Specificatie, plan en taken**: [`specs/001-mmwave-person-detection/`](specs/001-mmwave-person-detection/)
- **Testen en in gebruik nemen**: [`quickstart.md`](specs/001-mmwave-person-detection/quickstart.md)
- **Projectregels**: [`.specify/memory/constitution.md`](.specify/memory/constitution.md)

## Hardware

| Onderdeel | Type |
|-----------|------|
| Microcontroller | Seeed Studio XIAO ESP32-S3 |
| Radar | Seeed Studio 24GHz mmWave for XIAO (HLK-LD2410B), gestapeld via de header |

De radar praat via UART (256000 baud) over XIAO **D2 (GPIO3, ontvangen)** en **D3 (GPIO4, zenden)**; er is geen extra
bedrading nodig. Krijg je bij het opstarten `sensor_timeout` in de log, wissel dan `radar_rx_pin` en
`radar_tx_pin` om in de `substitutions` van [`voordeur-radar.yaml`](voordeur-radar.yaml).
XIAO **D10 (GPIO9)** is de OUT-pin van de radar: niet als uitgang gebruiken.

Buiten montage: behuizing zonder metaal die 24 GHz goed doorlaat (zie datasheet §8 in `Radar/`),
stevig en trillingsvrij bevestigd.

## Snel starten

```bash
python -m venv .venv
.venv/Scripts/pip install -r requirements.txt      # Linux/macOS: .venv/bin/pip
cp secrets.yaml.example secrets.yaml               # en invullen
.venv/Scripts/pio test -e native                   # host-tests van de detectielogica
.venv/Scripts/esphome run voordeur-radar.yaml      # bouwen en flashen (eerste keer via USB)
```

## Opbouw

| Pad | Inhoud |
|-----|--------|
| `voordeur-radar.yaml` | Apparaatconfiguratie (enige bron voor board, pinnen en entiteiten) |
| `components/door_radar/` | External component: `door_radar*` en `*.py` zijn ESPHome-lijm, de rest is framework-vrije logica |
| `test/` | Unity-tests voor de framework-vrije logica (`pio test -e native`) |
| `platformio.ini` | Alleen de `native`-testomgeving |

## Home Assistant

Het apparaat levert onder meer **Persoon aanwezig**, **Richting**, het event **Detectie**
(`naderend`/`verwijderend`), diagnose-entiteiten (positie, snelheid, afwijzingsreden, energie per
zone) en 18 instelbare drempels. Volledige lijst: [`contracts/ha-entities.md`](specs/001-mmwave-person-detection/contracts/ha-entities.md).
