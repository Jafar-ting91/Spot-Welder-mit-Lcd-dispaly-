🔧 Spot-Welder-60W10A (STM32)

Ein kompakter, mikrocontrollergesteuerter Kondensator-Spotwelder mit TFT-Interface, präziser Lade- und Pulssteuerung sowie umfangreichen Schutz- und Einstellfunktionen.

Dieses Projekt ist eine komplette Firmware für einen selbstgebauten Spotwelder auf STM32-Basis, ausgelegt für kontrolliertes Punktschweißen mit einstellbaren Pulsen und stabiler Energieverwaltung.

⚡ Leistung & technische Daten
Max. Ladeleistung: ~60 W
Max. Ladestrom: ~10 A (geregelt über PI-Regler)
Versorgung: Kondensatorbasiert (2 Zellen / Caps)
Spannungsbereich pro Zelle: 2.0 V – 4.2 V
Gesamtspannung (Weld Voltage): bis ~8.4 V
Shunt-Messung: 5 mΩ (INA219)
PWM-Steuerung: hochauflösend (Timer4)

👉 Ziel: konstante Energie + reproduzierbare Schweißpunkte

🎯 Hauptfunktionen
🔋 Intelligente Lade- & Energieverwaltung
PI-geregelter Ladestrom
Leistungsbegrenzung (Watt-Limit)
Soft-Ramp (sanftes Hochfahren des Stroms)
automatische Reduzierung nahe Zielspannung
⚙️ Schweißsteuerung
Puls 1 & Puls 2 einstellbar (ms)
einstellbare Pause zwischen Pulsen
Hand-Trigger + Auto-Trigger
einstellbare Trigger-Verzögerung
Throttling während des Schweißens (Schutz der Elektronik)
🌡️ Schutz & Sicherheit
Temperaturüberwachung (2 Sensoren)
Über- & Unterspannungsschutz
automatisches Entladen bei Überspannung
Ladefreigabe mit Hysterese
🔄 Balancing
automatische Zellbalance
differenzbasierte Steuerung
Schutz vor Drift zwischen den Kondensatoren
🖥️ Benutzerinterface
TFT Display (ILI9341)
Drehencoder + Button
Live-Anzeige:
Spannung (einzeln + gesamt)
Strom (A)
Leistung (W)
Temperatur
Status (z. B. LADEN, WELDING, ENTLADE)
💾 Komfort
EEPROM-Speicherung aller Parameter
Schweißzähler (Weld Count)
Auto-Kalibrierung für Stromoffset
Stoppuhr-Funktion integriert
🧠 Besonderheiten
Stabiler PI-Regler für sauberen Stromfluss
Anti-Flacker UI → Updates nur bei Änderungen
Sauberes Timing für Pulse
Progress-Bar für Trigger-Verzögerung
Optimiert für reproduzierbare Schweißpunkte
🛠️ Hardware
STM32 (z. B. Bluepill)
ILI9341 TFT Display
INA219 Stromsensor
Rotary Encoder
MOSFET / Leistungsteil
2x Kondensator / Zellen
NTC Temperaturfühler (2x)
🚀 Ziel des Projekts

Ein präziser, sicherer und einstellbarer Spotwelder, der:

reproduzierbare Ergebnisse liefert
einfach zu bedienen ist
gleichzeitig maximale Kontrolle über Energie und Pulse bietet
