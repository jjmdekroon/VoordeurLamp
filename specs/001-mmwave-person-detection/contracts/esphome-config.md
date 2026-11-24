# Contract: ESPHome-configuratie van `door_radar`

**Doel**: de YAML-interface van de external component. Dit is het configuratieschema dat de
Python-codegeneratie (`components/door_radar/*.py`) MOET accepteren en valideren. Geen volledige
apparaat-YAML; die volgt in de implementatie.

## Component

```yaml
external_components:
  - source:
      type: local
      path: components

uart:
  id: radar_uart
  rx_pin: ${radar_rx_pin}        # standaard GPIO43 (XIAO D6), te verifiëren bij bring-up
  tx_pin: ${radar_tx_pin}        # standaard GPIO44 (XIAO D7)
  baud_rate: 256000
  rx_buffer_size: 1024

door_radar:
  id: radar
  uart_id: radar_uart
  # Alleen-YAML afstemwaarden (optioneel; standaard zoals in data-model)
  reversal_hysteresis: 0.3m
  evaluation_window: 1500ms
  background_margin: 15
  background_time_constant: 30s
  startup_time: 5s
  sensor_timeout: 3s
  ack_timeout: 500ms
```

Validatie in het schema: `uart_id` verplicht; tijden als ESPHome-tijdsduur; afstanden als
float met optionele eenheid `m`; bereiken zoals in [data-model.md](../data-model.md). De
component vereist bij het valideren dat `uart.baud_rate` 256000 is (anders configuratiefout).

## Sub-platforms

```yaml
binary_sensor:
  - platform: door_radar
    door_radar_id: radar
    presence:
      name: "Persoon aanwezig"
    sensor_fault:
      name: "Radar storing"

text_sensor:
  - platform: door_radar
    door_radar_id: radar
    direction:
      name: "Richting"
    reject_reason:
      name: "Afwijzingsreden"

event:
  - platform: door_radar
    door_radar_id: radar
    detection:
      name: "Detectie"

sensor:
  - platform: door_radar
    door_radar_id: radar
    position:
      name: "Positie"
    speed:
      name: "Snelheid"
    gate_energy:              # lijst van precies 9, index = zone
      - name: "Zone 0 energie"
      # … t/m Zone 8

number:
  - platform: door_radar
    door_radar_id: radar
    min_range:      { name: "Minimale afstand",      initial_value: 0.75 }
    max_range:      { name: "Maximale afstand",      initial_value: 5.0 }
    min_speed:      { name: "Minimale snelheid",     initial_value: 0.3 }
    max_speed:      { name: "Maximale snelheid",     initial_value: 2.5 }
    min_duration:   { name: "Minimale duur",         initial_value: 1.0 }
    min_displacement: { name: "Minimale verplaatsing", initial_value: 0.5 }
    max_pause:      { name: "Toegestane pauze",      initial_value: 1.0 }
    absent_timeout: { name: "Wegtijd",               initial_value: 5 }
    max_hold:       { name: "Maximale vasthoudtijd", initial_value: 60 }
    gate_min_energy:          # lijst van precies 9
      - { name: "Zone 0 minimale energie", initial_value: 20 }
      # … t/m Zone 8
```

Regels:

- Elk sub-item is optioneel; wat niet gedeclareerd is, wordt niet gepubliceerd. Een
  ontbrekende `number` betekent dat de standaardwaarde vast is.
- `entity_category`, `device_class`, eenheden, nauwkeurigheid, bereik en stap worden door de
  component als standaard gezet (zie [ha-entities.md](ha-entities.md)) en mogen in de YAML
  worden overschreven, behalve de bereiken.
- `gate_energy` en `gate_min_energy` MOETEN, als ze opgegeven zijn, precies 9 items bevatten.

## Overige apparaat-YAML (verplicht volgens constitutie)

`esphome:` (naam, `min_version`), `esp32:` (board, variant, framework met vaste versie),
`logger:`, `api:` (`encryption.key: !secret`, `reboot_timeout: 0s`), `ota:` (`password:
!secret`), `wifi:` (`!secret`, `ap:` fallback), `captive_portal:`, `debug:`; pinnen als
`substitutions`.
