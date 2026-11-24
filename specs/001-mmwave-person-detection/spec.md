# Feature Specification: Persoonsdetectie met mmWave-radar

**Feature Branch**: `001-mmwave-person-detection` (werk gebeurt op `main`; geen aparte branch aangemaakt)

**Created**: 2026-11-22

**Status**: Draft

**Input**: User description: "Ik wil een mmwave radar gebruiken om mensen die aan komen lopen buiten te kunnen detecteren. De software moet voorkomen dat er foute detecties plaats vinden omdat door de wind struiken of bladeren bewegen. Een persoon zal altijd met een bepaalde snelheid naar de sensor toe of van de sensor af lopen."

## Clarifications

### Session 2026-11-24

- Q: Welke mmWave-radarmodule wordt gebruikt? → A: Seeed "24GHz mmWave for XIAO" met
  HLK-LD2410B (zie `Radar/`-documentatie). Deze levert geen snelheid en geen meerdere doelen;
  alleen afstand en signaalsterkte van het sterkste bewegende en stilstaande doel, plus
  (in engineering-modus) signaalsterkte per afstandszone van 0,75 m. Snelheid en richting
  worden afgeleid uit het verloop van afstand en zone-energie over tijd.
- Q: Moet deze feature ook de lamp schakelen, of alleen personen detecteren? → A: Alleen
  detectie; lampaansturing wordt een aparte, latere feature die de meldingen gebruikt.
- Q: Moeten katten en andere kleine dieren worden uitgefilterd? → A: Zo goed mogelijk, via
  een instelbare minimale signaalsterkte per afstandszone; geen garantie, en detecties door
  dieren tellen niet mee als valse detectie in SC-003.
- Q: Hoe loopt het pad naar de voordeur ten opzichte van de sensor? → A: Schuin of met een
  bocht; de afstand tot de sensor neemt daardoor langzamer af dan de loopsnelheid en kan in
  de bocht kort nagenoeg gelijk blijven.
- Q: Blijft de status "persoon aanwezig" als iemand na detectie stil blijft staan? → A: Ja,
  begrensd: een stilstaand doel houdt de status vast, maar alleen in de afstandszone waar de
  persoon het laatst bewoog en hoogstens een instelbare maximale tijd (standaard 60 s).
- Q: Op welk platform wordt de firmware gebaseerd? → A: ESPHome; het apparaat wordt een
  sensor voor Home Assistant. Status en meldingen worden via ESPHome aan Home Assistant
  beschikbaar gesteld.
- Q: Welke informatie moet in Home Assistant zichtbaar zijn? → A: Alles: aanwezigheid,
  richting, detectiemelding, sensorstatus, positie, snelheid, laatste afwijzingsreden én de
  signaalsterkte per afstandszone (9 zones).
- Q: Moeten de drempelwaarden vanuit Home Assistant instelbaar zijn? → A: Ja, alle drempels uit
  FR-011 zijn vanuit Home Assistant instelbaar en blijven bewaard na een herstart.
- Q: Wat toont "persoon aanwezig" in Home Assistant bij een sensorstoring? → A: "Niet
  beschikbaar"; na herstel direct de actuele waarde. (Verfijnd in de volgende vraag.)
- Q: Kan "persoon aanwezig" bij een storing "niet beschikbaar" worden? → A: Nee; ESPHome kent
  beschikbaarheid alleen per apparaat. Het wordt "onbekend", met "Radar storing" aan (research R8).

### Session 2026-11-24 (na hardwaretest)

- Q: Moet een vertrek ook gemeld worden als de persoon eerst naderde en binnen dezelfde
  aanwezigheid weer wegloopt (bijv. een bezorger)? → A: Ja. Tijdens een aanwezigheid volgt een
  richtingsmelding zodra de persoon aantoonbaar de andere kant op loopt; de aanwezigheid zelf
  blijft één geheel (geen nieuwe "persoon gedetecteerd").

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Naderende persoon wordt betrouwbaar gedetecteerd (Priority: P1)

