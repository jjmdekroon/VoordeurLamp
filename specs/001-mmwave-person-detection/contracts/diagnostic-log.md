# Contract: Log en diagnose-uitvoer

**Doel**: afstemmen op de locatie (US4, FR-012) en hardwaretests vastleggen. Uitvoer via de
ESPHome-logger (USB of `esphome logs <apparaat>.yaml` via WiFi). Alle regels van de component
hebben `TAG = door_radar`; velden als `sleutel=waarde`, zodat een log met een script te
analyseren is.

## Formaat

ESPHome voegt niveau en tag toe:

```text
[I][door_radar:###]: <bericht> [sleutel=waarde ...]
```

| Niveau | Wanneer |
|--------|---------|
| `E` | ongeldige herstelde configuratie (teruggevallen op standaard), sensor niet te initialiseren na alle pogingen |
| `W` | sensortimeout, ongeldige frames (geteld, max. 1 regel per 10 s), event-wachtrij overgelopen, geweigerde configuratiewijziging |
| `I` | engineering-modus actief, detectie klaar, detectiegebeurtenissen, sensor hersteld |
| `D` | per beoordeelde beweging de evaluatie (FR-012), max. 10 regels/s |

Het niveau stelt de gebruiker in via `logger: level:` (bijv. `INFO` voor normaal gebruik,
`DEBUG` voor afstemmen).

## Vaste berichten

```text
[I][door_radar]: engineering_mode ok attempts=1
[I][door_radar]: ready startup_ms=5000
[I][door_radar]: person_detected dir=naderend dist_m=3.60
[D][door_radar]: eval accepted=1 reason=none pos_m=3.60 speed_mps=-0.82 net_m=0.71 dur_ms=1100 gates=0b000011000
[D][door_radar]: eval accepted=0 reason=direction_reversal pos_m=2.20 speed_mps=0.10 net_m=0.05 dur_ms=900 gates=0b000000100
[I][door_radar]: direction_changed dir=verwijderend dist_m=2.10
[I][door_radar]: person_gone
[W][door_radar]: config_rejected field=max_speed value=0.20 reason=min_speed_ge_max_speed
[W][door_radar]: sensor_timeout no_frame_ms=3000
[I][door_radar]: sensor_recovered
```

Afwijzingsredenen (`reason=`, ook de waarden van de entiteit "Afwijzingsreden"): `none`,
`out_of_range`, `signal_too_weak`, `speed_out_of_range`, `too_short`,
`net_displacement_too_small`, `direction_reversal`, `startup`.

De reset-oorzaak verschijnt bij het opstarten via de ESPHome `debug`-component.
