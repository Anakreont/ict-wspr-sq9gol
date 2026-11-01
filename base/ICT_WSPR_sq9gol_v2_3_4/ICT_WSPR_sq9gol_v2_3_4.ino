/*
   HABalloon by KD2NDR, Miami Florida October 25 2018
   Improvements by YO3ICT, Romania, April-May 2019 
   Improvements By SA6BSS, Sweden, 2020-2026
   Changes by SQ9GOL, Poland, 2026: real VCC telemetry, fresh GPS time sync, GPS cold-start
   watchdog, CW on the tracker band, Traquito extended telemetry (see sq9gol-changes.diff)
   You may use and modify the following code to suit
   your needs so long as it remains open source
   and it is for non-commercial use only.
*/

#define FW_VERSION "2.3.4"           // SQ9GOL: wersja firmware - kanal.py dopisuje ja do nazwy szkicu i pliku .hex

#include <avr/wdt.h>
#include <TimeLib.h>
#include <TinyGPS++.h>
#include <JTEncode.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <avr/eeprom.h>
#include "ExtTelem.h"            // SQ9GOL: telemetria rozszerzona Traquito (User-Defined)

//#define WSPR_FREQ     3570185UL  // <<<<< SET TX FREQUENCY HERE
//#define WSPR_FREQ     7040185UL  // <<<<< SET TX FREQUENCY HERE
//#define WSPR_FREQ     10140285UL // <<<<< SET TX FREQUENCY HERE
//#define WSPR_FREQ     14097190UL // <<<<< SET TX FREQUENCY HERE
//#define WSPR_FREQ     18106200UL // <<<<< SET TX FREQUENCY HERE
//#define WSPR_FREQ     21096190UL // <<<<< SET TX FREQUENCY HERE  
#define WSPR_FREQ     24926055UL // <<<<< SET TX FREQUENCY HERE       
//#define WSPR_FREQ     28126100UL // <<<<< SET TX FREQUENCY HERE
#define ISM_WSPR_FREQ   13555425UL // = 1,500Hz audio on 13.553.900 Mhz // 13555425UL New correct ism freq
#define WSPR_CTC        1332       // WSPR CTC - 2668 @ 4Mhz //1334 @ 2Mhz //667 @ 1Mhz

// Enumerations
TinyGPSPlus gps;
JTEncode jtencode;

// Global variables
unsigned long freq;
const char call[] = "SA7BSS";      // WSPR Standard callsign -- SA7BSS for bench testing, SA6BSS for flight
char tlm_channel1 = '1';           // Telemetry channel: 0/1/Q
char tlm_channel2 = '1';           // Telemetry channel: 0-9
const int32_t  TCXO_CORR_HZ = 0;    // SQ9GOL: poprawka TCXO w Hz (zmierzona na ziemi z raportow WSPR); 0 = bez poprawki
const uint32_t tcxo  = 26000000UL + TCXO_CORR_HZ; // TCXO/crystal frequency in Hz
const int8_t   TEMP_OFFSET_C = 0;   // SQ9GOL: poprawka czujnika temperatury w procesorze (pomiar na ziemi); 0 = bez poprawki
const uint16_t BANDGAP_MV = 1100;   // SQ9GOL: napiecie wzorca 1,1 V tego egzemplarza (1000-1200); kalibracja pomiaru zasilania
const uint16_t MIN_TX_MV = 0;       // SQ9GOL: nie nadawaj ponizej tego napiecia [mV], np. 2900; 0 = wylaczone (domyslnie)
const bool     CW_AT_START = true;  // SQ9GOL: CW ze znakiem po wlaczeniu (false = bez CW, oszczednie o swicie)
const bool use180deg = false;      // Hardware config: true = 180° dipole (CLK0+CLK1), false = standard/ground-wire (CLK0)
const char CW[] = "SA6BSS";        // CW call at startup (SQ9GOL: tablica znakow zamiast int - kanal.py wpisuje znak)
// ↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑ // Edit above settings to match your setup 
char call_telemetry[7];            // WSPR telemetry callsign
char loc_telemetry[5];             // WSPR telemetry locator
char TestGrid[5];                  // Testing grid for Geofence
uint8_t dbm, dbm_telemetry;        // Standard WSPR dbm - coarse altitude & WSPR telemetry dbm
char loc4[5];                      // 4 digit gridsquare locator
long lat, lon, oldlat, oldlon;     // Used for location
uint8_t tx_buffer[WSPR_SYMBOL_COUNT]; // WSPR Tx buffer (SQ9GOL 1.11.2026: 162 zamiast 255 - brakowalo RAM na stos, patrz TelemFunctions.h)
uint8_t symbol_count;              // JTencode
uint16_t tone_spacing, CTC;        // JTencode
int alt_meters = 0;
int telemetry_set = 0;
int Sats = 0;
int gps_speed = 0;
volatile bool proceed = false;
int w=1; 
bool Outside = true;              // Geofence flag
uint16_t vcc_mv = 0;             // SQ9GOL: zmierzone napiecie zasilania [mV]
unsigned long lastGpsReset = 0;  // SQ9GOL: millis() ostatniego zimnego resetu GPS
bool gpsResetDone = false;
// SQ9GOL: dane do telemetrii rozszerzonej (slot +4 min)
uint8_t mcusr_mirror __attribute__ ((section (".noinit")));   // MCUSR zapisany przed wyczyszczeniem
uint8_t resetCause = 0;          // 0 zasilanie, 1 spadek napiecia (BOD), 2 watchdog, 3 reset zewnetrzny
uint8_t resetCount = 0;          // licznik startow (EEPROM adres 0)
unsigned long bootFixMs = 0;     // millis() pierwszego fixu po starcie (0 = jeszcze nie)
uint16_t vccTxMin = 0xFFFF;      // najnizsze napiecie w trakcie nadawania od ostatniej ramki rozszerzonej [mV]
int16_t vspeed_cms = 0;          // predkosc pionowa [cm/s]
bool havePrev = false; long prevAlt = 0; unsigned long prevFixMs = 0; float prevLat = 0, prevLon = 0;
uint8_t spoofTimeFlag = 0, spoofPosFlag = 0;
const uint8_t EXT_HDR_SLOT = 2;  // numer slotu w naglowku Traquito: 2 = ramka +4 min od startu kanalu
//GeoFence grids by Matt Downs - 2E1GYP // Airborne transmissions of this sort are not legal over North Korea and Yemen
const char NoTXGrids[] PROGMEM = {"PN31 PN41 PN20 PN30 PN40 PM29 PM39 PM28 PM38 LK16 LK15 LK14 LK13 LK23 LK24 LK25 LK26 LK36 LK35 LK34 LK33 LK44 LK45 LK46 LK47 LK48 LK58 LK57 LK56 LK55"};


