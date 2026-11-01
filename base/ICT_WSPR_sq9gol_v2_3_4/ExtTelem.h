/*
   ExtTelem.h - SQ9GOL: telemetria rozszerzona w formacie Traquito "Extended Telemetry / User-Defined"
   (zgodna z biblioteka WsprEncoded, Douglas Malnati: https://github.com/traquito/WsprEncoded).
   Bez zaleznosci od Arduino - ten sam plik kompiluje sie na PC do testu z dekoderem Traquito.

   Pola (kolejnosc definicji = kolejnosc w opisie dla Traquito Dashboard):
     VccTx      1.8 .. 4.2 V, krok 0.05   najnizsze napiecie zasilania zmierzone w trakcie nadawania (10 min)
     Sats       0 .. 15, krok 1           liczba satelitow (0 = brak swiezego fixu)
     VSpeed    -4 .. 4 m/s, krok 0.25     predkosc pionowa z wysokosci GPS co 10 min
     BootFixMin 0 .. 30 min, krok 1       ile minut od wlaczenia do pierwszego fixu GPS (30 = 30 lub wiecej)
     Resets     0 .. 31, krok 1           licznik startow procesora (EEPROM, modulo 32)
     ResetCause 0 .. 3                    przyczyna ostatniego startu: 0 wlaczenie zasilania, 1 spadek napiecia (BOD),
                                          2 watchdog, 3 reset zewnetrzny
     SpoofTime  0 .. 1                    czas z GPS skoczyl wzgledem zegara trackera o > 3 s (ostatnie 10 min)
     SpoofPos   0 .. 1                    pozycja/wysokosc skoczyla nierealnie dla balonu (ostatnie 10 min)
   Naglowek: HdrTelemetryType 0 (extended), HdrRESERVED 0, HdrType 0 (user-defined), HdrSlot = numer slotu 0-4
   (0 = ramka standardowa, 1 = telemetria podstawowa, 2 = +4 min, 3 = +6 min, 4 = +8 min).
*/
#pragma once
#include <stdint.h>

struct ExtTelemData {
  uint16_t vccTx_mV;
  uint8_t  sats;
  int16_t  vspeed_cms;     // cm/s
  uint8_t  bootFixMin;
  uint8_t  resets;
  uint8_t  resetCause;
  uint8_t  spoofTime;
  uint8_t  spoofPos;
};

static const uint8_t EXT_POWER_DBM[19] = {0, 3, 7, 10, 13, 17, 20, 23, 27, 30, 33, 37, 40, 43, 47, 50, 53, 57, 60};

static int32_t extClamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

// numer wartosci pola = round((wartosc - low) / krok), wartosc wczesniej przycieta do zakresu (jak WsprEncoded)
static uint32_t extFieldNum(int32_t value, int32_t low, int32_t high, int32_t step)
{
  value = extClamp(value, low, high);
  return (uint32_t)((value - low + step / 2) / step);
}

// koduje dane do (callsign[7], grid4[5], dbm); id13 = 2 znaki kanalu, hdrSlot = 0..4
#ifdef __AVR__
__attribute__((noinline))   // SQ9GOL v2.3.4: tablice nv/num nie zajmuja stosu w czasie wspr_encode
#endif
static void extTelemEncode(const ExtTelemData &d, const char id13[2], uint8_t hdrSlot,
                           char callsign[7], char grid4[5], uint8_t &dbm)
{
  // pola uzytkownika: wartosci w jednostkach calkowitych (mV, cm/s), liczby wartosci jak w definicji
  const uint32_t nv[8]  = {49, 16, 33, 31, 32, 4, 2, 2};
  uint32_t num[8];
  num[0] = extFieldNum(d.vccTx_mV, 1800, 4200, 50);
  num[1] = extFieldNum(d.sats, 0, 15, 1);
  num[2] = extFieldNum(d.vspeed_cms, -400, 400, 25);
  num[3] = extFieldNum(d.bootFixMin, 0, 30, 1);
  num[4] = extFieldNum(d.resets % 32, 0, 31, 1);
  num[5] = extFieldNum(d.resetCause, 0, 3, 1);
  num[6] = extFieldNum(d.spoofTime ? 1 : 0, 0, 1, 1);
  num[7] = extFieldNum(d.spoofPos ? 1 : 0, 0, 1, 1);

  uint64_t v = 0;
  for (int8_t i = 7; i >= 0; --i) { v *= nv[i]; v += num[i]; }   // pola uzytkownika od konca
  v *= 5;  v += hdrSlot;                                          // HdrSlot
  v *= 16; v += 0;                                                // HdrType = user-defined
  v *= 4;  v += 0;                                                // HdrRESERVED
  v *= 2;  v += 0;                                                // HdrTelemetryType = extended

  dbm = EXT_POWER_DBM[v % 19]; v /= 19;
  uint8_t g4 = v % 10; v /= 10;
  uint8_t g3 = v % 10; v /= 10;
  uint8_t g2 = v % 18; v /= 18;
  uint8_t g1 = v % 18; v /= 18;
  grid4[0] = 'A' + g1; grid4[1] = 'A' + g2; grid4[2] = '0' + g3; grid4[3] = '0' + g4; grid4[4] = 0;
  uint8_t id6 = v % 26; v /= 26;
  uint8_t id5 = v % 26; v /= 26;
  uint8_t id4 = v % 26; v /= 26;
  uint8_t id2 = v % 36;
  callsign[0] = id13[0];
  callsign[1] = id2 < 10 ? '0' + id2 : 'A' + (id2 - 10);
  callsign[2] = id13[1];
  callsign[3] = 'A' + id4; callsign[4] = 'A' + id5; callsign[5] = 'A' + id6; callsign[6] = 0;
}
