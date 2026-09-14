# Changelog

Historia wersji firmware `SW102_BAF_X.Y.Z`, wgrywanych na prawdziwy sprzęt przez SWD.

## 0.0.3 (2026-09-14)

- Naprawa błędu światła: komenda światła wysyłana bezwarunkowo w każdym cyklu odpytywania (zgodnie z zachowaniem sprawdzonym w EggSPEED), zamiast tylko przy zmianie stanu - poprzednia wersja powodowała, że lampa mrugała raz i gasła
- Nowa funkcja "Trip reset" w menu (reset dystansu/czasu/prędkości trip oraz średniego zużycia energii trip, z zachowaniem długoterminowych uśrednionych danych)
- Przesunięcia pozycji wskazań AV/AC (+1px w prawo) i etykiety km/h (2px w górę)

## 0.0.2 (2026-09-14)

- Pełna konwersja jednostek km/h↔mph: prędkość, dystans (ODO/trip), zużycie energii (Wh/km↔Wh/mile) przeliczane na granicy wyświetlania, dane wewnętrzne zawsze w km/h/Wh na km
- Nowa czcionka `font_il` (samodzielna, tylko znaki "i"/"l") do etykiet "ml"/"Wml"
- Nowa ikona-znacznik menu (`icon_menu_marker.xbm`) zastępująca podświetlenie prostokątem + odwrócony tekst - usunięcie glitcha nakładającego się tekstu
- Poprawka `numeric2string()`: brakujące zera wiodące w wartościach dziesiętnych (np. "1,1" → "1,01")
- Limit prędkości i edytor ODO w menu pozostają celowo w km (ograniczenie frameworku edytora liczbowego, brak przełączalnej jednostki w czasie działania)

## 0.0.1 (2026-09-14)

- Pierwsza wersja z numerem i ekranem startowym pokazującym pełną nazwę firmware
- Poprawka kalibracji napięcia: rzeczywisty dzielnik napięcia P+ ustalony na ~402 kΩ (zamiast założonych przez fork 300 kΩ), zweryfikowane multimetrem na sprzęcie (58,8 V rzeczywiste vs 44,4 V wyświetlane przed poprawką); dodane pole "Voltage cal." w menu
- Poprawka przepełnienia `uint8_t` w formule kontrastu/jasności LCD (`lcd_refresh()`) - podświetlenie renderowało się binarnie zamiast płynnie
- Nowa pozycja menu "Screen test" (wypełnienie ekranu na biało)
- Dodane "Dev Tomasz Pieczara" i "spotrobotics.app" w menu Info
- Zakończenie zakładki menu "Cockpit" (przełączniki ON/OFF separatorów i paska PAS)
- Pasek poziomu wspomagania (PAS) zmieniony na styl wykresu słupkowego (wypełnienie 0..aktywny poziom)
