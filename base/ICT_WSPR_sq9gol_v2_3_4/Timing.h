// ---- Schedule setup (10-minute cycle, 2-minute slots at minutes 0, 2, 4, 6, 8) ----
#define SLOT(m) (1 << ((m) / 2))
const uint8_t TX_SLOTS  = SLOT(0) | SLOT(2) | SLOT(4) | SLOT(6);  // minutes that transmit; any other even minute is idle (GPS only)
const uint8_t TLM_SLOT  = 2;   // minute that carries the telemetry message (must be one of TX_SLOTS)
const uint8_t EXT_SLOT_OFFSET = 4;  // SQ9GOL: telemetria rozszerzona Traquito w slocie +4 min (zamiast powtorki ramki)
const uint8_t PREP_SLOT = 0;   // slot where new GPS data is used, if 8 is gps-on =0, if 6 is gps-on =8 (the first TX slot after the idle one)

__attribute__((noinline)) void TXtiming() // Timing
{
  static int8_t lastHandledMinute = -1;   // minute we've already acted on
  static bool firstTx = true;             // true until the first transmission after power-up

  if ((timeStatus() == timeSet) && (second() <= 1) && (minute() != lastHandledMinute))
  {
    lastHandledMinute = minute();

    uint8_t slot = minute() % 10;         // position in the 10-minute cycle

if (((slot % 2) == 0) && ((TX_SLOTS & SLOT(slot)) || firstTx))   // a TX slot, or the first even minute after power-up
    {
      if ((slot == PREP_SLOT) || (telemetry_set == 0))  // fresh data at PREP_SLOT, or at first slot after power-up
      {
        loc4calc();
        call_telem();
        dbm_alt();
        loc_dbm_telem();
        geofence_check();
        freq = (Outside == true) ? WSPR_FREQ : ISM_WSPR_FREQ;
        telemetry_set = 1;                // data is ready
        gpsWatchdog();                    // SQ9GOL: zimny start GPS, gdy fix nie odswiezal sie > 20 min
        prepMotion();                     // SQ9GOL: predkosc pionowa, skoki pozycji
      }

      if (MIN_TX_MV && telemetry_set && vcc_mv && vcc_mv < MIN_TX_MV) { firstTx = false; return; }  // SQ9GOL: za niskie napiecie - pomin slot, GPS dalej wlaczony

      gps_off();
      rf_on();
      if ((slot == TLM_SLOT) && !firstTx) setModeWSPR_telem();
      else if ((slot == (PREP_SLOT + EXT_SLOT_OFFSET) % 10) && !firstTx) setModeWSPR_ext();   // SQ9GOL
      else                                setModeWSPR();
      encode();
      rf_off();
      gps_on();
      firstTx = false;
    }
    // idle slot: nothing to do, the GPS is already on after the previous TX
  }
}