Een bezoeker of bewoner loopt over het pad naar de voordeur. Het systeem herkent dat er een
persoon nadert en meldt dit, voordat de persoon de deur bereikt.

**Why this priority**: dit is de kernfunctie; zonder betrouwbare detectie van een naderende
persoon heeft het systeem geen waarde.

**Independent Test**: een testpersoon loopt meerdere keren in normaal tempo vanaf de rand van het
detectiegebied naar de sensor; per poging wordt vastgelegd of en wanneer een detectie wordt gemeld.

**Acceptance Scenarios**:

1. **Given** het detectiegebied is leeg, **When** een persoon in normaal wandeltempo vanaf de
   rand van het gebied naar de sensor loopt, **Then** meldt het systeem binnen 2 seconden na
   het betreden van het gebied "persoon gedetecteerd" met richting "naderend".
2. **Given** een persoon is zojuist gedetecteerd, **When** de persoon verder doorloopt naar de
   deur, **Then** wordt voor deze ene nadering slechts één detectiemelding gegeven.
3. **Given** een persoon is gedetecteerd, **When** er gedurende de ingestelde wegtijd geen
   persoon meer wordt waargenomen, **Then** meldt het systeem "persoon weg".
4. **Given** een persoon is gedetecteerd en blijft bij de deur stilstaan, **When** de sensor
   een stilstaand doel ziet in de zone waar de persoon het laatst bewoog, **Then** blijft de
   status "persoon aanwezig", tot hoogstens de maximale vasthoudtijd (standaard 60 s).

---

### User Story 2 - Geen valse detecties door bewegende begroeiing (Priority: P1)

Bij wind bewegen struiken, takken en bladeren in het detectiegebied heen en weer. Het systeem
mag deze bewegingen niet als persoon aanmerken.

**Why this priority**: valse detecties maken het systeem onbruikbaar en ondermijnen het
vertrouwen; dit is expliciet door de gebruiker als eis gesteld en is even belangrijk als de
detectie zelf.

**Independent Test**: het systeem staat een langere periode bij winderig weer opgesteld met
begroeiing in het detectiegebied, zonder dat er personen langskomen; het aantal valse
detecties wordt geteld.

**Acceptance Scenarios**:

1. **Given** het detectiegebied bevat struiken die door de wind heen en weer bewegen, **When**
   er geen persoon aanwezig is, **Then** meldt het systeem geen detectie.
2. **Given** windstoten laten takken korte tijd snel bewegen, **When** de beweging geen
   aanhoudende verplaatsing in één richting oplevert, **Then** meldt het systeem geen detectie.
3. **Given** begroeiing beweegt in het detectiegebied, **When** tegelijk een persoon naar de
   sensor loopt, **Then** wordt de persoon toch gedetecteerd volgens User Story 1.

---

### User Story 3 - Richting van de beweging wordt onderscheiden (Priority: P2)

Het systeem onderscheidt of een persoon naar de sensor toe loopt (aankomst) of ervan af loopt
(vertrek), zodat later gekoppelde functies daar verschillend op kunnen reageren.

**Why this priority**: waardevol voor vervolgfuncties (bijv. lamp alleen aan bij aankomst),
maar de basisdetectie werkt ook zonder.

**Independent Test**: een testpersoon loopt afwisselend naar de sensor toe en ervan af;
gecontroleerd wordt of de gemelde richting overeenkomt met de werkelijke richting.

**Acceptance Scenarios**:

1. **Given** het detectiegebied is leeg, **When** een persoon vanaf de deur van de sensor weg
   loopt, **Then** meldt het systeem "persoon gedetecteerd" met richting "verwijderend".
2. **Given** een persoon loopt naar de sensor toe, **When** de detectie wordt gemeld, **Then**
   is de gemelde richting "naderend".
3. **Given** een persoon is als "naderend" gedetecteerd en nog aanwezig, **When** de persoon
   omdraait en minstens 1 s en 0,6 m aanhoudend van de sensor wegloopt, **Then** meldt het
   systeem de richting "verwijderend", zonder nieuwe melding "persoon gedetecteerd".

