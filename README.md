# SW102 Firmware

Własny firmware na wyświetlacz Bafang SW102, docelowo z pełną funkcjonalnością OEM oraz regulowaną kalibracją wyświetlanego prądu/mocy.

To repozytorium zawiera **wyłącznie kod, który sami napisaliśmy** - nie zawiera pełnego forka bazowego (`anszom/SW102_LCD`), z którym pracujemy równolegle lokalnie. Zamiast kopiować cały fork (w większości kod Nordic SDK i UI oryginalnego autora), nasze zmiany w jego plikach są tu jako plik różnicowy (`patches/sw102_lcd.patch`).

## Zawartość

- **`firmware/`** - moduły napisane od zera: protokół Bafang UART (`bafang_protocol.*`), maszyna stanów telemetrii (`bafang_display.*`), kalibracja prądu/napięcia (`bafang_calibration.*`), ustawienia (`bafang_settings.*`), trwały zapis ustawień we flash (`bafang_storage.*`), trip/odo (`bafang_trip.*`), estymator zasięgu/zużycia energii - port EggSPEED's `EnergyAnalyzer.kt` (`bafang_energy.*`), numer wersji firmware (`firmware_version.h`), mosty integracyjne (`*_bridge.h`), oraz cała rodzina ręcznie narysowanych czcionek kokpitu, ikona-znacznik menu i logo ekranu boot (`font_*.xbm`, `icon_*.xbm`, `logo_eggspeed.xbm`)
- **`emu-rs/`** - terminalowy emulator firmware (Rust/ratatui) - kompiluje i uruchamia PRAWDZIWY kod C firmware (nie przybliżenie), renderuje framebuffer jako Braille'a w terminalu, symuluje fałszywy kontroler Bafang albo mostkuje do prawdziwego portu szeregowego (`--serial COM3`). Wymaga MinGW-w64 GCC na PATH (target `x86_64-pc-windows-gnu` - MSVC nie obsługuje składni GCC użytej w firmware) - `cargo build --target x86_64-pc-windows-gnu`
- **`patches/sw102_lcd.patch`** - dokładna różnica względem forka bazowego (`git diff --binary`, zawiera zarówno modyfikacje plików forka jak i dodanie nowych plików/`emu-rs/`)
- **`font_speed_work/`** - skrypty do generowania czcionek kokpitu (ekstrakcja piksel-po-pikselu z ręcznie rysowanych szablonów na siatce) oraz pixel-accurate symulacje w Pythonie (`simulate_cockpit.py`, `simulate_menu.py`) używane do iterowania nad layoutem przed dotknięciem kodu C
- **`research.md`** - pełna dokumentacja techniczna projektu (protokół, sprzęt, decyzje architektoniczne, historia)

## Jak zbudować

1. Sklonuj bazowy fork: `git clone https://github.com/anszom/SW102_LCD.git` (branch `sw102-new`)
2. Zastosuj patch: `git apply /ścieżka/do/patches/sw102_lcd.patch` w katalogu forka
3. Skopiuj `firmware/*.py` z tego repo do odpowiednich katalogów `firmware/SW102/include/` i `firmware/SW102/src/sw102/` forka (jeśli patch nie obejmuje nowych plików w Twojej wersji gita - `git apply` z opcją tworzenia nowych plików powinien to zrobić automatycznie, jeśli patch był generowany z `git diff` po `git add -A`, co obejmuje też nowe pliki)
4. Zbuduj wg instrukcji w `research.md` (arm-none-eabi-gcc, make, OpenOCD)

## Podgląd (symulacja)

<img src="font_speed_work/cockpit_simulation_natural.png" alt="Symulacja kokpitu SW102 - predkosc, moc, poziom wspomagania, trip/odo/range" width="260">

Pixel-accurate symulacja layoutu kokpitu (`font_speed_work/simulate_cockpit.py`) z ręcznie
narysowanymi czcionkami, używana do iterowania nad UI przed dotknięciem kodu C.

## Status

Firmware przetestowany na prawdziwym sprzęcie (flashowanie przez SWD/OpenOCD i ST-Link V2, aktualna wersja `SW102_BAF_0.0.3`). Działające na sprzęcie funkcje: protokół Bafang UART (telemetria, światła, wspomaganie), kalibracja prądu i napięcia, trip/odo z opcją resetu, przełącznik jednostek km/h↔mph, menu z ikoną-znacznikiem zamiast podświetlenia, ekran startowy z numerem wersji. Konwencja wersjonowania: `SW102_BAF_X.Y.Z`, każda wersja wgrana na sprzęt jest zapisywana jako osobny plik `.hex` (nienadpisywany). Buduje się bez błędów na obu toolchainach (ARM oraz emulator). Szczegóły i historia w `research.md` i `CHANGELOG.md`.

## Licencja i pochodzenie

Bazowy fork (`anszom/SW102_LCD`, sam fork `OpenSourceEBike/Color_LCD`) jest na licencji GPL-3.0. Ten projekt jest obecnie prywatny i niedystrybuowany. Przed jakąkolwiek dystrybucją publiczną kod w `patches/` zostanie zastąpiony w pełni niezależną implementacją.
