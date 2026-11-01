#!/bin/bash
# build.sh - kompiluje szkic ICT WSPR do .hex bez Arduino IDE (Linux, np. Dell albo sesja Claude'a).
# Ustawienia jak płytka "rAVA Low Power (1.8V, 2 MHz) w/ ATmega328": atmega328p, F_CPU 2 MHz, core arduino, variant standard.
#
# użycie: ./build.sh <katalog_szkicu>          -> <katalog_szkicu>/<nazwa>.hex
# Przy pierwszym uruchomieniu pobiera do ./.narzedzia (ok. 115 MB, poza repo):
#   - avr-gcc 14.1.0 (github.com/ZakKemble/avr-gcc-build)
#   - rdzeń Arduino AVR 1.8.6 (github.com/arduino/ArduinoCore-avr)
#   - biblioteki JTEncode, Time, TinyGPSPlus z repo Mike'a SA6BSS (github.com/micked99/ICT-Wspr-complete-setup)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SK=$(cd "$1" && pwd); NAME=$(basename "$SK")
[ -f "$SK/$NAME.ino" ] || { echo "brak $SK/$NAME.ino (nazwa pliku .ino = nazwa katalogu)"; exit 1; }
T=$HERE/.narzedzia; mkdir -p "$T"
if [ ! -x "$T/gcc/avr-gcc-14.1.0-x64-linux/bin/avr-gcc" ]; then
  curl -sSL -o "$T/avrgcc.tar.bz2" https://github.com/ZakKemble/avr-gcc-build/releases/download/v14.1.0-1/avr-gcc-14.1.0-x64-linux.tar.bz2
  mkdir -p "$T/gcc" && tar xjf "$T/avrgcc.tar.bz2" -C "$T/gcc" && rm "$T/avrgcc.tar.bz2"
fi
[ -d "$T/core" ] || git clone -q --depth 1 -b 1.8.6 https://github.com/arduino/ArduinoCore-avr "$T/core"
[ -d "$T/mike" ] || git clone -q --depth 1 https://github.com/micked99/ICT-Wspr-complete-setup "$T/mike"
BIN=$T/gcc/avr-gcc-14.1.0-x64-linux/bin; CORE=$T/core; LIBS=$T/mike/Libraries
B=$(mktemp -d)
FL="-mmcu=atmega328p -DF_CPU=2000000L -DARDUINO=10813 -DARDUINO_AVR_ATMEGA328P -DARDUINO_ARCH_AVR -Os -g -flto -ffunction-sections -fdata-sections -w"
INC="-I$CORE/cores/arduino -I$CORE/variants/standard -I$CORE/libraries/Wire/src -I$CORE/libraries/Wire/src/utility -I$LIBS/Time -I$LIBS/TinyGPSPlus/src -I$LIBS/JTEncode/src -I$SK"
CXX="$BIN/avr-g++ $FL -std=gnu++11 -fpermissive -fno-exceptions -fno-threadsafe-statics $INC"
CC="$BIN/avr-gcc $FL -std=gnu11 $INC"
objs=(); n=0
comp() { n=$((n+1)); local o=$B/$n.o
  case "$1" in *.c) $CC -c "$1" -o "$o";; *.S) $BIN/avr-gcc -x assembler-with-cpp $FL $INC -c "$1" -o "$o";; *) $CXX -c "$1" -o "$o";; esac; objs+=("$o"); }
# link Wire only if the sketch includes it (v2.3.4+ writes to the Si5351 with its own TWI code - Wire took 160 B of RAM)
WIRE=""; grep -qs "#include <Wire.h>" "$SK"/*.ino "$SK"/*.h && WIRE="$CORE/libraries/Wire/src/Wire.cpp $CORE/libraries/Wire/src/utility/twi.c"
for f in $CORE/cores/arduino/*.c $CORE/cores/arduino/*.cpp $CORE/cores/arduino/*.S $WIRE \
         $LIBS/Time/Time.cpp $LIBS/TinyGPSPlus/src/TinyGPS++.cpp $LIBS/JTEncode/src/*.cpp $LIBS/JTEncode/src/*.c; do comp "$f"; done
{ echo '#include <Arduino.h>'; echo "#line 1 \"$NAME.ino\""; cat "$SK/$NAME.ino"; } > "$B/sketch.cpp"; comp "$B/sketch.cpp"
$BIN/avr-gcc -w -Os -g -flto -fuse-linker-plugin -Wl,--gc-sections -mmcu=atmega328p -o "$B/$NAME.elf" "${objs[@]}" -lm
$BIN/avr-objcopy -O ihex -R .eeprom "$B/$NAME.elf" "$SK/$NAME.hex"
$BIN/avr-size -A "$B/$NAME.elf" | awk '/^.text|^.data|^.bss/{s[$1]=$2} END{printf "Flash %d B (max 30720), RAM %d B (max 2048)\n", s[".text"]+s[".data"], s[".data"]+s[".bss"]}'
rm -rf "$B"; echo "HEX: $SK/$NAME.hex"