---

### User Story 4 - Instellen en afstemmen op de locatie (Priority: P3)

De installateur (de gebruiker zelf) kan de detectie afstemmen op de situatie bij de voordeur,
zoals de grootte van het detectiegebied en de gevoeligheid, en kan zien waarom bewegingen wel
of niet als persoon zijn aangemerkt.

**Why this priority**: elke locatie is anders; afstemmen verhoogt de betrouwbaarheid, maar de
standaardinstellingen moeten zonder afstemming al bruikbaar zijn.

**Independent Test**: een parameter (bijv. maximale afstand) wordt gewijzigd en gecontroleerd
wordt dat het gedrag daarna overeenkomt; diagnose-uitvoer toont bij een afgewezen beweging de
reden.

**Acceptance Scenarios**:

1. **Given** de maximale detectieafstand is in Home Assistant ingesteld op 4 m, **When** een
   persoon op 5 m afstand loopt, **Then** volgt geen detectie, ook na een herstart van het
   apparaat.
2. **Given** de minimale snelheid staat op 0,3 m/s, **When** in Home Assistant een maximale
   snelheid van 0,2 m/s wordt ingesteld, **Then** wordt die waarde geweigerd en blijft de vorige
   maximale snelheid actief.
3. **Given** diagnose-uitvoer staat aan, **When** een beweging wordt afgewezen, **Then** toont
   de uitvoer de reden (bijv. "snelheid buiten bereik", "geen aanhoudende richting",
   "buiten gebied").

---

### Edge Cases

- **Persoon blijft stilstaan** (bijv. sleutels zoeken bij de deur): na een geldige detectie
  blijft de status "persoon aanwezig" behouden zolang de sensor een stilstaand doel ziet in de
  zone waar de persoon het laatst bewoog, tot hoogstens de maximale vasthoudtijd (FR-009);
  stilstand alleen leidt nooit tot een nieuwe detectie.
- **Struik als stilstaand doel**: begroeiing kan door de wind als stilstaand doel verschijnen;
  de beperking tot de laatste zone en de maximale vasthoudtijd voorkomen dat de status
  daardoor onbeperkt "persoon aanwezig" blijft.
- **Zeer langzaam lopen** (onder de minimale loopsnelheid) of **hardlopen** (boven de
  maximale): valt buiten het ingestelde snelheidsbereik en wordt niet als persoon gemeld; het
  bereik is instelbaar.
- **Persoon loopt dwars** langs de sensor (bijv. over de stoep): de beweging naar of van de
  sensor af is klein; dit leidt niet tot detectie zolang er geen aanhoudende nadering of
  verwijdering is.
- **Bocht in het pad**: tijdens de bocht verandert de afstand tot de sensor kort nauwelijks;
  dit breekt de detectie niet af (FR-002). Omdat de persoon schuin loopt, is de gemeten
  snelheid naar de sensor lager dan de werkelijke loopsnelheid; het snelheidsbereik (FR-003)
  heeft daarom een lage ondergrens.
- **Voorbijgangers en verkeer buiten het detectiegebied**: worden genegeerd op basis van de
  ingestelde maximale afstand.
- **Meerdere personen tegelijk**: er volgt minimaal één detectiemelding; tellen van personen is
  niet vereist.
- **Regen, sneeuw of hagel**: mag niet tot detectie leiden (kortstondige, ongerichte
  bewegingen).
- **Persoon al aanwezig bij opstarten**: na de opstartperiode wordt de persoon pas gemeld
  zodra deze een geldige beweging maakt; tijdens de opstartperiode worden geen detecties
  gemeld.
- **Sensor reageert niet of geeft ongeldige gegevens**: het systeem meldt een sensorfout,
  meldt geen detecties zolang de fout duurt (in Home Assistant is "persoon aanwezig" dan
  "onbekend" en "Radar storing" aan) en herstelt automatisch zodra de sensor weer werkt.
