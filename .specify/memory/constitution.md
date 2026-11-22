# VoordeurLamp Constitution

Embedded software voor de ESP32, ontwikkeld met het Arduino-framework.

## Core Principles

### I. Niet-blokkerende uitvoering

- `loop()` MOET binnen 50 ms terugkeren; blokkerende aanroepen (`delay()` langer dan
  enkele milliseconden, busy-wait lussen, synchrone netwerkcalls zonder timeout) zijn
  verboden in de hoofdlus.
- Tijdgestuurd gedrag MOET worden gerealiseerd met `millis()`-gebaseerde timers en
  expliciete toestandsmachines (state machines), of met FreeRTOS-taken/timers.
- Elke externe operatie (WiFi, netwerk, sensoren) MOET een timeout hebben.
- Code die vanuit een ISR draait MOET minimaal zijn (vlag zetten, `IRAM_ATTR`), en gedeelde
  variabelen MOETEN `volatile` of atomair beschermd zijn.

**Rationale**: een blokkerende hoofdlus maakt het apparaat onresponsief, kan de watchdog
laten afgaan en maakt gelijktijdig gedrag (knop, lamp, netwerk) onvoorspelbaar.

### II. Gescheiden en testbare logica

- Beslislogica (bijv. wanneer de lamp aan of uit gaat) MOET gescheiden zijn van
  hardwaretoegang (GPIO, PWM, WiFi, tijdsbron) via kleine interfaces of injecteerbare
  functies.
- Pure logica MOET zonder hardware testbaar zijn met unit tests op de host, via een
  PlatformIO `native`-omgeving met Unity, uit te voeren met `pio test -e native`.
- Pindefinities en hardware-specifieke constanten MOETEN op één centrale plek staan
  (bijv. `config.h` / `pins.h`); magische getallen in de code zijn niet toegestaan.
- Nieuwe logica MOET vergezeld gaan van tests; bij bugfixes MOET eerst een falende test
  het probleem aantonen waar dat zonder hardware mogelijk is.

**Rationale**: testen op het apparaat is traag en foutgevoelig; host-tests geven snelle,
herhaalbare feedback en houden de logica onafhankelijk van specifieke hardware.

### III. Betrouwbaarheid en veilige toestand

- Bij opstarten, na een reset en bij elke foutsituatie MOET de hardware (lamp, relais,
  uitgangen) in een gedefinieerde, veilige toestand staan.
- Uitval van WiFi, tijdsynchronisatie (NTP) of andere externe diensten MAG de
  kernfunctie niet blokkeren; het apparaat MOET autonoom blijven werken en zelf opnieuw
  verbinden met backoff.
- De task watchdog MOET actief blijven; deze uitschakelen vereist een gedocumenteerde
  rechtvaardiging.
- Dynamische geheugenallocatie (`new`, `malloc`, groeiende Arduino `String`-objecten) MAG
  NIET in de hoofdlus of in herhaald uitgevoerde paden plaatsvinden; gebruik vaste
  buffers of allocatie tijdens `setup()`.
- De reset-oorzaak (`esp_reset_reason()`) MOET bij opstarten gelogd worden.

**Rationale**: het apparaat draait onbeheerd en langdurig; heapfragmentatie, hangende
verbindingen en ongedefinieerde uitgangen leiden tot storingen die lastig te
reproduceren zijn.

### IV. Configuratie en geheimen buiten de code

- WiFi-wachtwoorden, API-sleutels en andere geheimen MOGEN NIET in versiebeheer staan;
  ze staan in een bestand dat in `.gitignore` is opgenomen (bijv. `secrets.h`), met een
  gecommit voorbeeldbestand (bijv. `secrets.example.h`).
- Instelbare parameters (tijden, drempelwaarden, helderheid) MOETEN als benoemde
  constanten of in persistente opslag (NVS/`Preferences`) staan, niet verspreid door de
  code.
- Als OTA-updates worden ondersteund, MOETEN deze beveiligd zijn met een wachtwoord of
  gelijkwaardige authenticatie.

**Rationale**: gelekte credentials in een repository zijn niet terug te draaien, en
gecentraliseerde configuratie maakt aanpassen zonder codewijziging mogelijk.

### V. Eenvoud en resource-bewustzijn

- Begin met de eenvoudigste oplossing die aan de specificatie voldoet (YAGNI); extra
  bibliotheken, taken of abstractielagen MOETEN in het plan gerechtvaardigd worden.
