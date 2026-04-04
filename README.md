🔧 Spot-Welder-60W10A (STM32)

Ein kompakter, mikrocontrollergesteuerter Kondensator-Spotwelder mit TFT-Interface, präziser Lade- und Pulssteuerung sowie umfangreichen Schutz- und Einstellfunktionen.

Dieses Projekt ist eine Firmware für einen selbstgebauten Spotwelder auf STM32-Basis, ausgelegt für kontrolliertes Punktschweißen mit einstellbaren Pulsen und stabiler Energieverwaltung.






⚠️ Projektstatus


🟡 Aktiv in Entwicklung

Die aktuelle Version ist voll funktionsfähig und wird im praktischen Einsatz verwendet.
Alle Hauptfunktionen (Laden, Schweißen, Anzeige, Schutzmechanismen) laufen stabil.

👉 Trotzdem gilt:

Einige Bereiche sind noch in Optimierung
UI und Regelverhalten werden weiter verbessert
Code wird laufend aufgeräumt und erweitert

Kurz gesagt:
✔ Funktioniert zuverlässig
🔧 Wird kontinuierlich verbessert





⚡ Leistung & technische Daten
Max. Ladeleistung: ~60 W
Max. Ladestrom: ~10 A (geregelt)
Spannungsbereich: 2.0 V – 4.2 V pro Zelle
Gesamtspannung: bis ~8.4 V
Strommessung: INA219 (Shunt 5 mΩ)
Regelung: PI-Regler + Ramp-Logik




🎯 Hauptfunktionen
🔋 Lade- & Energieverwaltung
PI-geregelter Strom
Leistungsbegrenzung (Watt)
Softstart + dynamische Anpassung
⚙️ Schweißsteuerung
Puls 1 / Puls 2 (ms einstellbar)
Puls-Pause einstellbar
Auto- und Hand-Trigger
Trigger-Verzögerung mit Progress-Bar
🌡️ Schutzfunktionen
Temperaturüberwachung (2 Sensoren)
Über-/Unterspannungsschutz
automatische Entladung bei Bedarf
🔄 Balancing
automatische Zellangleichung
Schutz vor Spannungsdrift
🖥️ Anzeige & Bedienung
TFT Display (ILI9341)
Encoder-Steuerung
Echtzeitdaten:
Spannung
Strom
Leistung
Temperatur
Status
💾 Zusatzfunktionen
EEPROM Speicherung
Schweißzähler
Offset-Kalibrierung
integrierte Stoppuhr
🧠 Besonderheiten
stabile und ruhige Regelung
saubere Pulssteuerung
UI ohne Flackern (Update nur bei Änderung)
praxisorientiertes Design
optimiert für reproduzierbare Schweißpunkte




🛠️ Hardware
STM32 (z. B. Bluepill)
ILI9341 Display
INA219
Rotary Encoder
MOSFET Leistungsstufe
2x Kondensator/Zellen
2x NTC Sensoren
🚀 Ziel

Ein zuverlässiger und präziser DIY-Spotwelder, der einfach zu bedienen ist und gleichzeitig volle Kontrolle über Energie und Schweißparameter bietet.