- **WiFi of Home Assistant onbereikbaar**: de detectie gaat op het apparaat door; meldingen
  die tijdens de onderbreking ontstaan, worden niet achteraf nagestuurd, maar na herstel wordt
  de actuele status direct doorgegeven (FR-016).
- **Kleine dieren** (katten, vogels): bewegingen met een signaalsterkte onder de ingestelde
  minimale waarde voor die afstandszone worden afgewezen (FR-015); een kat dichtbij kan die
  drempel toch halen, dus uitfilteren is niet gegarandeerd.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Het systeem MOET het detectiegebied voor de voordeur continu bewaken met de
  mmWave-radar.
- **FR-002**: Het systeem MOET een bewegend object alleen als persoon aanmerken wanneer het
  object gedurende een minimale duur (standaard 1 seconde) aanhoudend in één richting naar de
  sensor toe of van de sensor af beweegt. Een korte onderbreking waarin de afstand nagenoeg
  gelijk blijft (bijv. in de bocht van het pad, standaard maximaal 1 seconde) MAG het
  bewegingsspoor niet afbreken; een omkering van richting wel.
- **FR-003**: Het systeem MOET een object alleen als persoon aanmerken wanneer de snelheid
  naar of van de sensor binnen het loopsnelheidsbereik valt (standaard 0,3 tot 2,5 m/s). Omdat
  de sensor geen snelheid levert, MOET deze snelheid worden afgeleid uit de verandering van de
  gemeten afstand over een tijdvenster.
- **FR-004**: Het systeem MOET een object alleen als persoon aanmerken wanneer de afstand tot
  de sensor tijdens die beweging netto minimaal een ingestelde waarde verandert (standaard
  0,5 m).
- **FR-005**: Het systeem MOET heen-en-weer gaande bewegingen met weinig netto verplaatsing,
  zoals die van begroeiing in de wind, afwijzen, ook als de momentane snelheid binnen het
  loopsnelheidsbereik valt. Een persoon kenmerkt zich doordat de bewegingsenergie
  achtereenvolgens door aangrenzende afstandszones schuift; begroeiing kenmerkt zich doordat
  de bewegingsenergie in dezelfde zone(s) blijft.
- **FR-006**: Het systeem MOET objecten buiten het ingestelde detectiegebied (minimale en
  maximale afstand, standaard 0,75 tot 5 m; maximaal 6 m door het sensorbereik) negeren.
- **FR-007**: Het systeem MOET bij een geldige detectie de melding "persoon gedetecteerd"
  geven, met richting (naderend of verwijderend), afstand op het moment van detectie en
  tijdstip.
- **FR-008**: Het systeem MOET per doorlopende aanwezigheid één detectiemelding "persoon
  gedetecteerd" geven en MAG die NIET herhalen zolang de persoon aanwezig blijft. Beweegt de
  persoon tijdens de aanwezigheid aanhoudend in de tegengestelde richting van de laatst gemelde
  (volgens dezelfde criteria als FR-002 t/m FR-004), dan MOET het systeem een richtingsmelding
  met de nieuwe richting geven.
- **FR-009**: Het systeem MOET de melding "persoon weg" geven wanneer na een detectie
  gedurende een instelbare wegtijd (standaard 5 seconden) geen persoon meer wordt waargenomen.
  Een stilstaand doel in de afstandszone waar de persoon het laatst bewoog (of een direct
  aangrenzende zone) geldt als "persoon waargenomen", maar hoogstens gedurende een instelbare
  maximale vasthoudtijd (standaard 60 seconden) na de laatste beweging; daarna volgt
  "persoon weg" zodra de wegtijd zonder beweging verstreken is.
- **FR-010**: Het systeem MOET de huidige status (geen persoon / persoon aanwezig / sensorfout)
  op elk moment beschikbaar stellen aan Home Assistant.