#include "TelemFunctions.h"       // Various telemetry functions
#include "Beep.h"                 // beeps and CW
#include "Timing.h"               // Scheduling - it will tx on first 2 min slot after power on, running the nominal 10 min schedule

// Function Prototype
void wdt_init(void) __attribute__ ((naked, used, section(".init3")));
// Function Implementation
void wdt_init(void)
 {
  mcusr_mirror = MCUSR;  // SQ9GOL: zachowaj przyczyne startu
  MCUSR = 0x00;  //Clear all reset flags
  WDTCSR |= (1 << WDCE) | (1 << WDE);
  WDTCSR = (1 << WDP3) | (1 << WDP0);
 }

void setup()
 {
  wdt_enable(WDTO_8S);

  // SQ9GOL: przyczyna startu i licznik startow w EEPROM
  // 1.11.2026: przy wlaczeniu zasilania procesor ustawia czesto PORF i BORF naraz (napiecie rosnie powoli) - wtedy to zwykly start
  if      (mcusr_mirror & _BV(PORF))  resetCause = 0;
  else if (mcusr_mirror & _BV(WDRF))  resetCause = 2;
  else if (mcusr_mirror & _BV(BORF))  resetCause = 1;
  else if (mcusr_mirror & _BV(EXTRF)) resetCause = 3;
  else                                resetCause = 0;
  resetCount = eeprom_read_byte((uint8_t *)0) + 1;
  eeprom_update_byte((uint8_t *)0, resetCount);

  clock_prescale_set(clock_div_2);
  //clock_prescale_set(clock_div_4);

  DDRD |= 0b11110100;   // pins 2, 4, 5, 6, 7 - si5351 pins set as output 

  if (CW_AT_START) cw_send(CW);   // CW call at startup (SQ9GOL: wylaczalne CW_AT_START)

  PORTC |= 0b00001111;  // pre-load HIGH (enables pull-ups while still inputs) - gps_on
  DDRC  |= 0b00001111;  // then switch to outputs, they go HIGH immediately - gps_on 
  delay(1000);
  Serial.begin(9600);

  //delay(1500); Serial.write("$PMTK886,3*2B\r\n");   //set balloon mode for QUECTEL L70 GPS module
  delay(1500); Serial.write("$PCAS04,1*18\r\n");      //GPS only ATGM336
  //delay(1500); Serial.write("$PCAS04,2*1B\r\n");    //BD  only ATGM336
  //delay(1500); Serial.write("$PCAS04,7*1E\r\n");    //All ATGM336 
  //delay(1500); Serial.print("$PSIMNAV,W,3*3A\r\n"); //Sets Balloon mode of L70
  //delay(1500); Serial.write("$PMTK127*36\r\n");     //Erase EPO data stored in the flash memory - L70
  //delay(1500); Serial.write("$PMTK120*31\r\n");     //Erase aiding data stored in the flash memory - L70
  //delay(1500); Serial.write("$PMTK104*37\r\n");     //resets the GNSS module to the factory default - L70
  wdt_reset();
 }

ISR(TIMER1_COMPA_vect) // Interrupt to drive the TX timing
 {  proceed = true; }

void loop()
 {
  wdt_reset();
  // SQ9GOL 1.11.2026: Mike 2.2 usunal wdt_reset() z tej petli. Po kazdym nadawaniu GPS startuje od nowa (GPS+BeiDou,
  // duzo zdan GSV) i przy 2 MHz petla potrafila nie wyjsc przez > 8 s -> restart przez watchdog (test na stole:
  // ResetCause = watchdog po 2. nadawaniu z rzedu, brakujace ramki, przeskok zegara). Teraz: wdt_reset() przy kazdym
  // znaku i najwyzej 64 znaki na obieg, zeby TXtiming() sprawdzal czas regularnie.
  uint8_t nGps = 0;
  while ((Serial.available() > 0) && (nGps++ < 64))
  {
    wdt_reset();
    gps.encode(Serial.read());   // GPS related functions need to be in here to work with tinyGPS Plus library
  }
  syncTimeFromGPS();               // SQ9GOL: zegar tylko ze swiezego zdania RMC z prawdziwa data
  if (!bootFixMs && gps.location.isValid()) { bootFixMs = millis(); if (!bootFixMs) bootFixMs = 1; }  // SQ9GOL
  if (gps.location.isValid() && (timeStatus() != timeNotSet))
  {
    TXtiming();                    // Process timing
  }
  }
