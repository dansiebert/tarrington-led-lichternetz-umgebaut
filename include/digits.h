#include <Arduino.h>

// digits 0,1,2...9, font size 8x16
const uint8_t digitals[] = {
  0x00, 0x1C, 0x36, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00, 0x00, 0x00, 0x00, // 0
  0x00, 0x18, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00, 0x00, 0x00, 0x00, // 1
  0x00, 0x3E, 0x63, 0x63, 0x63, 0x06, 0x06, 0x0C, 0x18, 0x30, 0x63, 0x7F, 0x00, 0x00, 0x00, 0x00, // 2
  0x00, 0x3E, 0x63, 0x63, 0x06, 0x1C, 0x06, 0x03, 0x03, 0x63, 0x66, 0x3C, 0x00, 0x00, 0x00, 0x00, // 3
  0x00, 0x06, 0x0E, 0x1E, 0x36, 0x36, 0x66, 0x66, 0x7F, 0x06, 0x06, 0x1F, 0x00, 0x00, 0x00, 0x00, // 4
  0x00, 0x7F, 0x60, 0x60, 0x60, 0x7C, 0x76, 0x03, 0x03, 0x63, 0x66, 0x3C, 0x00, 0x00, 0x00, 0x00, // 5
  0x00, 0x1E, 0x36, 0x60, 0x60, 0x7C, 0x76, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00, 0x00, 0x00, 0x00, // 6
  0x00, 0x7F, 0x66, 0x66, 0x0C, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, // 7
  0x00, 0x3E, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x63, 0x63, 0x3E, 0x00, 0x00, 0x00, 0x00, // 8
  0x00, 0x1C, 0x36, 0x63, 0x63, 0x63, 0x37, 0x1F, 0x03, 0x03, 0x36, 0x3C, 0x00, 0x00, 0x00, 0x00, // 9
};

// Ziffern 8 Pixel breit, 16 Pixel hoch + Leerzeilen fuer vertikales Scrollen
uint8_t scrollDigits[] = {
B00000000,
B00111100,
B01111110,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B01111110,
B00111100,
B00000000,

B00000000,
B00001000,
B00011000,
B00111000,
B01111000,
B11011000,
B00011000,
B00011000,
B00011000,
B00011000,
B00011000,
B00011000,
B00011000,
B00011000,
B00011000,
B11111111,
B11111111,
B00000000,

B00000000,
B00111110,
B01111110,
B11000011,
B11000011,
B10000011,
B00000011,
B00000011,
B00000110,
B00001100,
B00011000,
B00110000,
B01100000,
B11000000,
B11000000,
B11111111,
B11111111,
B00000000,

B00000000,
B00111100,
B01111110,
B11100111,
B11000011,
B00000011,
B00000111,
B00001110,
B00011100,
B00011110,
B00000011,
B00000011,
B11000011,
B11000011,
B11100110,
B01111110,
B00111100,
B00000000,

B00000000,
B00000011,
B00000111,
B00001111,
B00011011,
B00110011,
B01100011,
B11000011,
B11000011,
B11000011,
B11000011,
B11111111,
B11111111,
B00000011,
B00000011,
B00000011,
B00000011,
B00000000,

B00000000,
B11111111,
B11111111,
B11000000,
B11000000,
B11000000,
B11000000,
B11111100,
B11111110,
B11000111,
B00000011,
B00000011,
B00000011,
B00000011,
B11000111,
B01111110,
B00111100,
B00000000,

B00000000,
B00111100,
B01111110,
B11000111,
B11000011,
B11000000,
B11000000,
B11000000,
B11011100,
B11111110,
B11100111,
B11000011,
B11000011,
B11000011,
B11100111,
B01111110,
B00111100,
B00000000,

B00000000,
B11111111,
B11111111,
B00000011,
B00000110,
B00001100,
B00011000,
B00110000,
B01100000,
B11000000,
B11000000,
B11000000,
B11000000,
B11000000,
B11000000,
B11000000,
B11000000,
B00000000,

B00000000,
B00111100,
B01111110,
B01100110,
B01100110,
B01100110,
B01100110,
B00111100,
B01111110,
B01100110,
B11000011,
B11000011,
B11000011,
B11000011,
B11100111,
B01111110,
B00111100,
B00000000,

B00000000,
B00111100,
B01111110,
B11100111,
B11000011,
B11000011,
B11000011,
B11100111,
B01111111,
B00111011,
B00000011,
B00000011,
B00000011,
B11000011,
B11100111,
B01111110,
B00111100,
B00000000,

B00000000,   // "0" ist hier nochmal fuer den Uebergang von 9 nach 0 beim vertikalen Scrollen
B00111100,
B01111110,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B11000011,
B01111110,
B00111100,
B00000000,
};
