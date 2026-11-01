#!/usr/bin/env python3
"""kanal.py - przygotowuje szkic ICT WSPR (v361-xs) dla wybranego pasma i kanału Traquito.

Użycie:
  python3 kanal.py 15m 540                       # tylko pokaż parametry kanału
  python3 kanal.py 15m 540 --src SZKIC --out KATALOG --call YOURCALL

SZKIC = katalog ze szkicem (np. ICT_WSPR_m z repo Mike'a albo nasz poprawiony szkic).
Powstaje KATALOG/ICT_WSPR_<pasmo>_ch<kanał>/ z ustawionymi: znakiem, id13, częstotliwością,
slotami w Timing.h, CW na pasmie trackera. Kompilacja: Arduino IDE albo build.sh.

Wzór kanału = Traquito Channel Map (picoinf WSPR.h), sprawdzony z kanałami 71/15 m, 82/10 m, 254/17 m.
"""
import argparse, re, shutil, sys
from pathlib import Path

DIAL = {"20m": 14_095_600, "17m": 18_104_600, "15m": 21_094_600, "12m": 24_924_600, "10m": 28_124_600}
MIN_OFF = {"20m": 8, "17m": 2, "15m": 6, "12m": 0, "10m": 4}   # minuta startu kanału 0 na paśmie
CW = {**{c: f"cw_{c.lower()}();" for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ"}, **{d: f"cw_{d}();" for d in "0123456789"}}

def channel(band, ch):
    if band not in DIAL: sys.exit(f"nieznane pasmo {band} (znam: {', '.join(DIAL)})")
    if not 0 <= ch <= 599: sys.exit("kanał 0-599")
    row = ch % 20
    lane = row // 5 + 1                                   # pasy 1,2,(środek pominięty),3,4
    band_idx = [1, 2, 4, 5][lane - 1]
    freq = DIAL[band] + 1400 + (band_idx - 1) * 40 + 20
    return {"id13": "01Q"[ch // 200] + str((ch // 20) % 10), "start": (2 * (ch % 5) + MIN_OFF[band]) % 10,
            "lane": lane, "freq": freq}

def sub(text, pattern, repl, what):
    new, n = re.subn(pattern, repl, text, count=1, flags=re.M)
    if n != 1: sys.exit(f"nie znalazłem w szkicu: {what}")
    return new

def main():
    a = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    a.add_argument("band"); a.add_argument("channel", type=int)
    a.add_argument("--src"); a.add_argument("--out"); a.add_argument("--call", help="your callsign (required with --src)")
    a.add_argument("--tag", help="końcówka nazwy zamiast wersji z FW_VERSION, np. mike_20261023")
    a.add_argument("--temp-offset", type=int, help="TEMP_OFFSET_C dla tej płytki [°C] (pomiar termometrem na procesorze)")
    o = a.parse_args()
    p = channel(o.band, o.channel)
    s, t = p["start"], p["id13"]
    slots = [s, (s + 2) % 10, (s + 4) % 10, (s + 6) % 10]
    print(f"{o.band} kanał {o.channel}: id13 {t}, ramka :x{s}, telemetria :x{slots[1]}, rozszerzona :x{slots[2]}, pas {p['lane']}, {p['freq']:,} Hz".replace(",", " "))
    if not o.src: return
    if not o.call: sys.exit("podaj --call ZNAK (your callsign)")
    src = Path(o.src); ino = next(src.glob("*.ino"))
    x0 = ino.read_text(encoding="utf-8")
    ver = re.search(r'^#define FW_VERSION "([^"]+)"', x0, re.M)
    # wersja w nazwie (31.10.2026): nasz firmware -> _v2_1, kod Mike'a bez FW_VERSION -> _mike (kropka niedozwolona w nazwie szkicu)
    tag = o.tag or (("v" + ver.group(1).replace(".", "_")) if ver else "mike")
    name = f"ICT_WSPR_{o.band}_ch{o.channel}_{tag}"
    dst = Path(o.out) / name
    if dst.exists(): sys.exit(f"{dst} już istnieje")
    dst.mkdir(parents=True)
    for f in src.glob("*.h"): shutil.copy(f, dst / f.name)
    x = ino.read_text(encoding="utf-8")
    x = re.sub(r"^(#define WSPR_FREQ\s)", r"//\1", x, flags=re.M)                       # wyłącz wszystkie pasma
    x = sub(x, r"^#define ISM_WSPR_FREQ", f"#define WSPR_FREQ     {p['freq']}UL // {o.band}, Traquito kanał {o.channel} (pas {p['lane']})\n#define ISM_WSPR_FREQ", "ISM_WSPR_FREQ")
    x = sub(x, r'^const char call\[\] = "[^"]*";.*$', f'const char call[] = "{o.call}";      // WSPR Standard callsign', "call[]")
    x = sub(x, r"^char tlm_channel1 = '.';.*$", f"char tlm_channel1 = '{t[0]}';           // Telemetry channel: 0/1/Q  (kanał {o.channel} -> id13 {t})", "tlm_channel1")
    x = sub(x, r"^char tlm_channel2 = '.';.*$", f"char tlm_channel2 = '{t[1]}';           // Telemetry channel: 0-9", "tlm_channel2")
    if re.search(r'^(int|const char) CW(\[\])?\s*=\s*"', x, re.M):
        # kod Mike'a od wersji 2.2 (01.11.2026): znak CW jako napis CW + cw_send(); "int CW" Mike'a -> poprawna tablica znaków
        x = sub(x, r'^(int|const char) CW(\[\])?\s*=\s*"[^"]*";.*$',
                f'const char CW[] = "{o.call.upper()}";        // CW call at startup (kanal.py)', "napis CW")
    else:
        cw = " ".join(CW[c] for c in o.call.upper() if c in CW)
        x = sub(x, r"^(\s*)(if \(CW_AT_START\) \{ )?(cw_\w\(\);\s*)+(\} )?//.*$",
                lambda m: f"{m.group(1)}{m.group(2) or ''}{cw} {m.group(4) or ''}// CW id {o.call} at startup", "linia CW")
    if o.temp_offset is not None:
        x = sub(x, r"^const int8_t   TEMP_OFFSET_C = -?\d+;", f"const int8_t   TEMP_OFFSET_C = {o.temp_offset};", "TEMP_OFFSET_C")
    (dst / f"{name}.ino").write_text(x, encoding="utf-8")
    b = (dst / "Beep.h").read_text(encoding="utf-8")
    b = re.sub(r"^(\s*)freq = \d+UL;", r"\1//freq = (stara stała);", b, flags=re.M)        # stałe częstotliwości CW Mike'a
    if "freq = WSPR_FREQ" not in b:
        b = sub(b, r"^(void beep\(int on, int off\) \{)$", r"\1\n     freq = WSPR_FREQ - 150UL;   // CW tuż pod kanałem WSPR, na paśmie trackera", "beep()")
    (dst / "Beep.h").write_text(b, encoding="utf-8")
    tm = (dst / "Timing.h").read_text(encoding="utf-8")
    tm = sub(tm, r"^const uint8_t TX_SLOTS\s*=.*$", "const uint8_t TX_SLOTS  = " + " | ".join(f"SLOT({m})" for m in slots)
             + f";  // kanał {o.channel}: ramka :x{s}, telemetria :x{slots[1]}, :x{slots[2]} rozszerzona (lub powtórka), :x{slots[3]} powtórka, :x{(s + 8) % 10} tylko GPS", "TX_SLOTS")
    tm = sub(tm, r"^const uint8_t TLM_SLOT\s*=\s*\d+;", f"const uint8_t TLM_SLOT  = {slots[1]};", "TLM_SLOT")
    tm = sub(tm, r"^const uint8_t PREP_SLOT\s*=\s*\d+;", f"const uint8_t PREP_SLOT = {s};", "PREP_SLOT")
    (dst / "Timing.h").write_text(tm, encoding="utf-8")
    print("szkic:", dst)

if __name__ == "__main__":
    main()
