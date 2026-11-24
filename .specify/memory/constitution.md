# VoordeurLamp Constitution

Embedded software voor de ESP32, gebouwd met ESPHome en geïntegreerd in Home Assistant.

## Core Principles

### I. Niet-blokkerende uitvoering

- De `loop()`- en `update()`-methoden van eigen ESPHome-componenten MOETEN binnen 30 ms
  terugkeren (de drempel waarboven ESPHome waarschuwt dat een component te lang duurt);
  blokkerende aanroepen (`delay()` langer dan enkele milliseconden, busy-wait lussen,
  synchrone netwerkcalls zonder timeout) zijn verboden.
- Tijdgestuurd gedrag MOET worden gerealiseerd met `millis()`-gebaseerde timers,
  ESPHome-timers (`set_timeout`, `set_interval`) en expliciete toestandsmachines.
- Elke externe operatie (UART-communicatie met sensoren, netwerk) MOET een timeout hebben.
- Code die vanuit een ISR draait MOET minimaal zijn (vlag zetten, `IRAM_ATTR`), en gedeelde
  variabelen MOETEN `volatile` of atomair beschermd zijn.

**Rationale**: ESPHome draait alle componenten in één hoofdlus; één blokkerende component
maakt het hele apparaat onresponsief, verstoort de verbinding met Home Assistant en kan de
watchdog laten afgaan.

### II. Gescheiden en testbare logica

- Beslislogica (bijv. detectie, wanneer de lamp aan of uit gaat) MOET gescheiden zijn van
  ESPHome en van hardwaretoegang (UART, GPIO, tijdsbron) via kleine interfaces of
  injecteerbare functies; deze logica MAG geen ESPHome-, Arduino- of ESP-IDF-headers gebruiken.
- Pure logica MOET zonder hardware testbaar zijn met unit tests op de host, via een
  PlatformIO `native`-omgeving met Unity, uit te voeren met `pio test -e native`. PlatformIO
  wordt uitsluitend als testrunner gebruikt (het wordt met ESPHome mee geïnstalleerd).
- Pindefinities en hardware-specifieke constanten MOETEN op één centrale plek staan: de
  `substitutions` van de ESPHome-YAML; magische getallen in de code zijn niet toegestaan.
- Nieuwe logica MOET vergezeld gaan van tests; bij bugfixes MOET eerst een falende test
  het probleem aantonen waar dat zonder hardware mogelijk is.

**Rationale**: testen op het apparaat is traag en foutgevoelig; host-tests geven snelle,
herhaalbare feedback en houden de logica onafhankelijk van ESPHome-versies en hardware.

### III. Betrouwbaarheid en veilige toestand

- Bij opstarten, na een reset en bij elke foutsituatie MOET de hardware (lamp, relais,
  uitgangen) in een gedefinieerde, veilige toestand staan (bijv. `restore_mode` expliciet
  ingesteld).
- Uitval van WiFi, Home Assistant, tijdsynchronisatie of andere externe diensten MAG de
  kernfunctie niet blokkeren; het apparaat MOET autonoom blijven werken en zelf opnieuw
  verbinden. Het apparaat MAG NIET herstarten alleen omdat Home Assistant onbereikbaar is
  (`api: reboot_timeout: 0s`), tenzij het plan dit rechtvaardigt.
- De task watchdog MOET actief blijven; deze uitschakelen vereist een gedocumenteerde
  rechtvaardiging.
- Dynamische geheugenallocatie (`new`, `malloc`, groeiende `std::string`/`String`-objecten)
  MAG NIET in herhaald uitgevoerde paden van eigen code plaatsvinden; gebruik vaste buffers
  of allocatie tijdens `setup()`.
- De reset-oorzaak MOET bij opstarten zichtbaar zijn (bijv. via de ESPHome
  `debug`-component).

**Rationale**: het apparaat draait onbeheerd en langdurig; heapfragmentatie, hangende
verbindingen en ongedefinieerde uitgangen leiden tot storingen die lastig te
reproduceren zijn.

### IV. Configuratie en geheimen buiten de code

- WiFi-wachtwoorden, de API-encryptiesleutel, het OTA-wachtwoord en andere geheimen MOGEN
  NIET in versiebeheer staan; ze staan in `secrets.yaml` (opgenomen in `.gitignore`) en
  worden met `!secret` gebruikt, met een gecommit voorbeeldbestand `secrets.yaml.example`.
- De verbinding met Home Assistant MOET via de native API met encryptiesleutel lopen.
- OTA-updates MOETEN beveiligd zijn met een wachtwoord.
- Instelbare parameters (tijden, drempelwaarden, helderheid) MOETEN als benoemde waarden in de
  YAML staan of, als ze vanuit Home Assistant instelbaar zijn, als `number`/`select`-entiteiten
  met persistente opslag; niet verspreid door de code.

**Rationale**: gelekte credentials in een repository zijn niet terug te draaien, en
gecentraliseerde configuratie maakt aanpassen zonder codewijziging mogelijk.

### V. Eenvoud en resource-bewustzijn

- Gebruik waar mogelijk standaard ESPHome-componenten en YAML; eigen C++ (external
  components) alleen als de standaardcomponenten de specificatie niet halen. Extra
  componenten, bibliotheken, taken of abstractielagen MOETEN in het plan gerechtvaardigd worden.
