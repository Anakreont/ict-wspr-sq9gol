/* 
 *  Beep functions and CW
 */

#define DIT_DURATION 50

void beep(int on, int off) {
  // freq = 14096950UL;
  // freq = 18105900UL;
     freq = WSPR_FREQ - 150UL;   // CW tuz pod kanalem WSPR, na pasmie trackera (bylo na sztywno 12 m)
  // freq = 28126015UL;
  
  rf_on();
  Si5351a_Write_Reg (18, 128);  // Disable output CLK2
  if (use180deg) Si5351a_Write_Reg (17, 77); // Enable output CLK1 (180° dipole mode only)
  Si5351a_Write_Reg (16, 77);   // Enable output CLK0, set crystal as source and Integer Mode on PLLA
  SetFrequency(freq * 100UL);   // SetFrequency expects centi-Hz (Hz x100)
  delay(off);
  Si5351a_Write_Reg (16, 128);  // Disable output CLK0
  Si5351a_Write_Reg (17, 128);  // Disable output CLK1 (harmless no-op if it was never enabled)

  delay(on);
  rf_off();
  wdt_reset();
}

void dit() { beep(DIT_DURATION, DIT_DURATION); }

void dah() { beep(DIT_DURATION * 3, DIT_DURATION); }

void sk() { delay(DIT_DURATION * 3 ); }

#include <avr/pgmspace.h>

#define M(len, bits) (((len) << 5) | (bits))

const uint8_t morse_az[] PROGMEM = {
  M(2,0b01),   M(4,0b1000), M(4,0b1010), M(3,0b100),  M(1,0b0),    // A B C D E
  M(4,0b0010), M(3,0b110),  M(4,0b0000), M(2,0b00),   M(4,0b0111), // F G H I J
  M(3,0b101),  M(4,0b0100), M(2,0b11),   M(2,0b10),   M(3,0b111),  // K L M N O
  M(4,0b0110), M(4,0b1101), M(3,0b010),  M(3,0b000),  M(1,0b1),    // P Q R S T
  M(3,0b001),  M(4,0b0001), M(3,0b011),  M(4,0b1001), M(4,0b1011), // U V W X Y
  M(4,0b1100)                                                      // Z
};

const uint8_t morse_09[] PROGMEM = {
  M(5,0b11111), M(5,0b01111), M(5,0b00111), M(5,0b00011), M(5,0b00001), // 0 1 2 3 4
  M(5,0b00000), M(5,0b10000), M(5,0b11000), M(5,0b11100), M(5,0b11110)  // 5 6 7 8 9
};

void cw_char(char c) {
  if (c >= 'a' && c <= 'z') c -= 32;           // accept lowercase
  uint8_t code;
  if      (c >= 'A' && c <= 'Z') code = pgm_read_byte(&morse_az[c - 'A']);
  else if (c >= '0' && c <= '9') code = pgm_read_byte(&morse_09[c - '0']);
  else return;                                  // unknown character: skip

  uint8_t len = code >> 5;
  rf_on();
  for (int8_t i = len - 1; i >= 0; i--) {
    if ((code >> i) & 1) dah(); else dit();
  }
  sk();
  rf_off();
}

void cw_send(const char *s) 
{
  while (*s) cw_char(*s++);
}