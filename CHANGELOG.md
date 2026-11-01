# Changelog

Only the current version is kept in the repository. No ready-made builds: run `kanal.py` with your own callsign and channel.

## v2.3.4 – 1 Nov 2026 (current)
- **Stack overflow fixed** – the real cause of the watchdog restarts (every 10 min, right after the 3rd transmission in a row) and of the
  clock jumps (`SpoofTime=1`). Found with the extended-telemetry diagnostic field and reproduced in simavr (`test/sim_gps.c`).
  `tx_buffer` 162 B instead of 255, own TWI write to the Si5351 with timeouts (no Wire library, −160 B RAM),
  `noinline` on the data-preparation functions and `setModeWSPR*` (LTO had merged their locals into one 467 B frame of `main()`).
  Static RAM 1266 → 992 B; Flash 22.9 kB. `build.sh` links Wire only if the sketch includes it.
- GPS time accepted only with a fresh position fix and year < 2080 (the ATGM336's default date 06.01.80 became "2080" and set the clock to 00:00).
- `kanal.py --temp-offset` for the per-board `TEMP_OFFSET_C` (one test board: −11 °C).
- Bench test: 12 frames in a row without a gap, Resets 0, SpoofTime 0.

## v2.3.3 – 1 Nov 2026
- `wdt_reset()` back in the GPS read loop (removed in Mike 2.2), max. 64 characters per pass, `$PCAS04,1` after every `gps_on()`. Did not stop the restarts (see v2.3.4).

## v2.3.2 – 1 Nov 2026
- `ResetCause`: PORF first (power-up sets PORF and BORF together → false "brown-out"); `SpoofTime` ignores the 17–19 s GPS→UTC leap-second jump.

## v2.3.1 – 1 Nov 2026
- 10 ms delay after powering the Si5351 in `rf_on()` – v2.3 and Mike's plain 2.2 sent nothing on the test board.

## v2.3 – 1 Nov 2026
- SQ9GOL changes moved onto Mike's **"version 2.2"** (files from the ICT_WSPR_m release – download the individual files, the "Source code" archive still holds the old code).
- From Mike 2.2: startup CW from a string (`CW` + `cw_send()`), GPS UART TX pin driven LOW while the GPS is off, direct port access for RF/GPS power, `wdt_enable(WDTO_8S)`, no separate test files (CW at start, WSPR at the next even minute after a fix).
- `CW` declared as `const char CW[]` (Mike's `int CW = "..."` only compiles with `-fpermissive`); `kanal.py` sets the CW callsign in both old and new style sketches.
- Kept from SQ9GOL: CLK1_INV for 180° (instead of PHOFF = 2 × outdivider), clock only from fresh RMC time (Mike's 2.2 sets the clock from GPS every minute again).
- Diff now against Mike 2.2. Flash 27.0 kB of 30 kB.

## v2.1 – 31 Oct 2026
- 180° mode: CLK1_INV inversion instead of phase offset = outdivider (90°). v361-xs builds unchanged.
- Version in every sketch / `.hex` name (`_v2_1`, `_mike_20261023`), `#define FW_VERSION`, `kanal.py --tag`.
- Notes from Mike SA6BSS: no warm-up needed (TCXO always on), in-flight frequency ~3 Hz lower than on the bench.

## v2 – 31 Oct 2026
- Traquito Extended Telemetry (User-Defined) in the +4 min slot: `VccTx`, `Sats`, `VSpeed`, `BootFixMin`, `Resets`,
  `ResetCause`, `SpoofTime`, `SpoofPos` (new file `ExtTelem.h`; verified against the official WsprEncoded decoder).
- Reset cause kept from MCUSR before it is cleared (`.noinit`), start counter in EEPROM.
- Supply voltage also measured in the middle of every transmission (under load); minimum reported.
- Spoofing checks: GPS time vs tracker clock (> 3 s), impossible position/altitude jumps.
- Basic telemetry: lowest bit always 1 (Traquito “basic” marker), GPS bit = fix younger than 15 min.
- `kanal.py`: schedule comment shows the extended slot.

## v1 – 31 Oct 2026
- Real supply voltage (internal bandgap) instead of fixed 3.51 V.
- Clock set only from fresh RMC time with a real date.
- GPS cold-start watchdog (`$PCAS10,2`) after 20 min without a fix.
- ADC off after use; startup CW on the tracker band.
- Settings: `TCXO_CORR_HZ`, `TEMP_OFFSET_C`, `BANDGAP_MV`, `MIN_TX_MV`, `CW_AT_START`.
- `kanal.py` (Traquito channel → sketch settings) and `build.sh` (command-line build).

Base: Mike SA6BSS, https://github.com/micked99/ICT-Wspr-complete-setup, commit `d4a305e` (23 Oct 2026).
