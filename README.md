# Minecraft Round Monitor – Waveshare ESP32-S3-Touch-AMOLED-1.75

Dieses Paket enthält die Firmware-Quelle und einen Web-Installer für das runde
Waveshare ESP32-S3-Touch-AMOLED-1.75 (nicht 1.75C).

## Verhalten
- Seite 1: Serverstatus
- Seite 2: aktuell eingeloggte Spieler
- Seite 3: letzte Ereignisse
- Touch links/rechts wechselt Seiten
- unbekannter Spieler erzeugt eine rote Warnseite
- Statusabfrage alle 3 Sekunden
- beim ersten Start: WLAN-Setup-AP `MinecraftMonitor-Setup`
  - verbinden
  - Browser: `http://192.168.4.1`
  - WLAN + API speichern
  - Standard-API: `http://192.168.178.115:8080/api/status`

## Web-Installer
Der Browser-Installer braucht eine kompilierte `firmware.bin` im Ordner `web/`.
Die GitHub-Actions-Datei in diesem Paket kompiliert die Firmware automatisch und
erzeugt ein kombiniertes Image `minecraft-monitor-firmware.bin`.

Nach Upload auf GitHub:
1. Actions aktivieren.
2. Workflow `Build firmware` ausführen.
3. erzeugtes Artifact laden und `minecraft-monitor-firmware.bin` nach `web/firmware.bin` kopieren.
4. GitHub Pages für `/web` veröffentlichen.
5. Installer in Chrome/Edge öffnen und USB-Board flashen.

## Board-Einstellungen
Arduino-ESP32 3.3.10
ESP32S3 Dev Module
Flash 16 MB
PSRAM OPI
Partition: app3M_fat9M_16MB