- **FR-011**: Alle drempelwaarden (detectiegebied, snelheidsbereik, minimale duur, minimale
  netto verplaatsing, toegestane onderbreking, wegtijd, maximale vasthoudtijd, minimale
  signaalsterkte per afstandszone) MOETEN vanuit Home Assistant instelbaar zijn, zonder
  opnieuw flashen en zonder de detectielogica te wijzigen. Gewijzigde waarden MOETEN direct
  ingaan en na een herstart of stroomuitval bewaard blijven. Standaardwaarden staan in de
  ESPHome-configuratie.
- **FR-012**: Het systeem MOET optionele diagnose-uitvoer bieden die per beoordeelde beweging
  toont of deze is geaccepteerd of afgewezen, en zo ja om welke reden.
- **FR-013**: Het systeem MOET binnen 3 seconden detecteren dat de sensor geen geldige
  gegevens meer levert, dit als sensorfout melden en automatisch herstellen zodra de sensor
  weer werkt. Tijdens een sensorfout MOET "Radar storing" in Home Assistant aan staan en MAG
  "persoon aanwezig" geen geldige toestand hebben (Home Assistant toont "onbekend"), zodat
  automatiseringen die op "aan" reageren niet afgaan; na herstel MOET direct de actuele waarde
  worden doorgegeven.
- **FR-014**: Het systeem MAG NIET binnen een opstartperiode (standaard 5 seconden na
  inschakelen) detecties melden.
- **FR-015**: Het systeem MOET bewegingen afwijzen waarvan de signaalsterkte onder de
  ingestelde minimale waarde voor de betreffende afstandszone ligt, om kleine dieren zo goed
  mogelijk uit te filteren; de reden "signaal te zwak" MOET in de diagnose-uitvoer (FR-012)
  verschijnen.
- **FR-016**: Het systeem MOET de detectiemeldingen (FR-007, FR-009) en de status (FR-010) via
  ESPHome aan Home Assistant doorgeven. De detectie zelf MOET op het apparaat plaatsvinden en
  blijven werken als de verbinding met WiFi of Home Assistant wegvalt; na herstel van de
  verbinding MOET de actuele status direct worden doorgegeven.
- **FR-017**: Het systeem MOET in Home Assistant de volgende gegevens beschikbaar stellen:
  - **Bediening/automatisering**: persoon aanwezig (aan/uit), richting van de laatste
    detectie (naderend / verwijderend), en een melding per detectie;
  - **Diagnose**: sensorstatus (ok / fout), geschatte positie (m), afgeleide snelheid (m/s),
    laatste afwijzingsreden, de bewegingssignaalsterkte per afstandszone (zones 0–8) en de
    drempel per zone waarboven beweging meetelt.
  Diagnosegegevens MOETEN als diagnose-onderdelen gemarkeerd zijn en hoogstens één keer per
  seconde per waarde worden bijgewerkt, zodat WiFi-verkeer en de Home Assistant-database
  beperkt blijven; aanwezigheid en detectiemeldingen worden direct doorgegeven (SC-007).
- **FR-018**: Het systeem MOET een ongeldige combinatie van drempelwaarden (bijv. minimale
  snelheid ≥ maximale snelheid, minimale afstand ≥ maximale afstand, of waarden buiten het
  sensorbereik) weigeren: de vorige geldige waarde blijft actief en de weigering is zichtbaar in
  de diagnose-uitvoer.

### Key Entities *(include if feature involves data)*

- **Waarneming**: één meting van de radar; doelstatus (geen / bewegend / stilstaand / beide),
  afstand en signaalsterkte van het sterkste bewegende doel, afstand en signaalsterkte van het
  sterkste stilstaande doel, signaalsterkte per afstandszone (0,75 m per zone) en tijdstip.
- **Bewegingsspoor**: de reeks waarnemingen over tijd; hieruit volgen afgeleide snelheid,
  richting, duur, netto verplaatsing en het verschuiven van bewegingsenergie tussen zones.
- **Detectiegebeurtenis**: een melding "persoon gedetecteerd" (met richting, afstand,
  tijdstip), een richtingsmelding tijdens een aanwezigheid, of "persoon weg".
- **Detectiestatus**: de actuele toestand van de detectie: geen persoon, persoon aanwezig of
  sensorfout.
