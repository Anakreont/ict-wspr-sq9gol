# ICT WSPR pico balloon tracker – SQ9GOL edition

Firmware for the **ICT WSPR** pico balloon tracker (ATmega328P @ 2 MHz, Si5351A, 26 MHz TCXO, ATGM336H GPS, solar powered, ~1 g board),
with a few improvements by **SQ9GOL** (Piotr, Poland), a tool that sets any Traquito channel, and a command-line build that produces a `.hex`.

## Credits – the original authors

This is **not** an original design. All the hard work was done by:

- **KD2NDR** (Miami, Florida, 2018) – original *HABalloon* WSPR tracker code for Arduino.
- **YO3ICT** (Romania, 2019) – improvements and the **ICT** tracker; please consider supporting the developer: https://yo3ict.uus.ro (“Show Sponsors”).
- **Mike, SA6BSS** (Sweden, 2020–2026) – years of development, flights all over the world, the v361-xs PCB, JLCPCB BOM/CPL files,
  telemetry, schedule and 180° TX work: **https://github.com/micked99/ICT-Wspr-complete-setup**.
  SQ9GOL's balloons would not exist without Mike's help.
- **Douglas Malnati (Traquito)** – the channel map that `kanal.py` re-implements and the Extended Telemetry format used by `ExtTelem.h`
  (https://traquito.github.io, library https://github.com/traquito/WsprEncoded).
- **U4B telemetry format** – QRP Labs.
- Libraries (downloaded by `build.sh`, not included): **JTEncode** (Jason Milldrum NT7S), **TinyGPS++** (Mikal Hart), **Time** (Michael Margolis / Paul Stoffregen),
  Arduino AVR core.

Original licence note (kept in the code): *“You may use and modify the following code to suit your needs so long as it remains open source
and it is for non-commercial use only.”* The same applies to everything in this repository.

Hardware, PCB files, fuses, Arduino `boards.txt` and full build instructions: see Mike's repository.

## What SQ9GOL changed (`sq9gol-changes.diff`, against Mike's "version 2.2" from the ICT_WSPR_m release, 1 Nov 2026)

1. **Real supply voltage in telemetry** – VCC measured against the internal 1.1 V bandgap (on v361-xs the solar cells feed the MCU directly,
   no regulator), instead of the fixed 3.51 V. U4B range is 3.00–4.95 V, lower values show as 3.00 V.
2. **Clock set only from fresh GPS time** – from an RMC sentence with a real date (year 2024–2079, and since v2.3.4 only together with a fresh position fix – without a fix the ATGM336 sends the default date 06.01.80, which TinyGPS++ reads as 2080) and age < 0.5 s.
   The old code re-synced every minute from the last stored time, and TinyGPS++ stores an empty time field as 00:00:00.
3. **GPS watchdog** – cold start of the ATGM336 (`$PCAS10,2`) when there is no new fix for 20 min (e.g. after GPS spoofing),
   at most every 30 min.
4. ADC switched off after measurements; startup CW on the tracker's own band (the original had 12 m hard-coded).
5. New settings at the top of the `.ino` (defaults change nothing): `TCXO_CORR_HZ`, `TEMP_OFFSET_C`, `BANDGAP_MV`,
   `MIN_TX_MV` (skip TX below a voltage, off by default), `CW_AT_START`.
6. **(v2) Traquito Extended Telemetry (User-Defined)** in the +4 min slot (header slot 2 = “slot 3” in the Traquito dashboard),
   instead of a repeat of the standard message. Encoder in `ExtTelem.h`, no extra hardware, no extra transmissions.
   Cross-checked with the official Traquito decoder (WsprEncoded) on 20 000 random cases – 0 errors.
7. **(v2) Basic telemetry flag bits as in Traquito/U4B:** lowest bit always 1 (“basic telemetry”; the ICT code used it for
   “sats ≥ 5”, so with 0 Traquito read the frame as extended), next bit = GPS fix younger than 15 min.
   Voltage and temperature stay in the classic QRP Labs/ICT formula, so **the Traquito dashboard shows the basic-telemetry
   voltage 1.00 V too high** – the real voltage is in the extended field `VccTx`.

### Extended telemetry fields (v2)

| field | range, step | meaning |
|---|---|---|
| `VccTx` | 1.8–4.2 V, 0.05 | lowest supply voltage measured during TX (symbol 80) in the last 10 min – also below 3 V |
| `Sats` | 0–15 | satellites in use (0 = fix older than 15 min) |
| `VSpeed` | −4…+4 m/s, 0.25 | vertical speed from GPS altitude between consecutive frames (10 min) |
| `BootFixMin` | 0–30 min | minutes from power-up (sunrise) to the first GPS fix; 30 = 30 or more / none yet |
| `Resets` | 0–31 | MCU start counter (EEPROM byte 0, modulo 32; flashing clears it) |
| `ResetCause` | 0–3 | 0 power-on, 1 brown-out (BOD 1.8 V), 2 watchdog, 3 external reset |
| `SpoofTime` | 0/1 | GPS time jumped > 3 s against the tracker clock (possible GPS spoofing) |
| `SpoofPos` | 0/1 | position moved faster than 400 km/h (+50 km) or altitude jumped > 3 km between frames |

Traquito dashboard definition (User-Defined, slot 3):
```
{ "name": "VccTx",      "unit": "Volts",   "lowValue": 1.8, "highValue": 4.2, "stepSize": 0.05 },
{ "name": "Sats",       "unit": "Count",   "lowValue": 0,   "highValue": 15,  "stepSize": 1 },
{ "name": "VSpeed",     "unit": "MPS",     "lowValue": -4,  "highValue": 4,   "stepSize": 0.25 },
{ "name": "BootFixMin", "unit": "Minutes", "lowValue": 0,   "highValue": 30,  "stepSize": 1 },
{ "name": "Resets",     "unit": "Count",   "lowValue": 0,   "highValue": 31,  "stepSize": 1 },
{ "name": "ResetCause", "unit": "Enum",    "lowValue": 0,   "highValue": 3,   "stepSize": 1 },
{ "name": "SpoofTime",  "unit": "Bool",    "lowValue": 0,   "highValue": 1,   "stepSize": 1 },
{ "name": "SpoofPos",   "unit": "Bool",    "lowValue": 0,   "highValue": 1,   "stepSize": 1 },
```

8. **180° mode** (`use180deg`, only on Mike's ict_180°-xs-V2 board): CLK1 inverted with the CLK1_INV bit instead of
   `PHOFF = outdivider` (which gives 90°; confirmed by Mike, who now uses `2 * outdivider`). Inversion gives exactly 180° on every band.
   No effect on v361-xs boards (the 15 m channel 540 `.hex` is byte-for-byte unchanged).

9. **(v2.3.1) 10 ms delay after powering the Si5351** in `rf_on()` – without it the test board sent nothing at all (no CW, no WSPR).
10. **(v2.3.2) `ResetCause`:** PORF takes priority (at power-up the MCU often sets PORF and BORF together); `SpoofTime` ignores
    a 17–19 s jump (after a cold start the GPS gives GPS time = UTC + 18 s, then switches to UTC).
11. **(v2.3.3) `wdt_reset()` in the GPS read loop**, max. 64 characters per pass; `$PCAS04,1` again after every `gps_on()`.
12. **(v2.3.4) Stack overflow fixed – the real cause of the restarts** (watchdog reset every 10 min, always after the 3rd TX in a row,
    clock jumps). Reproduced to the minute in the simavr simulator (`test/sim_gps.c`): with `-flto`, `main()` reserved 467 B and
    `JTEncode::wspr_encode` 349 B; the stack went 111 B below the end of the variables and overwrote `timer0_millis` and `Serial` →
    clock jump (SpoofTime=1), or, when `millis()` went backwards, the TimeLib `now()` catch-up loop ran > 8 s → watchdog.
    Fix: `tx_buffer[WSPR_SYMBOL_COUNT]` (162 B instead of 255), Si5351 written with a small TWI routine with timeouts instead of
    Wire (−160 B RAM; same bus settings as before: TWBR 0, no internal pull-ups), `noinline` on the data-preparation functions and
    `setModeWSPR*`. Static RAM 1266 → 993 B, 624 B of stack left in the simulation. Bench test 1 Nov 2026: 12 frames in a row, Resets 0.
13. **(v2.3.4) `kanal.py --temp-offset`** sets `TEMP_OFFSET_C` per board. The ATmega328 internal sensor can be ~10 °C off (one test board read 11 °C
    too high); without an offset it can hit the 39 °C top of the telemetry range. Measure the chip with a thermometer and compare.

Not changed yet: tone changes without a PLL reset on every symbol (Mike is testing which is better),
symbol-timing calibration from GPS, hardware sensors (MS5607 pressure, NTC air temperature).
No Si5351 warm-up needed: the TCXO is powered all the time and the Si5351 does not drift (Mike, 31 Oct 2026).
In flight (cold) the frequency is about 3 Hz lower than at room temperature – measure `TCXO_CORR_HZ` on the ground.
**Status:** bench-tested (no restarts), not flight-tested yet. Flash 22.9 kB of 30 kB, RAM 0.99 kB of 2 kB. See `CHANGELOG.md`.

## Repository layout

| path | contents |
|---|---|
| `base/ICT_WSPR_sq9gol_v2_3_4/` | **the one current version** (older ones in the git history): Mike's firmware + SQ9GOL changes (incl. `ExtTelem.h`), no channel set (Mike's defaults) |
| `kanal.py` | sets callsign, band and Traquito channel in a sketch (id13, start minute, frequency, TX slots, CW) |
| `build.sh` | builds a `.hex` on Linux without the Arduino IDE (fetches avr-gcc 14.1, Arduino AVR core 1.8.6 and Mike's libraries into `.narzedzia/`) |
| `test/sim_gps.c` | simavr simulation of the tracker with a fake GPS: watchdog resets, lowest stack, clock at every transmission |
| `sq9gol-changes.diff` | the changes, to apply on future versions of Mike's firmware |
| `CHANGELOG.md` | version history |

## File names carry the version

Every sketch and `.hex` has the version in its name so they never get mixed up: `ICT_WSPR_<band>_ch<channel>_v2_3_4`
(SQ9GOL firmware, version from `#define FW_VERSION` in the base sketch) and `..._mike_20261023` (Mike's code of that date,
`kanal.py --tag mike_20261023`). The base folder is versioned too: `base/ICT_WSPR_sq9gol_v2_3_4/`.

## Make your own tracker

```
python3 kanal.py 10m 138                                              # show channel parameters
python3 kanal.py 10m 138 --src base/ICT_WSPR_sq9gol_v2_3_4 --out builds/10m_ch138 --call YOURCALL [--temp-offset N]
./build.sh builds/10m_ch138/ICT_WSPR_10m_ch138_v2_3_4                      # -> .hex next to the sketch
```

Reserve your channel on the Traquito Channel Map first. Flash with a USBasp (AVRDUDESS: Flash → `.hex`; fuses `lfuse=FD hfuse=D9 efuse=FE`)
or open the `.ino` in Arduino IDE 1.8.x with Mike's `boards.txt` (“rAVA Low Power (1.8V, 2 MHz) w/ ATmega328”).
Disconnect the USBasp RST wire after flashing. Test on the ground before any flight – and after any code change run `test/sim_gps.c` (0 resets, stack margin): the ATmega328 has only 2 kB of RAM.

Schedule (4 TX per 10 min): standard message in the channel's start minute, basic telemetry +2 min,
extended telemetry +4 min, repeat of the standard message +6 min, +8 min GPS only.

## Updating to a new version of Mike's firmware

Download the individual files of his new `ICT_WSPR_m` release (not the "Source code" archive), rename `ICT_WSPR_m.ino` to `ICT_WSPR.ino`, run `patch -p1 -d <copy> < sq9gol-changes.diff`,
save as the new `base/`, then `kanal.py` + `build.sh` for each tracker.

73 de SQ9GOL – https://sq9gol.xyz
