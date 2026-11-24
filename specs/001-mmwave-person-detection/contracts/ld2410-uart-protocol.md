# Contract: LD2410B UART-protocol (gebruikte subset)

**Bron**: `Radar/24GHz_mmWave_for_XIAO-Usermanual-V1.00.pdf`, hoofdstuk 2. Alle waarden hex,
little-endian. UART: 256000 baud, 8N1.

## Rapportframes (radar → ESP32)

```text
F4 F3 F2 F1 | len (2) | type (1) | AA | doeldata | 55 | 00 | F8 F7 F6 F5
```

Doeldata basis (type `02`, 9 bytes):

| Veld | Bytes |
|------|-------|
| doelstatus (00 geen, 01 bewegend, 02 stilstaand, 03 beide) | 1 |
| afstand bewegend doel (cm) | 2 |
| energie bewegend doel | 1 |
| afstand stilstaand doel (cm) | 2 |
| energie stilstaand doel | 1 |
| detectieafstand (cm) | 2 |

Engineering (type `01`): basisdata, gevolgd door max. bewegingszone N (1), max. stilstandzone N
(1), bewegingsenergie zone 0..N (N+1), stilstandsenergie zone 0..N (N+1), en M extra bytes
(te negeren; M volgt uit `len`).

### Testvectoren (uit de handleiding)

| Soort | Bytes | Verwacht |
|-------|-------|----------|
| Basis | `F4 F3 F2 F1 0D 00 02 AA 02 51 00 00 00 00 3B 00 00 55 00 F8 F7 F6 F5` | status Stationary, bewegend 81 cm/0, stilstaand 0 cm/59, detectie 0 cm |
| Engineering | `F4 F3 F2 F1 23 00 01 AA 03 1E 00 3C 00 00 39 00 00 08 08 3C 22 05 03 03 04 03 06 05 00 00 39 10 13 06 06 08 04 03 05 55 00 F8 F7 F6 F5` | status Both, bewegend 30 cm/60, stilstaand 0 cm/57, N=8/8, bewegingsenergie `3C 22 05 03 03 04 03 06 05`, stilstandsenergie `00 00 39 10 13 06 06 08 04` |

## Commando's (ESP32 → radar)

```text
FD FC FB FA | len (2) | commandowoord (2) | waarde (N) | 04 03 02 01
```

ACK: zelfde omlijsting, commandowoord `| 0x0100`, gevolgd door status (2 bytes: 0 = ok).

| Stap | Commando | Bytes | Verwachte ACK |
|------|----------|-------|---------------|
| 1 | Configuratie inschakelen (0x00FF, waarde 0x0001) | `FD FC FB FA 04 00 FF 00 01 00 04 03 02 01` | `FD FC FB FA 08 00 FF 01 00 00 01 00 40 00 04 03 02 01` |
| 2 | Engineering-modus aan (0x0062) | `FD FC FB FA 02 00 62 00 04 03 02 01` | `FD FC FB FA 04 00 62 01 00 00 04 03 02 01` |
| 3 | Configuratie beëindigen (0x00FE) | `FD FC FB FA 02 00 FE 00 04 03 02 01` | `FD FC FB FA 04 00 FE 01 00 00 04 03 02 01` |

Regels:

- Elk commando wacht niet-blokkerend op de ACK, met timeout (standaard 500 ms) en maximaal
  3 pogingen. Lukt het niet, dan wordt het als sensorfout behandeld en volgt een nieuwe poging
  met backoff (1 s, 2 s, 4 s, max. 30 s).
- Tijdens de configuratie kunnen nog rapportframes binnenkomen; de parser MOET rapport- en
  ACK-frames uit dezelfde bytestroom kunnen scheiden (verschillende headers).
- Na stap 3 komen er engineering-frames binnen; komt er binnen 3 s alleen basisframes binnen,
  dan wordt de reeks herhaald.