- **Detectieparameters**: de instelbare drempelwaarden uit FR-011.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Minimaal 95% van de personen die in normaal wandeltempo over het werkelijke pad
  (inclusief de bocht) naar de deur lopen, wordt gedetecteerd (gemeten over minimaal 40
  loopproeven).
- **SC-002**: De detectie volgt binnen 2 seconden nadat de persoon het detectiegebied betreedt,
  en in ieder geval voordat de persoon de deur bereikt.
- **SC-003**: Gedurende 7 dagen opstelling met begroeiing in het detectiegebied, inclusief
  winderige perioden, treedt gemiddeld hoogstens 1 valse detectie per 24 uur op. Detecties
  veroorzaakt door dieren tellen hierbij niet mee.
- **SC-004**: Bij minimaal 95% van de detecties is de gemelde richting (naderend of
  verwijderend) juist.
- **SC-005**: Het systeem werkt 7 dagen achtereen zonder handmatige herstart of ingreep.
- **SC-006**: Na het tijdelijk loskoppelen van de sensor werkt de detectie binnen 10 seconden
  na herstel weer zonder ingreep.
- **SC-007**: Bij een werkende WiFi-verbinding is een detectie binnen 1 seconde zichtbaar in
  Home Assistant.

## Assumptions

- **Platform**: de firmware wordt gebouwd met ESPHome en geïntegreerd in Home Assistant. Het
  bord is een Seeed XIAO ESP32-S3. WiFi-gegevens en sleutels staan in een niet-gecommit
  geheimenbestand (constitutie IV).
- **Scope** (bevestigd): deze feature levert uitsluitend de detectie en de bijbehorende
  meldingen en status. Het aan- en uitschakelen van de lamp valt buiten deze feature en wordt
  een aparte, latere feature die deze meldingen gebruikt.
- **Opstelling**: de radar is bij de voordeur gemonteerd; het looppad komt schuin binnen of
  maakt een bocht. De sensor wordt zo gericht dat het grootste deel van het pad binnen de
  openingshoek (±60°) valt en de afstand tot de sensor over het pad merkbaar afneemt; het
  detectiegebied is ongeveer 5 m diep.
- **Sensor**: Seeed "24GHz mmWave for XIAO" (HLK-LD2410B, 24 GHz FMCW), bereik 0,75–6 m,
  openingshoek ±60°, afstandsresolutie 0,75 m per zone. De sensor rapporteert slechts één
  bewegend en één stilstaand doel (het sterkste) en geen snelheid. Documentatie staat in
  `Radar/`.
- **Gevolg van één doel**: als begroeiing en een persoon tegelijk bewegen, rapporteert de
  sensor alleen het sterkste bewegende doel. De beoordeling van zone-energie per afstandszone
  (FR-005) is daarom nodig om User Story 2, scenario 3 te halen.
- **Stilstaande persoon**: de sensor kan ook stilstaande personen waarnemen; dit wordt
  begrensd gebruikt om de status "persoon aanwezig" vast te houden (FR-009), maar nooit om een
  nieuwe detectie te starten.
- **Buitengebruik**: de sensor is primair voor binnen ontworpen; montage buiten vereist een
  behuizing zonder metaal die 24 GHz goed doorlaat, en een stevige, trillingsvrije bevestiging.
- **Loopsnelheid**: normaal wandelen ligt rond 1 tot 1,5 m/s; het standaard bereik van 0,3 tot
  2,5 m/s dekt langzaam lopen tot stevig doorstappen. Hardlopen valt hier buiten.
- **Dieren**: kleine dieren worden zo goed mogelijk uitgefilterd via een minimale
  signaalsterkte per afstandszone (FR-015); volledig uitfilteren is met deze sensor niet
  haalbaar en geen eis.
- **Tellen en identificatie**: het tellen of herkennen van personen valt buiten de scope.
- **Standaardwaarden** zijn een startpunt en worden tijdens de test op de echte locatie
  afgestemd (User Story 4).