- Elke externe bibliotheek MOET met een vaste versie in `lib_deps` van
  `platformio.ini` zijn vastgelegd.
- Flash- en RAM-gebruik (zoals getoond door `pio run`) MOETEN na elke build
  gecontroleerd worden; een toename van meer dan 10% ten opzichte van de vorige release
  MOET worden verklaard.
- Logging via `Serial` MOET niveaus kennen (bijv. ERROR, WARN, INFO, DEBUG) en
  compile-time uit te schakelen zijn; logging MAG het tijdsgedrag niet merkbaar
  beïnvloeden.

**Rationale**: een microcontroller heeft beperkte middelen; ongecontroleerde groei van
code en afhankelijkheden maakt de firmware fragiel en moeilijk te onderhouden.

## Technische randvoorwaarden

- **Doelplatform**: ESP32 (Espressif), Arduino-framework (arduino-esp32 core).
- **Taal**: C++ (de door de gebruikte core ondersteunde standaard, minimaal C++17).
- **Build**: PlatformIO is het buildsysteem. De firmware MOET reproduceerbaar via de
  commandoregel te bouwen zijn met `pio run` (en te flashen met `pio run -t upload`).
  - `platformio.ini` in de root van de repository is de enige bron voor board,
    framework, build-flags en afhankelijkheden, en MOET gecommit worden.
  - De `platform`-versie (espressif32) en alle `lib_deps` MOETEN vastgepind zijn op een
    exacte versie, zodat builds reproduceerbaar zijn.
  - Er MOETEN minimaal twee omgevingen zijn: een ESP32-omgeving voor de firmware en een
    `native`-omgeving voor host-unit tests (principe II).
  - Projectindeling volgt de PlatformIO-conventie: `src/`, `include/`, `lib/`, `test/`.
  - De map `.pio/` (buildartefacten) MOET in `.gitignore` staan.
- **Compilerwaarschuwingen**: de code MOET compileren met `-Wall` zonder nieuwe
  waarschuwingen in projectcode.
- **Codestijl**: consistente naamgeving (`camelCase` voor functies/variabelen,
  `PascalCase` voor klassen/types, `UPPER_SNAKE_CASE` voor constanten en macro's);
  geen globale variabelen buiten een duidelijk afgebakende module-scope.
- **Elektrische veiligheid**: schakelen van netspanning MOET via een geschikt,
  geïsoleerd relais of SSR gebeuren; aansluitschema's horen bij de documentatie.

## Ontwikkelworkflow en kwaliteitspoorten

- Features doorlopen de Spec Kit-flow: specificatie → plan → taken → implementatie.
- Elk plan MOET een Constitution Check bevatten die toetst aan principes I–V.
- Vóór het afronden van een feature MOETEN de volgende poorten gehaald zijn:
  1. De firmware bouwt met `pio run` zonder fouten en zonder nieuwe waarschuwingen.
  2. Alle host-unit tests slagen met `pio test -e native`.
  3. Een handmatige rooktest op echte hardware is uitgevoerd en het resultaat
     (inclusief geteste scenario's zoals stroomuitval en WiFi-uitval) is vastgelegd.
  4. Flash/RAM-gebruik is gecontroleerd conform principe V.
- Commits zijn klein en beschrijvend; geheimen of buildartefacten worden nooit
  gecommit.

## Governance

- Deze constitutie gaat boven andere werkafspraken binnen dit project. Bij conflict
  geldt de constitutie.
- Wijzigingen worden voorgesteld via `/speckit-constitution`, met een Sync Impact Report
  en een motivatie; afhankelijke sjablonen en lopende plannen worden daarna op
  consistentie gecontroleerd.
- Versiebeheer volgt semantische versionering:
  - MAJOR: verwijderen of herdefiniëren van principes op een niet-compatibele manier.
  - MINOR: nieuw principe of nieuwe sectie, of wezenlijk uitgebreide richtlijnen.
  - PATCH: verduidelijkingen, formulering, typfouten.
- Elke review van plan of code MOET naleving controleren; afwijkingen MOETEN in de
  sectie "Complexity Tracking" van het plan worden gerechtvaardigd.

**Version**: 1.1.0 | **Ratified**: 2026-11-22 | **Last Amended**: 2026-11-22