- De ESPHome-versie MOET exact vastgepind zijn (in `requirements.txt`); externe componenten en
  bibliotheken MOETEN op een vaste versie of commit staan.
- Flash- en RAM-gebruik (zoals getoond door `esphome compile`) MOETEN na elke build
  gecontroleerd worden; een toename van meer dan 10% ten opzichte van de vorige release
  MOET worden verklaard.
- Logging MOET via de ESPHome-logger (`ESP_LOGx` met een `TAG` per component) gebeuren; het
  logniveau staat in de YAML. Logging MAG het tijdsgedrag niet merkbaar beïnvloeden.
- Waarden die vaak veranderen MOGEN niet vaker naar Home Assistant worden gestuurd dan nodig
  (bijv. via `throttle`-filters of een vast publicatie-interval).

**Rationale**: een microcontroller heeft beperkte middelen; ongecontroleerde groei van
code, afhankelijkheden en netwerkverkeer maakt de firmware fragiel en Home Assistant traag.

## Technische randvoorwaarden

- **Doelplatform**: ESP32 (Espressif) met ESPHome. Het onderliggende framework (`arduino` of
  `esp-idf`) wordt per apparaat in de YAML vastgelegd; eigen code gebruikt ESPHome-API's en
  geen framework-specifieke API's, tenzij het plan dit rechtvaardigt.
- **Integratie**: Home Assistant via de ESPHome native API.
- **Taal**: YAML voor de apparaatconfiguratie; C++ (minimaal C++17) voor external components;
  Python alleen voor de codegeneratie van external components.
- **Build**: ESPHome is het buildsysteem. De firmware MOET reproduceerbaar via de
  commandoregel te bouwen zijn met `esphome compile <apparaat>.yaml` en te flashen met
  `esphome run <apparaat>.yaml`.
  - De apparaat-YAML is de enige bron voor board, framework, pinnen, componenten en
    afhankelijkheden, en MOET gecommit worden.
  - De ESPHome-versie staat exact vastgepind in `requirements.txt`; het ESP32-platform en
    het framework MOETEN in de YAML op een vaste versie staan.
  - Eigen C++-code staat als external component in `components/<naam>/` en wordt lokaal
    ingeladen via `external_components`.
  - Projectindeling: `<apparaat>.yaml`, `components/`, `test/`, `platformio.ini` (alleen de
    `native`-testomgeving), `requirements.txt`, `secrets.yaml.example`.
  - De mappen `.esphome/` en `.pio/` en het bestand `secrets.yaml` MOETEN in `.gitignore` staan.
- **Compilerwaarschuwingen**: eigen code MOET zonder nieuwe waarschuwingen compileren, zowel
  in de ESPHome-build als in de native testomgeving (`-Wall`).
- **Codestijl**: in eigen C++ consistente naamgeving (`camelCase` voor functies/variabelen,
  `PascalCase` voor klassen/types, `UPPER_SNAKE_CASE` voor constanten en macro's); in
  ESPHome-componentcode mag de ESPHome-conventie (`snake_case`) worden gevolgd. Geen globale
  variabelen buiten een duidelijk afgebakende module-scope.
- **Elektrische veiligheid**: schakelen van netspanning MOET via een geschikt,
  geïsoleerd relais of SSR gebeuren; aansluitschema's horen bij de documentatie.

## Ontwikkelworkflow en kwaliteitspoorten

- Features doorlopen de Spec Kit-flow: specificatie → plan → taken → implementatie.
- Elk plan MOET een Constitution Check bevatten die toetst aan principes I–V.
- Vóór het afronden van een feature MOETEN de volgende poorten gehaald zijn:
  1. `esphome config <apparaat>.yaml` is geldig en `esphome compile <apparaat>.yaml` slaagt
     zonder nieuwe waarschuwingen in eigen code.
  2. Alle host-unit tests slagen met `pio test -e native`.
  3. Een handmatige rooktest op echte hardware is uitgevoerd en het resultaat
     (inclusief geteste scenario's zoals stroomuitval, WiFi-uitval en zichtbaarheid in
     Home Assistant) is vastgelegd.
  4. Flash/RAM-gebruik is gecontroleerd conform principe V.
- Commits zijn klein en beschrijvend; geheimen of buildartefacten worden nooit
  gecommit.

## Governance

- Deze constitutie gaat boven andere werkafspraken binnen dit project. Bij conflict
  geldt de constitutie.
- Wijzigingen worden voorgesteld via `/speckit-constitution`, met een motivatie;
  afhankelijke sjablonen en lopende plannen worden daarna op consistentie gecontroleerd.
- Versiebeheer volgt semantische versionering:
  - MAJOR: verwijderen of herdefiniëren van principes op een niet-compatibele manier.
  - MINOR: nieuw principe of nieuwe sectie, of wezenlijk uitgebreide richtlijnen.
  - PATCH: verduidelijkingen, formulering, typfouten.
- Elke review van plan of code MOET naleving controleren; afwijkingen MOETEN in de
  sectie "Complexity Tracking" van het plan worden gerechtvaardigd.

**Version**: 2.0.0 | **Ratified**: 2026-11-22 | **Last Amended**: 2026-11-24
