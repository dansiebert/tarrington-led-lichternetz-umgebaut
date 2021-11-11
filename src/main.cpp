#include <Arduino.h>

// DEV: setzen, wenn fuer Entwicklungsmatrix (64x16) kompiliert werden soll
#define DEV

#include "prototypes.h"
#include "defaultsettings.h"
#include "fonts.h"
#include "digits.h"
#include "christmassymbols.h"
#include "starsymbols.h"
#include "inputparameters.h"
#include "website.h"

#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <time.h>           // https://werner.rothschopf.net/201802_arduino_esp8266_ntp.htm

// https://randomnerdtutorials.com/esp32-esp8266-input-data-html-form/
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>

// https://randomnerdtutorials.com/wifimanager-with-esp8266-autoconnect-custom-parameter-and-manage-your-ssid-and-password/
#include <DNSServer.h>
#include <ESPAsyncWiFiManager.h>   // https://github.com/alanswx/ESPAsyncWiFiManager

// OTA https://github.com/ayushsharma82/AsyncElegantOTA/
#include <Hash.h>
#include <AsyncElegantOTA.h>

#define CONFIG "/config.txt"

#define MY_NTP_SERVER "de.pool.ntp.org"           
#define MY_TZ "CET-1CEST,M3.5.0,M10.5.0/3"   // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

time_t now;            // this is the epoch
struct tm tm;          // the structure tm holds time information in a more convient way
int h, m, s, w, mo, ye, d;

// create different instances
WiFiClient client;
AsyncWebServer server(80);
DNSServer dns;

#ifdef DEV
  #define Y_OFFSET 0
  #define NUM_ROWS 16
  const int cs0  = D0;
  const int cs1  = D4;
  const int a0   = D2;
  const int a1   = D3;
  const int a2   = D8;
  const int clk  = D7;               // 74HC595 clock pin
  const int sdi  = D5;               // 74HC595 serial data in pin
  const int le   = D6;               // 74HC595 latch pin
  uint8_t mask = 0xff;               // reverse matrix: mask = 0xff, normal matrix: mask =0x00
  bool mirror = 0;                   // Display horizontal spiegeln ?
  String mirrorCheckbox = "checked";
  String reverseCheckbox = "checked";
#else
  #define Y_OFFSET 2
  #define NUM_ROWS 20
  const int cs0  = D0;
  const int cs1  = D8;
  const int a0   = D2;
  const int a1   = D3;
  const int a2   = D4;
  const int clk  = D7;               // TM1818 clock pin
  const int sdi  = D5;               // TM1818 serial data in pin
  const int le   = D6;               // TM1818 latch pin
  const int oe   = D1;               // TM1818 output enable pin
  uint8_t mask = 0x00;               // reverse matrix: mask = 0xff, normal matrix: mask =0x00
  bool mirror = 1;                   // Display horizontal spiegeln ?
  String mirrorCheckbox = "checked";
  String reverseCheckbox = "unchecked";
#endif

// =======================================================================

const char* weatherHost = "api.openweathermap.org";
String weatherKey = "OpenWheaterMap-API-Key_eintragen";
String weatherLang = "&lang=en";
String weatherString;
String deg = String(char('~' + 25));
int humidity, pressure, clouds, windDeg;
float onlinetemp, tempMin, tempMax, windSpeed;

// =======================================================================

String scrollString;
String dayName[] = {"Err", "Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};

unsigned long clkTimeEffect = 0;
unsigned long clkTimeLeadtime = 0;
int updCnt = 0;
uint8_t state = 1;

// =======================================================================

// Display Buffer 160 = 8 byte * 20 rows
// Das, was Funktionen in dieses Array schreiben, wird per Timer-Interrupt auf das LED-Netz ausgegeben (waagerecht 8*8=64, senkrecht 20).
uint8_t displaybuf[160] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x01,0x80,0x00,0x00,0x00,0x00,0x00,0x00,
0x01,0xFC,0x0C,0x00,0x03,0x00,0x00,0x00,
0x01,0xF8,0x0C,0x00,0x03,0x00,0x00,0x00,
0x01,0xF0,0x0C,0x00,0x0F,0xC0,0x00,0x00,
0x00,0xE0,0x0C,0xC7,0xE3,0x0F,0xE7,0xC0,
0x00,0x40,0x0D,0x8E,0x63,0x00,0xCC,0x60,
0x00,0xE0,0x0F,0x0C,0x63,0x01,0x8C,0x60,
0x01,0xF0,0x0E,0x0C,0x63,0x01,0x8F,0xC0,
0x23,0xF8,0x0D,0x8C,0x63,0x03,0x0C,0x00,
0x33,0xF8,0x0C,0xCE,0x63,0x06,0x0C,0xE0,
0x3B,0xF8,0x0C,0x67,0xF1,0xEF,0xE7,0xC0,
0x3B,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,
0x1B,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,
0x0B,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,
0x07,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

// =======================================================================

// transfer displaybuf onto led matrix using timer interrupt (configured in setup())
ICACHE_RAM_ATTR void timer1_ISR() {

  if (mirror == 1) {   // Scroll-Richtung tauschen (beim letzten Byte (Zone) beginnen und alle Bits in den Bytes spiegeln)
    static uint8_t row = 0;                       // wird wegen Typ "static" nur beim ersten Durchlauf auf 0 gesetzt
    uint8_t *head = displaybuf + row * 8 + 7;     // Zeiger auf letzte Zone (8. Byte) der aktuellen Zeile setzen
    for (uint8_t byte = 0; byte < 8; byte++) {
      uint8_t pixels = *head;                     // 1 Byte (pixels) aus Display-Puffer (wo der Zeiger (*head) gerade hinzeigt) lesen
      head--;                                     // Zeiger in aktueller Zeile eine Zone (1 Byte) nach links setzen (verringern)
      pixels = pixels ^ mask;                     // aktuelles Byte ggf. reversieren

      // alle Bits im aktuellen Byte spiegeln (pixels wird zu reversepixels)
	    uint8_t reversepixels = 0;
	    for (uint8_t i = 0; i<8; i++) {
        bitWrite(reversepixels,7-i,bitRead(pixels,i));
      }
	  
      // aktuelles Byte ins Schieberegister shiften
      for (uint8_t i = 0; i<8; i++) {
        digitalWrite(sdi, !!(reversepixels & (1 << (7 - i))));   // Bit an seriellen Eingang des Schieberegisters legen
        digitalWrite(clk,HIGH);                                  // Flankenwechsel an Takteingang des Schieberegisters zum Laden des Bits
        digitalWrite(clk,LOW);   
      }
    }
    #ifdef DEV
      digitalWrite(cs1, HIGH);                  // disable display (einfach beide 74HC138 via CS deaktivieren)    
      // select row
      digitalWrite(a0,  (row & 0x01));
      digitalWrite(a1,  (row & 0x02));
      digitalWrite(a2,  (row & 0x04));
      digitalWrite(cs0, (row & 0x08));
      // latch data
      digitalWrite(le,  HIGH);
      digitalWrite(le,  LOW);
      digitalWrite(cs1, LOW);                   // enable display
    #else
      digitalWrite(oe, HIGH);                  // disable display    
      // select row
      digitalWrite(a0,  (row & 0x01));
      digitalWrite(a1,  (row & 0x02));
      digitalWrite(a2,  (row & 0x04));
      digitalWrite(cs0, (row & 0x08));
      digitalWrite(cs1, (row & 0x10));
      // latch data
      digitalWrite(le, HIGH);
      digitalWrite(le, LOW);
      digitalWrite(oe, LOW);                   // enable display
    #endif
    row = row + 1;
    if ( row == NUM_ROWS ) row = 0;            // wieder zurueck auf erste Zeile

  } else {

    static uint8_t row = 0;                    // is only set the first time through the loop because of "static"
    uint8_t *head = displaybuf + row * 8;      // pointer to begin of every row (8 byte per row)
    for (uint8_t byte = 0; byte < 8; byte++) {
      uint8_t pixels = *head;
      head++;
      pixels = pixels ^ mask;
      for (uint8_t i = 0; i<8; i++) {
        digitalWrite(sdi, !!(pixels & (1 << (7 - i))));
        digitalWrite(clk,HIGH);
        digitalWrite(clk,LOW);   
      }
      
    }
    #ifdef DEV
      digitalWrite(cs1, HIGH);                  // disable display (einfach beide 74HC138 via CS deaktivieren)    
      // select row
      digitalWrite(a0,  (row & 0x01));
      digitalWrite(a1,  (row & 0x02));
      digitalWrite(a2,  (row & 0x04));
      digitalWrite(cs0, (row & 0x08));
      // latch data
      digitalWrite(le,  HIGH);
      digitalWrite(le,  LOW);
      digitalWrite(cs1, LOW);                   // enable display
    #else
      digitalWrite(oe, HIGH);                   // disable display    
      // select row
      digitalWrite(a0,  (row & 0x01));
      digitalWrite(a1,  (row & 0x02));
      digitalWrite(a2,  (row & 0x04));
      digitalWrite(cs0, (row & 0x08));
      digitalWrite(cs1, (row & 0x10));
      // latch data
      digitalWrite(le, HIGH);
      digitalWrite(le, LOW);
      digitalWrite(oe, LOW);                   // enable display
    #endif
    row = row + 1;
    if ( row == NUM_ROWS ) row = 0;            // Anzahl Zeilen

  }

    //timer1_write(500);
    timer1_write(1000);
}

// =======================================================================

void getTimeLocal(){
  /*
  Member    Type  Meaning Range
  tm_sec    int   seconds after the minute  0-61*
  tm_min    int   minutes after the hour  0-59
  tm_hour   int   hours since midnight  0-23
  tm_mday   int   day of the month  1-31
  tm_mon    int   months since January  0-11
  tm_year   int   years since 1900
  tm_wday   int   days since Sunday 0-6
  tm_yday   int   days since January 1  0-365
  tm_isdst  int   Daylight Saving Time flag
  */
  time(&now);                       // read the current time
  localtime_r(&now, &tm);           // update the structure tm with the current time
  h = tm.tm_hour;
  m = tm.tm_min;
  s = tm.tm_sec;
  ye = tm.tm_year + 1900;
  mo = tm.tm_mon + 1;
  d = tm.tm_mday;
  w = tm.tm_wday;
}

int8_t getWifiQuality() {   // converts the dBm to a range between 0 and 100%
  int32_t dbm = WiFi.RSSI();
  if (dbm <= -100) {
    return 0;
  } else if (dbm >= -50) {
    return 100;
  } else {
    return 2 * (dbm + 100);
  }
}

// =======================================================================

// Funktion taugt nichts, wenn mehrere Digits gleichzeitig gescrollt werden muessen,
// da sie in einer geschlossenen Schleife laeuft
void scrollDigit(uint8_t xPos, uint8_t yPos, uint8_t newDigit, uint8_t speed){
  if (newDigit == 0) newDigit = 10;               // fuer den Uebergang von 9 nach 0
  for (uint8_t trans = 0; trans < 19; trans++){   // Font-Array Ausschnitt (8x18) gleiten lassen
    drawImage(xPos, yPos, 8, 18, scrollDigits + trans + (newDigit - 1) * 18);
    delay(speed);
  }
}

// =======================================================================
/* --- draw functions --- */

void clearMatrix() {
  uint8_t *ptr = displaybuf;
    for (uint16_t i = 0; i < NUM_ROWS * 8; i++) {
      *ptr = 0x00;
      ptr++;
    }
}

void drawPoint(uint16_t x, uint16_t y, uint8_t pixel) {
  if ( x < 0 || x > 63 || y < 0 || y > (NUM_ROWS - 1) ) {
    return;
  }
  uint8_t *byte = displaybuf + x / 8 + y * 8;
  uint8_t  bit = x % 8;
  if (pixel) {
    *byte |= 0x80 >> bit;
  } else {
    *byte &= ~(0x80 >> bit);
  }
}

void drawRect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t pixel) {
  for (uint16_t x = x1; x < x2; x++) {
    for (uint16_t y = y1; y < y2; y++) {
      drawPoint(x, y, pixel);
    }
  }
}

void drawImage(uint16_t xoffset, uint16_t yoffset, uint16_t width, uint16_t height, const uint8_t *image) {
  for (uint16_t y = 0; y < height; y++) {
    for (uint16_t x = 0; x < width; x++) {
      const uint8_t *byte = image + (x + y * width) / 8;
      uint8_t  bit = 7 - x % 8;
      uint8_t  pixel = (*byte >> bit) & 1;
      drawPoint(x + xoffset, y + yoffset, pixel);
    }
  }
}

// (x, y) top-left position, x should be multiple of 8
void drawDigital_8(uint16_t x, uint16_t y, uint8_t n) {
  if ((n >= 10) || (0 != (x % 8))) {
    return;
  }
  uint8_t *pDst = displaybuf + y * 8 + x / 8;
  const uint8_t *pSrc = digitals + n * 16;
  for (uint8_t i = 0; i < 16; i++) {
    *pDst = *pSrc;
    pDst += 8;
    pSrc++;
  }
}

void drawDigital_16(uint16_t x, uint16_t y, uint8_t n) {
  uint8_t *pDst = displaybuf + y * 8 + x / 8;
  const uint8_t *pSrc = BigFont + n * 32;
  for (uint8_t i = 0; i < 16; i++) {
    *pDst = *pSrc;
    pDst++;
    pSrc++;
    *pDst = *pSrc;
    pDst += 7;
    pSrc++;
  }
}
/* --- end draw functions --- */

// =======================================================================

String processor(const String& var) {

// wifi
  if (var == "SSID") {
    return WiFi.SSID();
  } else if (var == "RSSI") {
    return String(getWifiQuality());

// matrix appearance
  } else if (var == "MIRROR_CHECKBOX") {
    return mirrorCheckbox;
  } else if (var == "REVERSE_CHECKBOX") {
    return reverseCheckbox;

// clock
  } else if (var == "ANIMCLOCK_CHECKBOX") {
    return animClockCheckbox;
  } else if (var == "COLONS_CHECKBOX") {
    return colonsCheckbox;

// scrollText1
  } else if (var == "SCROLLTEXT1_INPUT") {
    return scrollText1;
  } else if (var == "SCROLLTEXT1_CHECKBOX") {
    return scrollText1Checkbox;
  } else if (var == "SCROLLTEXT1_LEADTIME") {
    return scrollText1Leadtime;
  } else if (var == "SCROLLTEXT1_DELAY") {
    return scrollText1Delay;
  } else if (var == "SCROLLTEXT1_FONT") {
      String tFont1 = scrollText1Font;
      String textFont1Options = "<option value='1'>8x16</option><option value='2'>16x20</option><option value='3'>random</option>";
      textFont1Options.replace(tFont1 + "'", tFont1 + "' selected" );
    return textFont1Options;

// scrollText2
  } else if (var == "SCROLLTEXT2_INPUT") {
    return scrollText2;
  } else if (var == "SCROLLTEXT2_CHECKBOX") {
    return scrollText2Checkbox;
  } else if (var == "SCROLLTEXT2_LEADTIME") {
    return scrollText2Leadtime;
  } else if (var == "SCROLLTEXT2_DELAY") {
    return scrollText2Delay;
  } else if (var == "SCROLLTEXT2_FONT") {
      String tFont2 = scrollText2Font;
      String textFont2Options = "<option value='1'>8x16</option><option value='2'>16x20</option><option value='3'>random</option>";
      textFont2Options.replace(tFont2 + "'", tFont2 + "' selected" );
    return textFont2Options;

// scrollText3
  } else if (var == "SCROLLTEXT3_INPUT") {
    return scrollText3;
  } else if (var == "SCROLLTEXT3_CHECKBOX") {
    return scrollText3Checkbox;
  } else if (var == "SCROLLTEXT3_LEADTIME") {
    return scrollText3Leadtime;
  } else if (var == "SCROLLTEXT3_DELAY") {
    return scrollText3Delay;
  } else if (var == "SCROLLTEXT3_FONT") {
      String tFont3 = scrollText3Font;
      String textFont3Options = "<option value='1'>8x16</option><option value='2'>16x20</option><option value='3'>random</option>";
      textFont3Options.replace(tFont3 + "'", tFont3 + "' selected" );
    return textFont3Options;

// christmasCounter(Text)
  } else if (var == "CHRISTMASCOUNTER_TEXT") {
    return christmasCounterText;
  } else if (var == "CHRISTMASCOUNTER_CHECKBOX") {
    return christmasCounterCheckbox;
  } else if (var == "CHRISTMASCOUNTER_LEADTIME") {
    return christmasCounterLeadtime;
  } else if (var == "CHRISTMASCOUNTER_DELAY") {
    return christmasCounterDelay;
  } else if (var == "CHRISTMASCOUNTER_FONT") {
      String ccFont = christmasCounterFont;
      String ccFontOptions = "<option value='1'>8x16</option><option value='2'>16x20</option><option value='3'>random</option>";
      ccFontOptions.replace(ccFont + "'", ccFont + "' selected" );
    return ccFontOptions;

// weather
  } else if (var == "WEATHER_CHECKBOX") {
    return weatherCheckbox;
  } else if (var == "WEATHER_LEADTIME") {
    return weatherLeadtime;
  } else if (var == "WEATHER_DELAY") {
    return weatherDelay;
  } else if (var == "WEATHER_FONT") {
      String wFont = weatherFont;
      String weatherFontOptions = "<option value='1'>8x16</option><option value='2'>16x20</option><option value='3'>random</option>";
      weatherFontOptions.replace(wFont + "'", wFont + "' selected" );
    return weatherFontOptions;

  } else if (var == "WEATHERLOCATION_CHECKBOX") {
    return weatherLocationCheckbox;
  } else if (var == "WEATHERLOCATION") {
    return weatherLocation;
  } else if (var == "CITYID") {
    return cityID;
  } else if (var == "DATE_CHECKBOX") {
    return dateCheckbox;
  } else if (var == "TEMP_CHECKBOX") {
    return tempCheckbox;
  } else if (var == "RAIN_CHECKBOX") {
    return rainCheckbox;
  } else if (var == "WIND_CHECKBOX") {
    return windCheckbox;
  } else if (var == "HUMIDITY_CHECKBOX") {
    return humidityCheckbox;
  } else if (var == "PRESSURE_CHECKBOX") {
    return pressureCheckbox;

// snowFallSingle
  } else if (var == "SNOWFALLSINGLE_CHECKBOX") {
    return snowFallSingleCheckbox;
  } else if (var == "SNOWFALLSINGLE_LEADTIME") {
    return snowFallSingleLeadtime;
  } else if (var == "SNOWFALLSINGLE_DELAY") {
    return snowFallSingleDelay;
  } else if (var == "SNOWFALLSINGLE_DURATION") {
    return snowFallSingleDuration;

// snowFallMulti
  } else if (var == "SNOWFALLMULTI_CHECKBOX") {
    return snowFallMultiCheckbox;
  } else if (var == "SNOWFALLMULTI_LEADTIME") {
    return snowFallMultiLeadtime;
  } else if (var == "SNOWFALLMULTI_DELAY") {
    return snowFallMultiDelay;
  } else if (var == "SNOWFALLMULTI_DURATION") {
    return snowFallMultiDuration;

// pixelFall
  } else if (var == "PIXELFALL_CHECKBOX") {
    return pixelFallCheckbox;
  } else if (var == "PIXELFALL_LEADTIME") {
    return pixelFallLeadtime;
  } else if (var == "PIXELFALL_DELAY") {
    return pixelFallDelay;
  } else if (var == "PIXELFALL_DURATION") {
    return pixelFallDuration;

// starrySky
  } else if (var == "STARRYSKY_CHECKBOX") {
    return starrySkyCheckbox;
  } else if (var == "STARRYSKY_LEADTIME") {
    return starrySkyLeadtime;
  } else if (var == "STARRYSKY_DELAY") {
    return starrySkyDelay;
  } else if (var == "STARRYSKY_DURATION") {
    return starrySkyDuration;
  } else if (var == "STARRYSKY_STARCOUNT") {
    return starrySkyStarCount;

// growingStar
  } else if (var == "GROWINGSTAR_CHECKBOX") {
    return growingStarCheckbox;
  } else if (var == "GROWINGSTAR_LEADTIME") {
    return growingStarLeadtime;
  } else if (var == "GROWINGSTAR_DELAY") {
    return growingStarDelay;
  } else if (var == "GROWINGSTAR_DURATION") {
    return growingStarDuration;

// christmasSymbols
  } else if (var == "CHRISTMASSYMBOLS_CHECKBOX") {
    return christmasSymbolsCheckbox;
  } else if (var == "CHRISTMASSYMBOLS_LEADTIME") {
    return christmasSymbolsLeadtime;
  } else if (var == "CHRISTMASSYMBOLS_DELAY") {
    return christmasSymbolsDelay;
  } else if (var == "CHRISTMASSYMBOLS_DURATION") {
    return christmasSymbolsDuration;

// moveChristmasSymbols
  } else if (var == "MOVECHRISTMASSYMBOLS_CHECKBOX") {
    return moveChristmasSymbolsCheckbox;
  } else if (var == "MOVECHRISTMASSYMBOLS_LEADTIME") {
    return moveChristmasSymbolsLeadtime;
  } else if (var == "MOVECHRISTMASSYMBOLS_DELAY") {
    return moveChristmasSymbolsDelay;
  } else if (var == "MOVECHRISTMASSYMBOLS_DURATION") {
    return moveChristmasSymbolsDuration;
  } else if (var == "MOVECHRISTMASSYMBOLS_SYMBOL") {
      String cSymbol = moveChristmasSymbolsSymbol;
      String moveChristmasSymbolsSymbolOptions = "<option value='1'>Schneemann</option><option value='2'>Tannenbaum</option><option value='3'>Kerze</option><option value='4'>Paket</option><option value='5'>Schneeflocke</option><option value='6'>Zufall</option>";
      moveChristmasSymbolsSymbolOptions.replace(cSymbol + "'", cSymbol + "' selected" );
    return moveChristmasSymbolsSymbolOptions;

  }
  return String();
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void initLittleFS() {
  LittleFS.begin();
  if (!LittleFS.exists("/formatComplete.txt")) {
    Serial.println();
    Serial.println("Please wait 30 secs for LittleFS to be formatted...");
    LittleFS.format();
    Serial.println("LittleFS formatted.");   
    File f = LittleFS.open("/formatComplete.txt", "w");
    if (!f) {
      Serial.println("File open failed!");
    } else {
      f.println("Format complete.");
    }
  } else {
    Serial.println();
    Serial.println("LittleFS is formatted. Moving along...");
  }
}

void writeConfig() {
  File f = LittleFS.open(CONFIG, "w");
  if (!f) {
    Serial.println("File open failed!");
  } else {
    Serial.println();
    Serial.println("Saving settings to LittleFS now...");
    f.println("mirrorCheckbox=" + mirrorCheckbox);
    f.println("reverseCheckbox=" + reverseCheckbox);

    f.println("animClockCheckbox=" + animClockCheckbox);
    f.println("colonsCheckbox=" + colonsCheckbox);

    f.println("scrollText1=" + scrollText1);
    f.println("scrollText1Checkbox=" + scrollText1Checkbox);
    f.println("scrollText1Leadtime=" + scrollText1Leadtime);
    f.println("scrollText1Delay=" + scrollText1Delay);
    f.println("scrollText1Font=" + scrollText1Font);

    f.println("scrollText2=" + scrollText2);
    f.println("scrollText2Checkbox=" + scrollText2Checkbox);
    f.println("scrollText2Leadtime=" + scrollText2Leadtime);
    f.println("scrollText2Delay=" + scrollText2Delay);
    f.println("scrollText2Font=" + scrollText2Font);

    f.println("scrollText3=" + scrollText3);
    f.println("scrollText3Checkbox=" + scrollText3Checkbox);
    f.println("scrollText3Leadtime=" + scrollText3Leadtime);
    f.println("scrollText3Delay=" + scrollText3Delay);
    f.println("scrollText3Font=" + scrollText3Font);

    f.println("weatherCheckbox=" + weatherCheckbox);
    f.println("weatherLeadtime=" + weatherLeadtime);
    f.println("weatherDelay=" + weatherDelay);
    f.println("weatherFont=" + weatherFont);

    f.println("weatherLocationCheckbox=" + weatherLocationCheckbox);
    f.println("cityID=" + cityID);
    f.println("dateCheckbox=" + dateCheckbox);
    f.println("tempCheckbox=" + tempCheckbox);
    f.println("rainCheckbox=" + rainCheckbox);
    f.println("windCheckbox=" + windCheckbox);
    f.println("humidityCheckbox=" + humidityCheckbox);
    f.println("pressureCheckbox=" + pressureCheckbox);

    f.println("snowFallSingleCheckbox=" + snowFallSingleCheckbox);
    f.println("snowFallSingleLeadtime=" + snowFallSingleLeadtime);
    f.println("snowFallSingleDelay=" + snowFallSingleDelay);
    f.println("snowFallSingleDuration=" + snowFallSingleDuration);

    f.println("snowFallMultiCheckbox=" + snowFallMultiCheckbox);
    f.println("snowFallMultiLeadtime=" + snowFallMultiLeadtime);
    f.println("snowFallMultiDuration=" + snowFallMultiDuration);
    f.println("snowFallMultiDelay=" + snowFallMultiDelay);

    f.println("pixelFallCheckbox=" + pixelFallCheckbox);
    f.println("pixelFallLeadtime=" + pixelFallLeadtime);
    f.println("pixelFallDelay=" + pixelFallDelay);
    f.println("pixelFallDuration=" + pixelFallDuration);

    f.println("starrySkyCheckbox=" + starrySkyCheckbox);
    f.println("starrySkyLeadtime=" + starrySkyLeadtime);
    f.println("starrySkyDuration=" + starrySkyDuration);
    f.println("starrySkyDelay=" + starrySkyDelay);
    f.println("starrySkyStarCount=" + starrySkyStarCount);

    f.println("growingStarCheckbox=" + growingStarCheckbox);
    f.println("growingStarLeadtime=" + growingStarLeadtime);
    f.println("growingStarDelay=" + growingStarDelay);
    f.println("growingStarDuration=" + growingStarDuration);

    f.println("christmasSymbolsCheckbox=" + christmasSymbolsCheckbox);
    f.println("christmasSymbolsLeadtime=" + christmasSymbolsLeadtime);
    f.println("christmasSymbolsDelay=" + christmasSymbolsDelay);
    f.println("christmasSymbolsDuration=" + christmasSymbolsDuration);

    f.println("moveChristmasSymbolsCheckbox=" + moveChristmasSymbolsCheckbox);
    f.println("moveChristmasSymbolsLeadtime=" + moveChristmasSymbolsLeadtime);
    f.println("moveChristmasSymbolsDelay=" + moveChristmasSymbolsDelay);
    f.println("moveChristmasSymbolsDuration=" + moveChristmasSymbolsDuration);
    f.println("moveChristmasSymbolsSymbol=" + moveChristmasSymbolsSymbol);

    f.println("christmasCounterText= " + christmasCounterText);
    f.println("christmasCounterCheckbox= " + christmasCounterCheckbox);
    f.println("christmasCounterLeadtime= " + christmasCounterLeadtime);
    f.println("christmasCounterDelay= " + christmasCounterDelay);
    f.println("christmasCounterFont= " + christmasCounterFont);
  }
  f.close();
}

void readConfig() {
  if (LittleFS.exists(CONFIG) == false) {
    Serial.println("Settings File does not yet exists.");
    writeConfig();
  }
  File fr = LittleFS.open(CONFIG, "r");
  Serial.println();
  Serial.println("Reading settings from LittleFS now...");
  String configline;  
  while (fr.available()) {
    configline = fr.readStringUntil('\n');

    // matrix appearance
    if (configline.indexOf("mirrorCheckbox=") >= 0) {
      mirrorCheckbox = configline.substring(configline.lastIndexOf("mirrorCheckbox=") + 15);
      mirrorCheckbox.trim();
      Serial.println("mirrorCheckbox= " + mirrorCheckbox);
      if (mirrorCheckbox == "checked") {
        mirror = 1;
      }
    }
    if (configline.indexOf("reverseCheckbox=") >= 0) {
      reverseCheckbox = configline.substring(configline.lastIndexOf("reverseCheckbox=") + 16);
      reverseCheckbox.trim();
      Serial.println("reverseCheckbox= " + reverseCheckbox);
      if (reverseCheckbox == "checked") {
        mask = 0xff;
      }
    }

    // clock
    if (configline.indexOf("colonsCheckbox=") >= 0) {
      colonsCheckbox = configline.substring(configline.lastIndexOf("colonsCheckbox=") + 15);
      colonsCheckbox.trim();
      Serial.println("colonsCheckbox= " + colonsCheckbox);
    }
    if (configline.indexOf("animClockCheckbox=") >= 0) {
      animClockCheckbox = configline.substring(configline.lastIndexOf("animClockCheckbox=") + 18);
      animClockCheckbox.trim();
      Serial.println("animClockCheckbox= " + animClockCheckbox);
    }

    // scrollText1
    if (configline.indexOf("scrollText1=") >= 0) {
      scrollText1 = configline.substring(configline.lastIndexOf("scrollText1=") + 12);
      scrollText1 = scrollText1.substring(0,scrollText1.length()-1); // letztes Zeichen (cr) abschneiden
      Serial.println("scrollText1= " + scrollText1);
    }
    if (configline.indexOf("scrollText1Checkbox=") >= 0) {
      scrollText1Checkbox = configline.substring(configline.lastIndexOf("scrollText1Checkbox=") + 20);
      scrollText1Checkbox.trim();
      Serial.println("scrollText1Checkbox= " + scrollText1Checkbox);
    }
    if (configline.indexOf("scrollText1Leadtime=") >= 0) {
      scrollText1Leadtime = configline.substring(configline.lastIndexOf("scrollText1Leadtime=") + 20).toInt();
      Serial.println("scrollText1Leadtime= " + scrollText1Leadtime);
    }
    if (configline.indexOf("scrollText1Delay=") >= 0) {
      scrollText1Delay = configline.substring(configline.lastIndexOf("scrollText1Delay=") + 17).toInt();
      Serial.println("scrollText1Delay= " + scrollText1Delay);
    }
    if (configline.indexOf("scrollText1Font=") >= 0) {
      scrollText1Font = configline.substring(configline.lastIndexOf("scrollText1Font=") + 16).toInt();
      Serial.println("scrollText1Font= " + scrollText1Font);
    }

    // scrollText2
    if (configline.indexOf("scrollText2=") >= 0) {
      scrollText2 = configline.substring(configline.lastIndexOf("scrollText2=") + 12);
      scrollText2 = scrollText2.substring(0,scrollText2.length()-1);
      Serial.println("scrollText2= " + scrollText2);
    }
    if (configline.indexOf("scrollText2Checkbox=") >= 0) {
      scrollText2Checkbox = configline.substring(configline.lastIndexOf("scrollText2Checkbox=") + 20);
      scrollText2Checkbox.trim();
      Serial.println("scrollText2Checkbox= " + scrollText2Checkbox);
    }
    if (configline.indexOf("scrollText2Leadtime=") >= 0) {
      scrollText2Leadtime = configline.substring(configline.lastIndexOf("scrollText2Leadtime=") + 20).toInt();
      Serial.println("scrollText2Leadtime= " + scrollText2Leadtime);
    }
    if (configline.indexOf("scrollText2Delay=") >= 0) {
      scrollText2Delay = configline.substring(configline.lastIndexOf("scrollText2Delay=") + 17).toInt();
      Serial.println("scrollText2Delay= " + scrollText2Delay);
    }
    if (configline.indexOf("scrollText2Font=") >= 0) {
      scrollText2Font = configline.substring(configline.lastIndexOf("scrollText2Font=") + 16).toInt();
      Serial.println("scrollText2Font= " + scrollText2Font);
    }

    // scrollText3
    if (configline.indexOf("scrollText3=") >= 0) {
      scrollText3 = configline.substring(configline.lastIndexOf("scrollText3=") + 12);
      scrollText3 = scrollText3.substring(0,scrollText3.length()-1);
      Serial.println("scrollText3= " + scrollText3);
    }
    if (configline.indexOf("scrollText3Checkbox=") >= 0) {
      scrollText3Checkbox = configline.substring(configline.lastIndexOf("scrollText3Checkbox=") + 20);
      scrollText3Checkbox.trim();
      Serial.println("scrollText3Checkbox= " + scrollText3Checkbox);
    }
    if (configline.indexOf("scrollText3Leadtime=") >= 0) {
      scrollText3Leadtime = configline.substring(configline.lastIndexOf("scrollText3Leadtime=") + 20).toInt();
      Serial.println("scrollText3Leadtime= " + scrollText3Leadtime);
    }
    if (configline.indexOf("scrollText3Delay=") >= 0) {
      scrollText3Delay = configline.substring(configline.lastIndexOf("scrollText3Delay=") + 17).toInt();
      Serial.println("scrollText3Delay= " + scrollText3Delay);
    }
    if (configline.indexOf("scrollText3Font=") >= 0) {
      scrollText3Font = configline.substring(configline.lastIndexOf("scrollText3Font=") + 16).toInt();
      Serial.println("scrollText3Font= " + scrollText3Font);
    }

    // christmasCounter(Text)
    if (configline.indexOf("christmasCounterText=") >= 0) {
      christmasCounterText = configline.substring(configline.lastIndexOf("christmasCounterText=") + 21);
      christmasCounterText = christmasCounterText.substring(0,christmasCounterText.length()-1);
      Serial.println("christmasCounterText= " + christmasCounterText);
    }
    if (configline.indexOf("christmasCounterCheckbox=") >= 0) {
      christmasCounterCheckbox = configline.substring(configline.lastIndexOf("christmasCounterCheckbox=") + 25);
      christmasCounterCheckbox.trim();
      Serial.println("christmasCounterCheckbox= " + christmasCounterCheckbox);
    }
    if (configline.indexOf("christmasCounterLeadtime=") >= 0) {
      christmasCounterLeadtime = configline.substring(configline.lastIndexOf("christmasCounterLeadtime=") + 25).toInt();
      Serial.println("christmasCounterLeadtime= " + christmasCounterLeadtime);
    }
    if (configline.indexOf("christmasCounterDelay=") >= 0) {
      christmasCounterDelay = configline.substring(configline.lastIndexOf("christmasCounterDelay=") + 22).toInt();
      Serial.println("christmasCounterDelay= " + christmasCounterDelay);
    }
    if (configline.indexOf("christmasCounterFont=") >= 0) {
      christmasCounterFont = configline.substring(configline.lastIndexOf("christmasCounterFont=") + 21).toInt();
      Serial.println("christmasCounterFont= " + christmasCounterFont);
    }

    // weather
    if (configline.indexOf("weatherCheckbox=") >= 0) {
      weatherCheckbox = configline.substring(configline.lastIndexOf("weatherCheckbox=") + 16);
      weatherCheckbox.trim();
      Serial.println("weatherCheckbox= " + weatherCheckbox);
    }
    if (configline.indexOf("weatherLeadtime=") >= 0) {
      weatherLeadtime = configline.substring(configline.lastIndexOf("weatherLeadtime=") + 16).toInt();
      Serial.println("weatherLeadtime= " + weatherLeadtime);
    }
    if (configline.indexOf("weatherDelay=") >= 0) {
      weatherDelay = configline.substring(configline.lastIndexOf("weatherDelay=") + 13).toInt();
      Serial.println("weatherDelay= " + weatherDelay);
    }
    if (configline.indexOf("weatherFont=") >= 0) {
      weatherFont = configline.substring(configline.lastIndexOf("weatherFont=") + 12).toInt();
      Serial.println("weatherFont= " + weatherFont);
    }
    
    if (configline.indexOf("weatherLocationCheckbox=") >= 0) {
      weatherLocationCheckbox = configline.substring(configline.lastIndexOf("weatherLocationCheckbox=") + 24);
      weatherLocationCheckbox.trim();
      Serial.println("weatherLocationCheckbox= " + weatherLocationCheckbox);
    }
    if (configline.indexOf("cityID=") >= 0) {
      cityID = configline.substring(configline.lastIndexOf("cityID=") + 7);
      cityID.trim();
      Serial.println("cityID= " + cityID);
    }
    if (configline.indexOf("dateCheckbox=") >= 0) {
      dateCheckbox = configline.substring(configline.lastIndexOf("dateCheckbox=") + 13);
      dateCheckbox.trim();
      Serial.println("dateCheckbox= " + dateCheckbox);
    }
    if (configline.indexOf("tempCheckbox=") >= 0) {
      tempCheckbox = configline.substring(configline.lastIndexOf("tempCheckbox=") + 13);
      tempCheckbox.trim();
      Serial.println("tempCheckbox= " + tempCheckbox);
    }
    if (configline.indexOf("rainCheckbox=") >= 0) {
      rainCheckbox = configline.substring(configline.lastIndexOf("rainCheckbox=") + 13);
      rainCheckbox.trim();
      Serial.println("rainCheckbox= " + rainCheckbox);
    }
    if (configline.indexOf("windCheckbox=") >= 0) {
      windCheckbox = configline.substring(configline.lastIndexOf("windCheckbox=") + 13);
      windCheckbox.trim();
      Serial.println("windCheckbox= " + windCheckbox);
    }
    if (configline.indexOf("humidityCheckbox=") >= 0) {
      humidityCheckbox = configline.substring(configline.lastIndexOf("humidityCheckbox=") + 17);
      humidityCheckbox.trim();
      Serial.println("humidityCheckbox= " + humidityCheckbox);
    }
    if (configline.indexOf("pressureCheckbox=") >= 0) {
      pressureCheckbox = configline.substring(configline.lastIndexOf("pressureCheckbox=") + 17);
      pressureCheckbox.trim();
      Serial.println("pressureCheckbox= " + pressureCheckbox);
    }

    // snowFallSingle
    if (configline.indexOf("snowFallSingleCheckbox=") >= 0) {
      snowFallSingleCheckbox = configline.substring(configline.lastIndexOf("snowFallSingleCheckbox=") + 23);
      snowFallSingleCheckbox.trim();
      Serial.println("snowFallSingleCheckbox= " + snowFallSingleCheckbox);
    }
    if (configline.indexOf("snowFallSingleLeadtime=") >= 0) {
      snowFallSingleLeadtime = configline.substring(configline.lastIndexOf("snowFallSingleLeadtime=") + 23).toInt();
      Serial.println("snowFallSingleLeadtime= " + snowFallSingleLeadtime);
    }
    if (configline.indexOf("snowFallSingleDelay=") >= 0) {
      snowFallSingleDelay = configline.substring(configline.lastIndexOf("snowFallSingleDelay=") + 20).toInt();
      Serial.println("snowFallSingleDelay= " + snowFallSingleDelay);
    }
    if (configline.indexOf("snowFallSingleDuration=") >= 0) {
      snowFallSingleDuration = configline.substring(configline.lastIndexOf("snowFallSingleDuration=") + 23).toInt();
      Serial.println("snowFallSingleDuration= " + snowFallSingleDuration);
    }

    // snowFallMulti
    if (configline.indexOf("snowFallMultiCheckbox=") >= 0) {
      snowFallMultiCheckbox = configline.substring(configline.lastIndexOf("snowFallMultiCheckbox=") + 22);
      snowFallMultiCheckbox.trim();
      Serial.println("snowFallMultiCheckbox= " + snowFallMultiCheckbox);
    }
    if (configline.indexOf("snowFallMultiLeadtime=") >= 0) {
      snowFallMultiLeadtime = configline.substring(configline.lastIndexOf("snowFallMultiLeadtime=") + 22).toInt();
      Serial.println("snowFallMultiLeadtime= " + snowFallMultiLeadtime);
    }
    if (configline.indexOf("snowFallMultiDelay=") >= 0) {
      snowFallMultiDelay = configline.substring(configline.lastIndexOf("snowFallMultiDelay=") + 19).toInt();
      Serial.println("snowFallMultiDelay= " + snowFallMultiDelay);
    }
    if (configline.indexOf("snowFallMultiDuration=") >= 0) {
      snowFallMultiDuration = configline.substring(configline.lastIndexOf("snowFallMultiDuration=") + 22).toInt();
      Serial.println("snowFallMultiDuration= " + snowFallMultiDuration);
    }

    // pixelFall
    if (configline.indexOf("pixelFallCheckbox=") >= 0) {
      pixelFallCheckbox = configline.substring(configline.lastIndexOf("pixelFallCheckbox=") + 18);
      pixelFallCheckbox.trim();
      Serial.println("pixelFallCheckbox= " + pixelFallCheckbox);
    }
    if (configline.indexOf("pixelFallLeadtime=") >= 0) {
      pixelFallLeadtime = configline.substring(configline.lastIndexOf("pixelFallLeadtime=") + 18).toInt();
      Serial.println("pixelFallLeadtime= " + pixelFallLeadtime);
    }
    if (configline.indexOf("pixelFallDelay=") >= 0) {
      pixelFallDelay = configline.substring(configline.lastIndexOf("pixelFallDelay=") + 15).toInt();
      Serial.println("pixelFallDelay= " + pixelFallDelay);
    }
    if (configline.indexOf("pixelFallDuration=") >= 0) {
      pixelFallDuration = configline.substring(configline.lastIndexOf("pixelFallDuration=") + 18).toInt();
      Serial.println("pixelFallDuration= " + pixelFallDuration);
    }
    
    // starrySky
    if (configline.indexOf("starrySkyCheckbox=") >= 0) {
      starrySkyCheckbox = configline.substring(configline.lastIndexOf("starrySkyCheckbox=") + 18);
      starrySkyCheckbox.trim();
      Serial.println("starrySkyCheckbox= " + starrySkyCheckbox);
    }
    if (configline.indexOf("starrySkyLeadtime=") >= 0) {
      starrySkyLeadtime = configline.substring(configline.lastIndexOf("starrySkyLeadtime=") + 18).toInt();
      Serial.println("starrySkyLeadtime= " + starrySkyLeadtime);
    }
    if (configline.indexOf("starrySkyDelay=") >= 0) {
      starrySkyDelay = configline.substring(configline.lastIndexOf("starrySkyDelay=") + 15).toInt();
      Serial.println("starrySkyDelay= " + starrySkyDelay);
    }
    if (configline.indexOf("starrySkyDuration=") >= 0) {
      starrySkyDuration = configline.substring(configline.lastIndexOf("starrySkyDuration=") + 18).toInt();
      Serial.println("starrySkyDuration= " + starrySkyDuration);
    }
    if (configline.indexOf("starrySkyStarCount=") >= 0) {
      starrySkyStarCount = configline.substring(configline.lastIndexOf("starrySkyStarCount=") + 19).toInt();
      Serial.println("starrySkyStarCount= " + starrySkyStarCount);
    }
    
    // growingStar
    if (configline.indexOf("growingStarCheckbox=") >= 0) {
      growingStarCheckbox = configline.substring(configline.lastIndexOf("growingStarCheckbox=") + 20);
      growingStarCheckbox.trim();
      Serial.println("growingStarCheckbox= " + growingStarCheckbox);
    }
    if (configline.indexOf("growingStarLeadtime=") >= 0) {
      growingStarLeadtime = configline.substring(configline.lastIndexOf("growingStarLeadtime=") + 20).toInt();
      Serial.println("growingStarLeadtime= " + growingStarLeadtime);
    }
    if (configline.indexOf("growingStarDelay=") >= 0) {
      growingStarDelay = configline.substring(configline.lastIndexOf("growingStarDelay=") + 17).toInt();
      Serial.println("growingStarDelay= " + growingStarDelay);
    }
    if (configline.indexOf("growingStarDuration=") >= 0) {
      growingStarDuration = configline.substring(configline.lastIndexOf("growingStarDuration=") + 20).toInt();
      Serial.println("growingStarDuration= " + growingStarDuration);
    }
    
    // christmaSymbols
    if (configline.indexOf("christmasSymbolsCheckbox=") >= 0) {
      christmasSymbolsCheckbox = configline.substring(configline.lastIndexOf("christmasSymbolsCheckbox=") + 25);
      christmasSymbolsCheckbox.trim();
      Serial.println("christmasSymbolsCheckbox= " + christmasSymbolsCheckbox);
    }
    if (configline.indexOf("christmasSymbolsLeadtime=") >= 0) {
      christmasSymbolsLeadtime = configline.substring(configline.lastIndexOf("christmasSymbolsLeadtime=") + 25).toInt();
      Serial.println("christmasSymbolsLeadtime= " + christmasSymbolsLeadtime);
    }
    if (configline.indexOf("christmasSymbolsDelay=") >= 0) {
      christmasSymbolsDelay = configline.substring(configline.lastIndexOf("christmasSymbolsDelay=") + 22).toInt();
      Serial.println("christmasSymbolsDelay= " + christmasSymbolsDelay);
    }
    if (configline.indexOf("christmasSymbolsDuration=") >= 0) {
      christmasSymbolsDuration = configline.substring(configline.lastIndexOf("christmasSymbolsDuration=") + 25).toInt();
      Serial.println("christmasSymbolsDuration= " + christmasSymbolsDuration);
    }

    // moveChristmaSymbols
    if (configline.indexOf("moveChristmasSymbolsCheckbox=") >= 0) {
      moveChristmasSymbolsCheckbox = configline.substring(configline.lastIndexOf("moveChristmasSymbolsCheckbox=") + 29);
      moveChristmasSymbolsCheckbox.trim();
      Serial.println("moveChristmasSymbolsCheckbox= " + moveChristmasSymbolsCheckbox);
    }
    if (configline.indexOf("moveChristmasSymbolsLeadtime=") >= 0) {
      moveChristmasSymbolsLeadtime = configline.substring(configline.lastIndexOf("moveChristmasSymbolsLeadtime=") + 29).toInt();
      Serial.println("moveChristmasSymbolsLeadtime= " + moveChristmasSymbolsLeadtime);
    }
    if (configline.indexOf("moveChristmasSymbolsDelay=") >= 0) {
      moveChristmasSymbolsDelay = configline.substring(configline.lastIndexOf("moveChristmasSymbolsDelay=") + 26).toInt();
      Serial.println("moveChristmasSymbolsDelay= " + moveChristmasSymbolsDelay);
    }
    if (configline.indexOf("moveChristmasSymbolsDuration=") >= 0) {
      moveChristmasSymbolsDuration = configline.substring(configline.lastIndexOf("moveChristmasSymbolsDuration=") + 29).toInt();
      Serial.println("moveChristmasSymbolsDuration= " + moveChristmasSymbolsDuration);
    }
    if (configline.indexOf("moveChristmasSymbolsSymbol=") >= 0) {
      moveChristmasSymbolsSymbol = configline.substring(configline.lastIndexOf("moveChristmasSymbolsSymbol=") + 27).toInt();
      Serial.println("moveChristmasSymbolsSymbol= " + moveChristmasSymbolsSymbol);
    }
   
  }  
  fr.close();
  Serial.println();
}

// =======================================================================

byte dig[6] = {0,0,0,0,0,0};
byte dig2[6] = {0,0,0,0,0,0};
byte digold[6] = {0,0,0,0,0,0};
byte digtrans[6] = {0,0,0,0,0,0};
int colons = 0;
long dotTime = 0;
uint8_t scrollInProgress = 0;
uint8_t transPos = 0;

void showClock() {

  byte digPos[6] = {3, 12, 24, 33, 45, 54};
  uint8_t shiftRange = 18;   // um diesen Betrag soll der Ausschnitt im scrollDigits-Array beim Digit-Wechsel verschoben werden
  
  // Rahmen zeichnen
  for (uint8_t x = 0; x < 64; x++){
    drawPoint(x, 0, 1);
    drawPoint(x, 19, 1);
  }
  for (uint8_t y = 0; y < 20; y++){
    drawPoint(0, y, 1);
    drawPoint(63, y, 1);
  } 
  
  getTimeLocal();

  if (animClockCheckbox == "checked") {
    // animierte Uhr
    uint8_t i;
    if (scrollInProgress == 0){   // waerend des Scrollens die Stellen der aktuellen Zeit nicht neu in dig[] einlesen
      scrollInProgress = shiftRange;
      for (i = 0; i < 6; i++) digold[i] = dig[i];
      dig[0] = h / 10;
      dig[1] = h % 10;
      dig[2] = m / 10;
      dig[3] = m % 10;
      dig[4] = s / 10;
      dig[5] = s % 10;
      for (i = 0; i < 6; i++) digtrans[i] = (dig[i] == digold[i]) ? 0 : shiftRange;   // wenn sich ein Digit geaendert hat (digold <> dig) wird digtrans[] 0, sonst shifRange
    } else {
      scrollInProgress--;
    } 

    for (i = 0; i < 6; i++){
      if (h < 10){ 
        if (i > 0){                 // fuehrende 0 nicht anzeigen
          if (digtrans[i] == 0){    // wenn Digit sich nicht geaendert hat
            transPos = 0;
            drawImage(digPos[i], 1, 8, 16, scrollDigits + dig[i] * shiftRange);
          } else {
            transPos = 18 - digtrans[i];
            dig2[i] = dig[i];
            if (dig[i] == 0) dig2[i] = 10;
            drawImage(digPos[i], 1, 8, 16, scrollDigits + transPos + (dig2[i] - 1) * shiftRange);
            digtrans[i]--;
          }
        }
      } else {
        if (digtrans[i] == 0){    // wenn Digit sich nicht geaendert hat
            transPos = 0;
            drawImage(digPos[i], 2, 8, 16, scrollDigits + dig[i] * shiftRange + 1);
        } else {
          transPos = 18 - digtrans[i];
          dig2[i] = dig[i];
          if (dig[i] == 0) dig2[i] = 10;
          drawImage(digPos[i], 2, 8, 16, scrollDigits + transPos + (dig2[i] - 1) * shiftRange + 1);
          digtrans[i]--;
        }
      } 
    }
    
    transPos = 0;
    delay(20);

  } else {
    // normale Uhr
    dig[0] = h / 10;
    dig[1] = h % 10;
    dig[2] = m / 10;
    dig[3] = m % 10;
    dig[4] = s / 10;
    dig[5] = s % 10;

    if (h > 9) {    // fuehrende 0 (wenn h < 10) nicht anzeigen
      drawImage(digPos[0], 2, 8, 16, scrollDigits + dig[0] * 18 + 1);
    }
    drawImage(digPos[1], 2, 8, 16, scrollDigits + dig[1] * 18 + 1);
    drawImage(digPos[2], 2, 8, 16, scrollDigits + dig[2] * 18 + 1);
    drawImage(digPos[3], 2, 8, 16, scrollDigits + dig[3] * 18 + 1);
    drawImage(digPos[4], 2, 8, 16, scrollDigits + dig[4] * 18 + 1);
    drawImage(digPos[5], 2, 8, 16, scrollDigits + dig[5] * 18 + 1);
  }

  if (colonsCheckbox == "checked") {
    if (millis() - dotTime > 500) {
      dotTime = millis();
      colons = !colons;   //toggle colons
    }
  } else {
    colons = 1;
  }
  if (colons) {
    drawPoint(21,6,1);
    drawPoint(21,12,1);
    drawPoint(22,6,1);
    drawPoint(22,12,1);
    drawPoint(42,6,1);
    drawPoint(42,12,1);
    drawPoint(43,6,1);
    drawPoint(43,12,1);
    drawPoint(21,7,1);
    drawPoint(21,13,1);
    drawPoint(22,7,1);
    drawPoint(22,13,1);
    drawPoint(42,7,1);
    drawPoint(42,13,1);
    drawPoint(43,7,1);
    drawPoint(43,13,1);
  } else {
    drawPoint(21,6,0);
    drawPoint(21,12,0);
    drawPoint(22,6,0);
    drawPoint(22,12,0);
    drawPoint(42,6,0);
    drawPoint(42,12,0);  
    drawPoint(43,6,0);
    drawPoint(43,12,0);  
    drawPoint(21,7,0);
    drawPoint(21,13,0);
    drawPoint(22,7,0);
    drawPoint(22,13,0);
    drawPoint(42,7,0);
    drawPoint(42,13,0);  
    drawPoint(43,7,0);
    drawPoint(43,13,0);  
  }
}

// =======================================================================

void getWeatherData()
{ 
  String weatherGetString = "Hole Internet-Wetter-Daten.                       ";
  Serial.println("Getting online weather data for cityid " + cityID + "...");
  Serial.println("Connecting to " + String(weatherHost) + "...");
  WiFiClient client;
  if (client.connect(weatherHost, 80)) {
    client.println(String("GET /data/2.5/weather?id=") + cityID + "&units=metric&appid=" + weatherKey + weatherLang + "\r\n" +
                   "Host: " + weatherHost + "\r\nUser-Agent: ArduinoWiFi/1.1\r\n" +
                   "Connection: close\r\n\r\n");
  } else {
    Serial.println("Connection failed!");
    weatherGetString = "Fehlgeschlagen!                      ";
    weatherString = "Online-Wetter: keine Daten";
    return;
  }
  String line;
  int repeatCounter = 0;
  while (!client.available() && repeatCounter < 10) {
    delay(500);
    Serial.println("...");
    repeatCounter++;
  }
  while (client.available()) {
    char c = client.read();
    line += c;
  }
  client.stop();
  Serial.println(line);
  DynamicJsonDocument jsonDoc(2048);
  DeserializationError error = deserializeJson(jsonDoc, line);
  if (error)   {
    Serial.print("deserializeJson() failed with code ");
    Serial.println(error.c_str());
    weatherGetString = "Fehlgeschlagen!                      ";
    weatherString = "Online-Wetter: keine Daten";
    return;
  }
  weatherGetString = "OK.                      ";
  weatherLocation = jsonDoc["name"].as<String>();
  onlinetemp = jsonDoc["main"]["temp"];
  humidity = jsonDoc["main"]["humidity"];
  pressure = jsonDoc["main"]["pressure"];
  tempMin = jsonDoc["main"]["temp_min"];
  tempMax = jsonDoc["main"]["temp_max"];
  windSpeed = jsonDoc["wind"]["speed"];
  windDeg = jsonDoc["wind"]["deg"];
  clouds = jsonDoc["clouds"]["all"];
  weatherString = "";
  if (tempCheckbox == "checked") {
    if (weatherLocationCheckbox == "checked") {
      weatherString = "Temperatur in " + weatherLocation + ": " + String(onlinetemp, 1) + "`C    ";   // `-Zeichen ist im Font als Grad definiert
    } else {
      weatherString = "Temperatur: " + String(onlinetemp, 1) + "`C    ";
    }
  }
  if (humidityCheckbox == "checked") {
    weatherString += "Luftfeuchte: " + String(humidity) + "%    ";
  }
  if (pressureCheckbox == "checked") {
    weatherString += "Luftdruck: " + String(pressure) + "hPa    ";
  }
  if (rainCheckbox == "checked") {
    weatherString += "Niederschlagsrisiko: " + String(clouds) + "%    ";
  }
  if (windCheckbox == "checked") {
    weatherString += "Wind: " + String(windSpeed, 1) + "m/s    ";
  }
}

// =======================================================================

void wipeHorizontal() {
  //clearMatrix();
  for (uint8_t i = 0; i < 64; i++) {
    for (uint8_t j = 0; j < NUM_ROWS; j++) {
      drawPoint(i, j, 1);
      delay(1);
    }
  }
  for (uint8_t i = 0; i < 64; i++) {
    for (uint8_t j = 0; j < NUM_ROWS; j++) {
      drawPoint(i, j, 0);
      delay(1);
    }
  }
}

// =======================================================================

void wipeHorizontalLine() {
  for (uint8_t i = 0; i < 64; i++) {
    for (uint8_t j = 0; j < NUM_ROWS; j++) {
      drawPoint(63 - i, j, 1);
    }
    delay(45);
    for (uint8_t j = 0; j < NUM_ROWS; j++) {
      drawPoint(63 - i, j, 0);
    }
  }
}

// =======================================================================

void wipeVertical() {
  for (uint8_t j = 0; j < NUM_ROWS; j++) {
    for (uint8_t i = 0; i < 64; i++) {
      drawPoint(i, j, 1);
      delay(1);
    }
  } 
  for (uint8_t j = 0; j < NUM_ROWS; j++) {
    for (uint8_t i = 0; i < 64; i++) {
      drawPoint(i, j, 0);
      delay(1);
    }
  } 
}

// =======================================================================

void wipeVerticalLine() {
  for (uint8_t j = 0; j < NUM_ROWS; j++) {
    for (uint8_t i = 0; i < 64; i++) {
      drawPoint(i, j, 1);
    }
    delay(70);
    for (uint8_t i = 0; i < 64; i++) {
      drawPoint(i, j, 0);
    }
  } 
}

// =======================================================================

void wipeLeftShift() {
  Serial.println("Starte wipeLeftShift...");
  for (uint8_t shift = 0; shift < 64; shift++) {       // 64 Spalten
    for (uint8_t row = 0; row < NUM_ROWS; row++) {     // Zeile für Zeile durchgehen
      for (uint8_t zone = 0; zone < 8; zone++){        // alle 8 Zonen der aktuellen Zeile durchgehen
        uint8_t *pDst = displaybuf + row * 8 + zone;   // Deklaration Pointer auf erstes (linkes) Byte der aktuellen Zeile des Displaypuffers
        uint8_t *pSrc = pDst + 1;                      // Deklaration Pointer pSrc ein Byte (1 Zone) weiter als pDst
        *pDst = *pDst << 1;                            // alle Bits der Zone um eins nach links schieben
        if (zone == 7) {
          bitWrite(*pDst, 0 , bitRead(0x00, 7));       // fuer Zone 8 holen wir uns Nullen (sonst wäre es das nächste Byte aus displaybuf und am Ende sogar irgendwas außerhalb)
        } else {
          bitWrite(*pDst, 0 , bitRead(*pSrc, 7));      // linkes Bit der rechten Zone hierher als rechtes Bit holen
        }
      } 
    }
    delay(40);
  }
} 

// =======================================================================

void wipeRandom(){
  uint8_t wipeType = random(0,5);   // bei random() ist der Startwert inklusive, der Endwert exklusive
  Serial.println("wipeRandomType: " + String(wipeType));
  switch (wipeType) {
    case 0:
      wipeHorizontal();
      break;
    case 1:
       wipeHorizontalLine();
       break;
    case 2:
      wipeVertical();
      break;
    case 3:
      wipeVerticalLine();
      break;
    case 4:
      wipeLeftShift();
      break;
  } 
}

// =======================================================================

// Arrays fuer die 8 Zonen + Puffer fuer neu reinlaufendes Zeichen
uint8_t  buf1[NUM_ROWS];
uint8_t  buf2[NUM_ROWS];
uint8_t zone1[NUM_ROWS];
uint8_t zone2[NUM_ROWS];
uint8_t zone3[NUM_ROWS];
uint8_t zone4[NUM_ROWS];
uint8_t zone5[NUM_ROWS];
uint8_t zone6[NUM_ROWS];
uint8_t zone7[NUM_ROWS];
uint8_t zone8[NUM_ROWS];

uint8_t msglineindex, charWidth;

void clearLastScroll() {                         // Reste vom vorherigen Scrollen loeschen
  for (uint8_t row = 0; row < NUM_ROWS; row++) {
    buf1[row]  = 0;
    buf2[row]  = 0;
    zone1[row] = 0;
    zone2[row] = 0;
    zone3[row] = 0;
    zone4[row] = 0;
    zone5[row] = 0;
    zone6[row] = 0;
    zone7[row] = 0;
    zone8[row] = 0;
  }
}

// =======================================================================

void textScroll_16x20(const char *s, uint8_t sdelay) {                        // s = Text (Array); sdelay = Speed (Verzögerung)
  clearLastScroll();                                                          // Reste vom vorherigen Scrollen loeschen
  Serial.println("Start hTextScroll16x20...");
  while (*s) {
    unsigned char msglineindex = *s;                                          // ACSII-Wert des aktuellen Zeichens
    switch (msglineindex) {                                                   // Umlaute auf unseren Zeichensatz mappen
      case 196: msglineindex = 95 + 32;   // Ä
        break;
      case 214: msglineindex = 96 + 32;   // Ö
        break;
      case 220: msglineindex = 97 + 32;   // Ü
        break;
      case 228: msglineindex = 98 + 32;   // ä
        break;
      case 246: msglineindex = 99 + 32;   // ö
        break;
      case 252: msglineindex = 100 + 32;  // ü
        break;
      case 223: msglineindex = 101 + 32;  // ß
        break;
    }

    uint8_t bytecount = 0;
    charWidth = ArialRound[(msglineindex - 32) * 41 + 40];                  // Zeichenbreite (41. Byte jedes Zeichens im Font)
    for (uint8_t row = 0; row < NUM_ROWS; row++) {                          // nächstes Zeichen in Puffer laden (2 Byte breit)
      buf1[row] = ArialRound[(msglineindex - 32) * 41 + row + bytecount];   // erstes Byte des 2 Byte breiten Zeichens
      bytecount++;
      buf2[row] = ArialRound[(msglineindex - 32) * 41 + row + bytecount];   // zweites Byte des 2 Byte breiten Zeichens
    }
    for (uint8_t shift = 0; shift < charWidth; shift++) {                   // Bit fuer Bit um Zeichenbreite des aktuellen Zeichens nach links shiften
      for (uint8_t row = 0; row < NUM_ROWS; row++) {                        // dabei Zeile für Zeile durchgehen
        uint8_t *pDst = displaybuf + row * 8;                               // Pointer auf erstes (linkes) Byte der aktuellen Zeile des Displaypuffers setzen
        // jede Zeile ist in 8 Zonen (8 Bytes) aufgeteilt
        zone8[row] = zone8[row] << 1;                                       // alle Bits der Zone eins nach links schieben
        bitWrite(zone8[row],0 , bitRead(zone7[row],7));                     // linkes Bit der rechten Zone hierher als rechtes Bit holen
        *pDst = zone8[row];                                                 // Zone (Byte) in Displaypuffer uebertragen
        zone7[row] = zone7[row] << 1;
        bitWrite(zone7[row],0 , bitRead(zone6[row],7));
        pDst++;                                                             // Pointer auf naechstes Byte der aktuellen Zeile setzen
        *pDst = zone7[row];
        zone6[row] = zone6[row] << 1;
        bitWrite(zone6[row],0 , bitRead(zone5[row],7));
        pDst++;
        *pDst = zone6[row];
        zone5[row] = zone5[row] << 1;
        bitWrite(zone5[row],0 , bitRead(zone4[row],7));
        pDst++;
        *pDst = zone5[row];
        zone4[row] = zone4[row] << 1;
        bitWrite(zone4[row],0 , bitRead(zone3[row],7));
        pDst++;
        *pDst = zone4[row];
        zone3[row] = zone3[row] << 1;
        bitWrite(zone3[row],0 , bitRead(zone2[row],7));
        pDst++;
        *pDst = zone3[row];
        zone2[row] = zone2[row] << 1;
        bitWrite(zone2[row],0 , bitRead(zone1[row],7));
        pDst++;
        *pDst = zone2[row];
        zone1[row] = zone1[row] << 1;
        bitWrite(zone1[row],0 , bitRead(buf1[row],7));
        pDst++;
        *pDst = zone1[row];
        buf1[row] = buf1[row] << 1;
        bitWrite(buf1[row],0 , bitRead(buf2[row],7));
        buf2[row] = buf2[row] << 1;
      }
      delay(sdelay);
    }
  s++;
  }
  clearMatrix();
  delay(1000);
}

// =======================================================================

void textScroll_8x16(const char *s, uint8_t sdelay) {                         // s = Text (Array); sdelay = Speed (Verzögerung)
  clearLastScroll();                                                          // Reste vom vorherigen Scrollen loeschen
  Serial.println("Start hTextScroll8x16...");
  while (*s) {
    unsigned char msglineindex = *s;                                          // ACSII-Wert des aktuellen Zeichens
    switch (msglineindex) {                                                   // Umlaute auf unseren Zeichensatz mappen
      case 196: msglineindex = 95 + 32;   // Ä
        break;
      case 214: msglineindex = 96 + 32;   // Ö
        break;
      case 220: msglineindex = 97 + 32;   // Ü
        break;
      case 228: msglineindex = 98 + 32;   // ä
        break;
      case 246: msglineindex = 99 + 32;   // ö
        break;
      case 252: msglineindex = 100 + 32;  // ü
        break;
      case 223: msglineindex = 101 + 32;  // ß
        break;
    }

    charWidth = smallFont[(msglineindex - 32) * 17 + 16];                   // Zeichenbreite (17. Byte eines Zeichens im Font)
    for (uint8_t row = 0; row < NUM_ROWS - (NUM_ROWS - 16); row++) {        // nächstes Zeichen zeilenweise (20 oder 16 Zeilen) in Puffer laden (1 Byte)
      buf1[row] = smallFont[(msglineindex - 32) * 17 + row];
    }
    for (uint8_t shift = 0; shift < charWidth; shift++) {                   // Bit fuer Bit um Zeichenbreite des aktuellen Zeichens nach links shiften
      for (uint8_t row = 0; row < NUM_ROWS - (NUM_ROWS - 16); row++) {      // dabei Zeile für Zeile durchgehen
        uint8_t *pDst = displaybuf + row * 8 + (NUM_ROWS - 16) * 4;         // Pointer auf erstes (linkes) Byte der aktuellen Zeile des Displaypuffers setzen (Start in 3. Zeile)
        // jede Zeile ist in 8 Zonen (8 Bytes) aufgeteilt - links ist Zone 1 - rechts ist Zone 8 - daneben der Puffer fuer neues Zeichen
        zone8[row] = zone8[row] << 1;                                       // alle Bits der Zone eins nach links schieben
        bitWrite(zone8[row],0 , bitRead(zone7[row],7));                     // linkes Bit der Zone nebenan rechts hierher als rechtes Bit holen
        *pDst = zone8[row];                                                 // Zone (Byte) in Displaypuffer uebertragen
        zone7[row] = zone7[row] << 1;
        bitWrite(zone7[row],0 , bitRead(zone6[row],7));
        pDst++;                                                             // Pointer auf naechstes Byte der aktuellen Zeile setzen
        *pDst = zone7[row];
        zone6[row] = zone6[row] << 1;
        bitWrite(zone6[row],0 , bitRead(zone5[row],7));
        pDst++;
        *pDst = zone6[row];
        zone5[row] = zone5[row] << 1;
        bitWrite(zone5[row],0 , bitRead(zone4[row],7));
        pDst++;
        *pDst = zone5[row];
        zone4[row] = zone4[row] << 1;
        bitWrite(zone4[row],0 , bitRead(zone3[row],7));
        pDst++;
        *pDst = zone4[row];
        zone3[row] = zone3[row] << 1;
        bitWrite(zone3[row],0 , bitRead(zone2[row],7));
        pDst++;
        *pDst = zone3[row];
        zone2[row] = zone2[row] << 1;
        bitWrite(zone2[row],0 , bitRead(zone1[row],7));
        pDst++;
        *pDst = zone2[row];
        zone1[row] = zone1[row] << 1;
        bitWrite(zone1[row],0 , bitRead(buf1[row],7));
        pDst++;
        *pDst = zone1[row];
        buf1[row] = buf1[row] << 1;
      }
      delay(sdelay);
    }
  s++;
  }
  clearMatrix();
  delay(1000);
}

// =======================================================================

void drawChristmasSymbols() {
  Serial.println("Start drawChristmasSymbols...");
  clkTimeEffect = millis();
  while (millis() < (christmasSymbolsDuration.toInt() * 1000) + clkTimeEffect) {
    uint8_t x = random(0,48);
    uint8_t y = random(0,4);
    uint8_t s = random(1,6);
    drawImage( x, y, 16, 16, christmasSymbols16x16 + s * 32);
    delay(christmasSymbolsDelay.toInt() * 100);
    clearMatrix();
  }
}  
  
// =======================================================================

void moveChristmasSymbols(uint8_t symbol) {
  Serial.println("Start moveChristmasSymbols...");
  Serial.println("randomSymbol= " + String(symbol));
  clkTimeEffect = millis();
  uint8_t x = random(0,48);   // zufaelliger Startwert x
  uint8_t y = random(0,4);    // zufaelliger Startwert y
  while (millis() < (moveChristmasSymbolsDuration.toInt() * 1000) + clkTimeEffect) {
    uint8_t direction = random(0,4);   // zufaellige Richtung
    uint8_t steps = random(0,24);
    for (uint8_t i = 0; i < steps; i++) {
      drawImage( x, y, 16, 16, christmasSymbols16x16 + 32 * symbol);
      delay(moveChristmasSymbolsDelay.toInt());
      drawImage( x, y, 16, 16, christmasSymbols16x16);  // alte Position loeschen (erste 32 Byte im Array )
      switch (direction){
        case 0:
          // left
          if (x > 0){
            x--;
          } else {
            direction = 1;
          }    
          break;
        case 1:
          // right
          if (x < 47){
            x++;
          } else {
            direction = 0;
          }  
          break;
        case 2:
          // up;
          if (y > 0){
            y--;
          } else{
            direction = random(2,4);
          } 
          break;
        case 3:
          // down;
          if (y < 3){
            y++;
          } else{
            direction = random(0,4);
          } 
          break;
      }
    } 
  }
  delay(500);
  clearMatrix();
}  
  
// =======================================================================  

void growingStar() {
  Serial.println("Start growingStar...");
  clkTimeEffect = millis();
  while (millis() < (growingStarDuration.toInt() * 1000) + clkTimeEffect) {
    uint8_t x = random(0,48);
    uint8_t y = random(0,6);
    for (uint8_t i = 0; i < 9; i++) {
      drawImage( x, y, 16, 15, growingStarImage + i * 30);
      delay(growingStarDelay.toInt());
    }
    uint8_t goback = random(0, 2);
    if (goback == 1){
      for (uint8_t i = 0; i < 9; i++) {
        drawImage( x, y, 16, 15, growingStarImage + (8 - i) * 30);
        delay(growingStarDelay.toInt());
      }
    } 
    delay(300);
    clearMatrix();
  }
}  
  
// ======================================================================= 

void pixelFall() {   // Pixel vertikal animiert
  Serial.println("Start pixelFall...");
  clearMatrix();
  clkTimeEffect = millis();
  
  uint16_t yPos[64];
  uint8_t startDelay[64];
  uint8_t speedDelay[64];
  uint8_t startDelayCount[64];
  uint8_t speedDelayCount[64];
  uint8_t pixelDone[64];
  uint8_t allPixelsDone = 0;

  for (uint8_t xPos = 0; xPos < 64; xPos++) {
    yPos[xPos] = 0;
    startDelay[xPos] = random(0,51);
    speedDelay[xPos] = random(0,5);
    startDelayCount[xPos] = 0;
    speedDelayCount[xPos] = 0;
    pixelDone[xPos] = 0;
  }

  while ( allPixelsDone < 64 ) {
    for (uint16_t xPos = 0; xPos < 64; xPos++) {
      if ( (millis() > (pixelFallDuration.toInt() * 1000) + clkTimeEffect) and yPos[xPos] == 20 ) {   // wenn Zeit abgelaufen und Pixel unten raus
        drawPoint(xPos, 19, 0);                                                                       // alten Pixel in Zeile 20 loeschen, da naechstes if nicht mehr greift
        pixelDone[xPos] = 1;                                                                          // dann Pixel an aktueller x-Position als fertig kennzeichnen
      }
      if ( startDelayCount[xPos] >= startDelay[xPos] and pixelDone[xPos] == 0 ) {                     // Start des Pixel um eine zufaellige Zeit verzögern
        drawPoint(xPos, yPos[xPos], 1);                                                               // Pixel zeichnen
        if (yPos[xPos] > 0) drawPoint(xPos, yPos[xPos] - 1, 0);                                       // alten Pixel loeschen (nur, wenn sein y mindestens 0 war, also aktueller y mindestens 1)
        if (speedDelayCount[xPos] >= speedDelay[xPos]) {                                              // wenn Speed Delay erreicht, Pixel eins weiter runter
          speedDelayCount[xPos] = 0;
          yPos[xPos]++;                                                                               // Pixel eins runter
        }
        speedDelayCount[xPos]++;
        if ( yPos[xPos] > 20 ) {                                                                      // letzter Zustand erreicht (Pixel unten rausgescrollt)
          yPos[xPos] = 0;
          startDelayCount[xPos] = 0;
          startDelay[xPos] = random(0,51);
          speedDelay[xPos] = random(0,5);
        }
      }
      startDelayCount[xPos]++;
      allPixelsDone = 0;
      for (uint8_t i = 0; i < 64; i++) {   // checken, ob alle Pixel fertig gescrollt sind
        allPixelsDone += pixelDone[i];     // wenn Pixel fertig, wird allPixelDone fuer jeden fertigen Pixel um eins hochgezaehlt
      }
    }
    delay(pixelFallDelay.toInt());
  }
  
  delay(500);
  clearMatrix();
}

// =======================================================================

void snowFallMulti() {   // Schneeflocken vertikal scrollen
  Serial.println("Start snowFallMulti...");
  clearMatrix();
  clkTimeEffect = millis();
  
  uint8_t starImageCutout[8];
  uint8_t startDelay[8];
  uint8_t speedDelay[8];
  uint8_t startDelayCount[8];
  uint8_t speedDelayCount[8];
  uint8_t starDone[8];
  uint8_t allStarsDone = 0;

  for (uint8_t zone = 0; zone < 8; zone++) {
    starImageCutout[zone] = 0;
    startDelay[zone] = random(0,51);
    speedDelay[zone] = random(0,5);
    startDelayCount[zone] = 0;
    speedDelayCount[zone] = 0;
    starDone[zone] = 0;
  }

  while ( allStarsDone < 8 ) {
    for (uint8_t zone = 0; zone < 8; zone++) {
      if ( (millis() > (snowFallMultiDuration.toInt() * 1000) + clkTimeEffect) and starImageCutout[zone] == 0 ) {   // wenn Zeit abgelaufen und Flocke unten raus
        starDone[zone] = 1;
      }
      if ( startDelayCount[zone] >= startDelay[zone] and starDone[zone] == 0 ) {   // Start der Flocke um eine zufaellige Zeit verzögern
        drawImage( zone * 8, 0, 8, 20, starImage + 27 - starImageCutout[zone] );   // passenden Ausschnitt aus starImage-Array zeichnen
        if (speedDelayCount[zone] >= speedDelay[zone]) {
          speedDelayCount[zone] = 0;
          starImageCutout[zone]++;   // Flocke eins runter
        }
        speedDelayCount[zone]++;
        if ( starImageCutout[zone] == 28 ) {   // letzter Zustand erreicht (Flocke unten rausgescrollt bzw. Ausschnitt am Anfang des starImage-Arrays)
          starImageCutout[zone] = 0;
          startDelayCount[zone] = 0;
          startDelay[zone] = random(0,51);
          speedDelay[zone] = random(0,6);
        }
      }
      startDelayCount[zone]++;
      allStarsDone = 0;
      for (uint8_t i = 0; i < 8; i++) {   // checken, ob alle Flocken fertig gescrollt sind
        allStarsDone += starDone[i];
      }
    }
    delay(snowFallMultiDelay.toInt());
  }
  
  delay(1000);
  clearMatrix();
}

// =======================================================================

void snowFallSingle() {   // einzelne Schneeflocke an verschiedenen Positionen vertikal scrollen
  Serial.println("Start snowFallSingle...");
  clearMatrix();
  clkTimeEffect = millis();
  while (millis() < (snowFallSingleDuration.toInt() * 1000) + clkTimeEffect) {
    uint8_t x = random(0,57);
    for (uint8_t i = 0; i <= 27; i++) {
      drawImage( x, 0, 8, 20, starImage + 27 - i);
      delay(snowFallSingleDelay.toInt());
    } 
  }
  delay(500);
  clearMatrix();
}

// =======================================================================

void starrySky() {   // funkelnder Sternenhimmel
  Serial.println("Start starrySky...");
  clearMatrix();
  uint8_t xPos[1000];
  uint8_t yPos[1000];
  for (uint16_t i = 0; i < starrySkyStarCount.toInt(); i++) {
    xPos[i] = 0;
    yPos[i] = 0;
  }
  clkTimeEffect = millis();
  while (millis() < (starrySkyDuration.toInt() * 1000) + clkTimeEffect) {
    for (uint16_t i = 0; i < starrySkyStarCount.toInt(); i++) {
      drawPoint(xPos[i], yPos[i], 0);
      xPos[i] = random(0,64);
      yPos[i] = random(0,20);
      drawPoint(xPos[i], yPos[i], 1);
      delay(starrySkyDelay.toInt());
    }
  }
  for (uint16_t i = 0; i < starrySkyStarCount.toInt(); i++) {
    delay(starrySkyDelay.toInt());
    drawPoint(xPos[i], yPos[i], 0);
  }
  delay(1000);
  clearMatrix();
}

// =======================================================================

void setup() {  
  Serial.begin(115200);
  delay(10);
  Serial.println('\n');

  initLittleFS();
  readConfig();

  AsyncWiFiManager wifiManager(&server,&dns);
  
  if(!wifiManager.autoConnect("LED-Lichternetz")) {
    Serial.println("Failed to connect and hit timeout!");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  Serial.println("Connected.");

  Serial.println("\n");
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());
  Serial.println("\n");

  if (weatherCheckbox == "checked") {
    Serial.println("Getting online weather data...");
    getWeatherData();
  }
  
  noInterrupts();
  pinMode(cs0, OUTPUT);
  pinMode(cs1, OUTPUT);
  pinMode(a0,  OUTPUT);
  pinMode(a1,  OUTPUT);
  pinMode(a2,  OUTPUT);
  pinMode(clk, OUTPUT);
  pinMode(sdi, OUTPUT);
  pinMode(le,  OUTPUT);
  #ifdef DEV
    digitalWrite(cs1, HIGH);
  #else
    pinMode(oe,  OUTPUT);
    digitalWrite(oe, HIGH);
  #endif

  // https://www.instructables.com/id/MULTI-EFFECTS-INTERNET-CLOCK/
  // With following setup, Timer 1 will run at 5MHz (80MHz/16=5MHz) or 1/5MHz = 0.2us.
  // When we set timer1_write (500), this means the interrupt will be called timer1_ISR() every 500 x 0.2us = 100us.
  timer1_isr_init();
  timer1_attachInterrupt(timer1_ISR);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  //timer1_write(500);
  timer1_write(1000);

  interrupts();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/ota", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Switching off display due to unfavorable states...");
    #ifdef DEV
      digitalWrite(cs1, HIGH);     // switch off display due to unfavorable states
    #else
      pinMode(oe,  OUTPUT);
      digitalWrite(oe, HIGH);
    #endif
    request->redirect("/update");
  });

  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {

    // matrix appearance
    if (request->hasParam(PARAM_INPUT_01)) {
      mirrorCheckbox = request->getParam(PARAM_INPUT_01)->value();
      mirror = 1;
    } else {
      mirrorCheckbox = "unchecked";
      mirror = 0;
    }
    if (request->hasParam(PARAM_INPUT_02)) {
      reverseCheckbox = request->getParam(PARAM_INPUT_02)->value();
      mask = 0xff;
    } else {
      reverseCheckbox = "unchecked";
      mask = 0x00;
    }

    // clock
    if (request->hasParam(PARAM_INPUT_03)) {
      animClockCheckbox = request->getParam(PARAM_INPUT_03)->value();
    } else {
      animClockCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_04)) {
      colonsCheckbox = request->getParam(PARAM_INPUT_04)->value();
    } else {
      colonsCheckbox = "unchecked";
    }

    // scrollText1
    if (request->hasParam(PARAM_INPUT_05)) scrollText1 = request->getParam(PARAM_INPUT_05)->value();
    if (request->hasParam(PARAM_INPUT_06)) {
      scrollText1Checkbox = request->getParam(PARAM_INPUT_06)->value();
    } else {
      scrollText1Checkbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_07)) scrollText1Leadtime = request->getParam(PARAM_INPUT_07)->value();
    if (request->hasParam(PARAM_INPUT_08)) scrollText1Delay = request->getParam(PARAM_INPUT_08)->value();
    if (request->hasParam(PARAM_INPUT_09)) scrollText1Font = request->getParam(PARAM_INPUT_09)->value();
    
    // scrollText2
    if (request->hasParam(PARAM_INPUT_10)) scrollText2 = request->getParam(PARAM_INPUT_10)->value();
    if (request->hasParam(PARAM_INPUT_11)) {
      scrollText2Checkbox = request->getParam(PARAM_INPUT_11)->value();
    } else {
      scrollText2Checkbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_12)) scrollText2Leadtime = request->getParam(PARAM_INPUT_12)->value();
    if (request->hasParam(PARAM_INPUT_13)) scrollText2Delay = request->getParam(PARAM_INPUT_13)->value();
    if (request->hasParam(PARAM_INPUT_14)) scrollText2Font = request->getParam(PARAM_INPUT_14)->value();

    // scrollText3
    if (request->hasParam(PARAM_INPUT_15)) scrollText3 = request->getParam(PARAM_INPUT_15)->value();
    if (request->hasParam(PARAM_INPUT_16)) {
      scrollText3Checkbox = request->getParam(PARAM_INPUT_16)->value();
    } else {
      scrollText3Checkbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_17)) scrollText3Leadtime = request->getParam(PARAM_INPUT_17)->value();
    if (request->hasParam(PARAM_INPUT_18)) scrollText3Delay = request->getParam(PARAM_INPUT_18)->value();
    if (request->hasParam(PARAM_INPUT_19)) scrollText3Font = request->getParam(PARAM_INPUT_19)->value();

    // weather
    if (request->hasParam(PARAM_INPUT_20)) {
      weatherCheckbox = request->getParam(PARAM_INPUT_20)->value();
      updCnt = 0;
    } else {
      weatherCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_21)) weatherLeadtime = request->getParam(PARAM_INPUT_21)->value();
    if (request->hasParam(PARAM_INPUT_22)) weatherDelay = request->getParam(PARAM_INPUT_22)->value();
    if (request->hasParam(PARAM_INPUT_23)) weatherFont = request->getParam(PARAM_INPUT_23)->value();

    if (request->hasParam(PARAM_INPUT_24)) {
      weatherLocationCheckbox = request->getParam(PARAM_INPUT_24)->value();
    } else {
      weatherLocationCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_25)) cityID = request->getParam(PARAM_INPUT_25)->value();
    if (request->hasParam(PARAM_INPUT_26)) {
      dateCheckbox = request->getParam(PARAM_INPUT_26)->value();
    } else {
      dateCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_27)) {
      tempCheckbox = request->getParam(PARAM_INPUT_27)->value();
    } else {
      tempCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_28)) {
      rainCheckbox = request->getParam(PARAM_INPUT_28)->value();
    } else {
      rainCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_29)) {
      windCheckbox = request->getParam(PARAM_INPUT_29)->value();
    } else {
      windCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_30)) {
      humidityCheckbox = request->getParam(PARAM_INPUT_30)->value();
    } else {
      humidityCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_31)) {
      pressureCheckbox = request->getParam(PARAM_INPUT_31)->value();
    } else {
      pressureCheckbox = "unchecked";
    }
    
    // snowFallSingle
    if (request->hasParam(PARAM_INPUT_32)) {
      snowFallSingleCheckbox = request->getParam(PARAM_INPUT_32)->value();
    } else {
      snowFallSingleCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_33)) snowFallSingleLeadtime = request->getParam(PARAM_INPUT_33)->value();
    if (request->hasParam(PARAM_INPUT_34)) snowFallSingleDelay = request->getParam(PARAM_INPUT_34)->value();
    if (request->hasParam(PARAM_INPUT_35)) snowFallSingleDuration = request->getParam(PARAM_INPUT_35)->value();

    // snowFallMulti
    if (request->hasParam(PARAM_INPUT_36)) {
      snowFallMultiCheckbox = request->getParam(PARAM_INPUT_36)->value();
    } else {
      snowFallMultiCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_37)) snowFallMultiLeadtime = request->getParam(PARAM_INPUT_37)->value();
    if (request->hasParam(PARAM_INPUT_38)) snowFallMultiDelay = request->getParam(PARAM_INPUT_38)->value();
    if (request->hasParam(PARAM_INPUT_39)) snowFallMultiDuration = request->getParam(PARAM_INPUT_39)->value();

    // pixelFall
    if (request->hasParam(PARAM_INPUT_40)) {
      pixelFallCheckbox = request->getParam(PARAM_INPUT_40)->value();
    } else {
      pixelFallCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_41)) pixelFallLeadtime = request->getParam(PARAM_INPUT_41)->value();
    if (request->hasParam(PARAM_INPUT_42)) pixelFallDelay = request->getParam(PARAM_INPUT_42)->value();
    if (request->hasParam(PARAM_INPUT_43)) pixelFallDuration = request->getParam(PARAM_INPUT_43)->value();

    // starrySky
    if (request->hasParam(PARAM_INPUT_44)) {
      starrySkyCheckbox = request->getParam(PARAM_INPUT_44)->value();
    } else {
      starrySkyCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_45)) starrySkyLeadtime = request->getParam(PARAM_INPUT_45)->value();
    if (request->hasParam(PARAM_INPUT_46)) starrySkyDelay = request->getParam(PARAM_INPUT_46)->value();
    if (request->hasParam(PARAM_INPUT_47)) starrySkyDuration = request->getParam(PARAM_INPUT_47)->value();
    if (request->hasParam(PARAM_INPUT_48)) starrySkyStarCount = request->getParam(PARAM_INPUT_48)->value();

    // growingStar
    if (request->hasParam(PARAM_INPUT_49)) {
      growingStarCheckbox = request->getParam(PARAM_INPUT_49)->value();
    } else {
      growingStarCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_50)) growingStarLeadtime = request->getParam(PARAM_INPUT_50)->value();
    if (request->hasParam(PARAM_INPUT_51)) growingStarDelay = request->getParam(PARAM_INPUT_51)->value();
    if (request->hasParam(PARAM_INPUT_52)) growingStarDuration = request->getParam(PARAM_INPUT_52)->value();

    // christmasSymbols
    if (request->hasParam(PARAM_INPUT_53)) { christmasSymbolsCheckbox = request->getParam(PARAM_INPUT_53)->value();
    } else {
      christmasSymbolsCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_54)) christmasSymbolsLeadtime = request->getParam(PARAM_INPUT_54)->value();
    if (request->hasParam(PARAM_INPUT_55)) christmasSymbolsDelay = request->getParam(PARAM_INPUT_55)->value();
    if (request->hasParam(PARAM_INPUT_56)) christmasSymbolsDuration = request->getParam(PARAM_INPUT_56)->value();    
    
    // moveChristmasSymbols
    if (request->hasParam(PARAM_INPUT_57)) { moveChristmasSymbolsCheckbox = request->getParam(PARAM_INPUT_57)->value();
    } else {
      moveChristmasSymbolsCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_58)) moveChristmasSymbolsLeadtime = request->getParam(PARAM_INPUT_58)->value();
    if (request->hasParam(PARAM_INPUT_59)) moveChristmasSymbolsDelay = request->getParam(PARAM_INPUT_59)->value();
    if (request->hasParam(PARAM_INPUT_60)) moveChristmasSymbolsDuration = request->getParam(PARAM_INPUT_60)->value();
    if (request->hasParam(PARAM_INPUT_61)) moveChristmasSymbolsSymbol = request->getParam(PARAM_INPUT_61)->value();  

    // christmasCounter
    if (request->hasParam(PARAM_INPUT_63)) { christmasCounterCheckbox = request->getParam(PARAM_INPUT_63)->value();
    } else {
      christmasCounterCheckbox = "unchecked";
    }
    if (request->hasParam(PARAM_INPUT_62)) christmasCounterText = request->getParam(PARAM_INPUT_62)->value();
    if (request->hasParam(PARAM_INPUT_64)) christmasCounterLeadtime = request->getParam(PARAM_INPUT_64)->value();
    if (request->hasParam(PARAM_INPUT_65)) christmasCounterDelay = request->getParam(PARAM_INPUT_65)->value();
    if (request->hasParam(PARAM_INPUT_66)) christmasCounterFont = request->getParam(PARAM_INPUT_66)->value();
    
    Serial.println("mirrorCheckbox = " + mirrorCheckbox);
    Serial.println("reverseCheckbox = " + reverseCheckbox);

    Serial.println("animClockCheckbox = " + animClockCheckbox);
    Serial.println("colonsCheckbox = " + colonsCheckbox);

    Serial.println("scrollText1 = " + scrollText1);
    Serial.println("scrollText1Checkbox = " + scrollText1Checkbox);
    Serial.println("scrollText1Leadtime = " + scrollText1Leadtime);
    Serial.println("scrollText1Delay = " + scrollText1Delay);
    Serial.println("scrollText1Font = " + scrollText1Font);

    Serial.println("scrollText2 = " + scrollText2);
    Serial.println("scrollText2Checkbox = " + scrollText2Checkbox);
    Serial.println("scrollText2Leadtime = " + scrollText2Leadtime);
    Serial.println("scrollText2Delay = " + scrollText2Delay);
    Serial.println("scrollText2Font = " + scrollText2Font);

    Serial.println("scrollText3 = " + scrollText3);
    Serial.println("scrollText3Checkbox = " + scrollText3Checkbox);
    Serial.println("scrollText3Leadtime = " + scrollText3Leadtime);
    Serial.println("scrollText3Delay = " + scrollText3Delay);
    Serial.println("scrollText3Font = " + scrollText3Font);

    Serial.println("weatherCheckbox = " + weatherCheckbox);
    Serial.println("weatherLeadtime = " + weatherLeadtime);
    Serial.println("weatherDelay = " + weatherDelay);
    Serial.println("weatherFont = " + weatherFont);

    Serial.println("weatherLocationCheckbox = " + weatherLocationCheckbox);
    Serial.println("cityID = " + cityID);
    Serial.println("dateCheckbox = " + dateCheckbox);
    Serial.println("tempCheckbox = " + tempCheckbox);
    Serial.println("rainCheckbox = " + rainCheckbox);
    Serial.println("windCheckbox = " + windCheckbox);
    Serial.println("humidityCheckbox = " + humidityCheckbox);
    Serial.println("pressureCheckbox = " + pressureCheckbox);

    Serial.println("snowFallSingleCheckbox = " + snowFallSingleCheckbox);
    Serial.println("snowFallSingleLeadtime = " + snowFallSingleLeadtime);
    Serial.println("snowFallSingleDuration = " + snowFallSingleDuration);
    Serial.println("snowFallSingleDelay = " + snowFallSingleDelay);

    Serial.println("snowFallMultiCheckbox = " + snowFallMultiCheckbox);
    Serial.println("snowFallMultiLeadtime = " + snowFallMultiLeadtime);
    Serial.println("snowFallMultiDelay = " + snowFallMultiDelay);
    Serial.println("snowFallMultiDuration = " + snowFallMultiDuration);

    Serial.println("pixelFallCheckbox = " + pixelFallCheckbox);
    Serial.println("pixelFallLeadtime = " + pixelFallLeadtime);
    Serial.println("pixelFallDelay = " + pixelFallDelay);
    Serial.println("pixelFallDuration = " + pixelFallDuration);

    Serial.println("starrySkyCheckbox = " + starrySkyCheckbox);
    Serial.println("starrySkyLeadtime = " + starrySkyLeadtime);
    Serial.println("starrySkyDelay = " + starrySkyDelay);
    Serial.println("starrySkyDuration = " + starrySkyDuration);
    Serial.println("starrySkyStarCount = " + starrySkyStarCount);

    Serial.println("christmasSymbolsCheckbox = " + christmasSymbolsCheckbox);
    Serial.println("christmasSymbolsLeadtime = " + christmasSymbolsLeadtime);
    Serial.println("christmasSymbolsDelay = " + christmasSymbolsDelay);
    Serial.println("christmasSymbolsDuration = " + christmasSymbolsDuration);

    Serial.println("moveChristmasSymbolsCheckbox = " + moveChristmasSymbolsCheckbox);
    Serial.println("moveChristmasSymbolsLeadtime = " + moveChristmasSymbolsLeadtime);
    Serial.println("moveChristmasSymbolsDelay = " + moveChristmasSymbolsDelay);
    Serial.println("moveChristmasSymbolsDuration = " + moveChristmasSymbolsDuration);
    Serial.println("moveChristmasSymbolsSymbol = " + moveChristmasSymbolsSymbol);

    Serial.println("christmasCounterText = " + christmasCounterText);
    Serial.println("christmasCounterCheckbox = " + christmasCounterCheckbox);
    Serial.println("christmasCounterLeadtime = " + christmasCounterLeadtime);
    Serial.println("christmasCounterDelay = " + christmasCounterDelay);
    Serial.println("christmasCounterFont = " + christmasCounterFont);

    writeConfig();
    request->redirect("/");
  });
  
  server.onNotFound(notFound);

  AsyncElegantOTA.begin(&server);   // je nach Library (IDE oder GitHub)
  server.begin();
  Serial.println("HTTP webserver started.");
  
  delay(2000);
  wipeLeftShift();
  delay(500);

  String ipInfo = "IP: " + WiFi.localIP().toString() + "                       ";
  textScroll_8x16(ipInfo.c_str(), 25);

  clkTimeLeadtime = millis();
}

// =======================================================================

void loop() {
  showClock();
  
  switch (state) {   // https://www.instructables.com/id/Finite-State-Machine-on-an-Arduino/
    case 1:   // Schneefall (multi)
      if (snowFallMultiCheckbox == "checked") {
        if (millis() > (snowFallMultiLeadtime.toInt() * 1000) + clkTimeLeadtime && !scrollInProgress && colons) {   // Effekt erst, wenn Vorlaufzeit erreicht, Uhr-Digit nicht scrollt, und Doppelpunkte an sind
          wipeRandom();
          snowFallMulti();
          state++;
          clkTimeLeadtime = millis();
        }
      } else {
        clkTimeLeadtime = millis();
        state++;
      }
      break;
  
    case 2:   // Wetter
      if (weatherCheckbox == "checked") {
        if (millis() > (weatherLeadtime.toInt() * 1000) + clkTimeLeadtime && !scrollInProgress && colons) {
          wipeRandom();
          updCnt--;
          Serial.println("Wetter-Update-Count= " + String(updCnt));
          if (updCnt <= 0) {
            updCnt = 10;
            getWeatherData();
          }
          scrollString = "";
          if (dateCheckbox == "checked") {
            scrollString += dayName[w]  + ", " + String(d) + "." + String(mo) + "." + String(ye) + "    ";
          }
          scrollString += weatherString;
          scrollString += "        ";
          switch (weatherFont.toInt()){
            case 1:
              textScroll_8x16(scrollString.c_str(), weatherDelay.toInt());
              break;
            case 2:
              textScroll_16x20(scrollString.c_str(), weatherDelay.toInt());
              break;
            case 3:
              uint8_t rand = random(1,3);
              Serial.println("random: " + String(rand));
              if (rand == 1) {
                textScroll_8x16(scrollString.c_str(), weatherDelay.toInt());
              } else {
                textScroll_16x20(scrollString.c_str(), weatherDelay.toInt());
              }
              break;
          }
          state++;
          clkTimeLeadtime = millis();
        }
      } else {
        clkTimeLeadtime = millis();
        state++;
      }
      break;
  
    case 3:   // Sternenhimmel
      if (starrySkyCheckbox == "checked") {
        if (millis() > (starrySkyLeadtime.toInt() * 1000) + clkTimeLeadtime && !scrollInProgress && colons) {
          wipeRandom();
          starrySky();
          state++;
          clkTimeLeadtime = millis();
        }
      } else {
        clkTimeLeadtime = millis();
        state++;
      }
      break;
    
    case 4:   // bewegte Symbole
      if (moveChristmasSymbolsCheckbox == "checked") {
        if (millis() > (moveChristmasSymbolsLeadtime.toInt() * 1000) + clkTimeLeadtime && !scrollInProgress && colons) {
          wipeRandom();
          uint8_t symbolSelection = moveChristmasSymbolsSymbol.toInt();
          if (symbolSelection == 6) symbolSelection = random(1,6);
          moveChristmasSymbols(symbolSelection);
          state++;
          clkTimeLeadtime = millis();
        }
      } else {
        clkTimeLeadtime = millis();
        state++;
      }
      break;
  
    case 5:   // Scrolltext 1
      if (scrollText1Checkbox == "checked") {
        if (millis() > (scrollText1Leadtime.toInt() * 1000) + clkTimeLeadtime && !scrollInProgress && colons) {
          wipeRandom();
          switch (scrollText1Font.toInt()){
            case 1:
              textScroll_8x16(scrollText1.c_str(), scrollText1Delay.toInt());
              break;
            case 2:
              textScroll_16x20(scrollText1.c_str(), scrollText1Delay.toInt());
              break;
            case 3:
              uint8_t rand = random(1,3);
              Serial.println("randomTextSize= " + String(rand));
              if (rand == 1) {
                textScroll_8x16(scrollText1.c_str(), scrollText1Delay.toInt());
              } else {
                textScroll_16x20(scrollText1.c_str(), scrollText1Delay.toInt());
              }
              break;
          }
          state++;
          clkTimeLeadtime = millis();
        }
      } else {
        clkTimeLeadtime = millis();
        state++;
      }
      break;

    case 6:   // Symbole
      if (christmasSymbolsCheckbox == "checked") {
        if (millis() > (christmasSymbolsLeadtime.toInt() * 1000) + clkTimeLeadtime && !scrollInProgress && colons) {
          wipeRandom();
          drawChristmasSymbols();
          state++;
          clkTimeLeadtime = millis();
        }
      } else {
        clkTimeLeadtime = millis();
        state++;
      }
      break;

    case 7:   // Scrolltext 2
      if (scrollText2Checkbox == "checked") {
        if (millis() > (scrollText2Leadtime.toInt() * 1000) + clkTimeLeadtime && !scrollInProgress && colons) {
          wipeRandom();
          switch (scrollText2Font.toInt()){
            case 1:
              textScroll_8x16(scrollText2.c_str(), scrollText2Delay.toInt());
              break;
            case 2:
              textScroll_16x20(scrollText2.c_str(), scrollText2Delay.toInt());
              break;
            case 3:
              uint8_t rand = random(1,3);
              Serial.println("randomTextSize= " + String(rand));
              if (rand == 1) {
                textScroll_8x16(scrollText2.c_str(), scrollText2Delay.toInt());
              } else {
                textScroll_16x20(scrollText2.c_str(), scrollText2Delay.toInt());
              }
              break;
          }
          state++;
          clkTimeLeadtime = millis();
        }
      } else {
        clkTimeLeadtime = millis();
        state++;
      }
      break;
 
    case 8:   // Schneefall (Pixel)
      if (pixelFallCheckbox == "checked") {
        if (millis() > (pixelFallLeadtime.toInt() * 1000) + clkTimeLeadtime && !scrollInProgress && colons) {
          wipeRandom();
          pixelFall();
          state++;
          clkTimeLeadtime = millis();
        }
      } else {
        clkTimeLeadtime = millis();
        state++;
      }
      break;
  
    case 9:   // wachsende Schneeflocke
      if (growingStarCheckbox == "checked") {
        if (millis() > (growingStarLeadtime.toInt() * 1000) + clkTimeLeadtime && !scrollInProgress && colons) {
          wipeRandom();
          growingStar();
          state++;
          clkTimeLeadtime = millis();
        } 
      } else {
        clkTimeLeadtime = millis();
        state++;
      }
      break;

    case 10:   // Scrolltext 3
      if (scrollText3Checkbox == "checked") {
        if (millis() > (scrollText3Leadtime.toInt() * 1000) + clkTimeLeadtime && !scrollInProgress && colons) {
          wipeRandom();
          switch (scrollText3Font.toInt()){
            case 1:
              textScroll_8x16(scrollText3.c_str(), scrollText3Delay.toInt());
              break;
            case 2:
              textScroll_16x20(scrollText3.c_str(), scrollText3Delay.toInt());
              break;
            case 3:
              uint8_t rand = random(1,3);
              Serial.println("randomTextSize= " + String(rand));
              if (rand == 1) {
                textScroll_8x16(scrollText3.c_str(), scrollText3Delay.toInt());
              } else {
                textScroll_16x20(scrollText3.c_str(), scrollText3Delay.toInt());
              }
              break;
          }
          state++;
          clkTimeLeadtime = millis();
        }
      } else {
        clkTimeLeadtime = millis();
        state++;
      }
      break;
  
    case 11:   // Schneefall (single)
      if (snowFallSingleCheckbox == "checked") {
        if (millis() > (snowFallSingleLeadtime.toInt() * 1000) + clkTimeLeadtime && !scrollInProgress && colons) {
          wipeRandom();
          snowFallSingle();
          state++;
          clkTimeLeadtime = millis();
        }
      } else {
        clkTimeLeadtime = millis();
        state++;
      }
      break;

     case 12:   // Text Weihnachtscounter
      getTimeLocal();
      if (christmasCounterCheckbox == "checked" && mo == 12 && d < 24) {
        if (millis() > (christmasCounterLeadtime.toInt() * 1000) + clkTimeLeadtime && !scrollInProgress && colons) {
          uint8_t daysToChristmas = 24 - d;
          String christmasCounterScrollText = christmasCounterText + "Noch " + String(daysToChristmas) + " Tage bis Weihnachten...        ";
          if (daysToChristmas == 1) christmasCounterScrollText = christmasCounterText + "Morgen ist Weihnachten!        ";
          wipeRandom();
          switch (christmasCounterFont.toInt()){
           case 1:
             textScroll_8x16(christmasCounterScrollText.c_str(), christmasCounterDelay.toInt());
             break;
           case 2:
             textScroll_16x20(christmasCounterScrollText.c_str(), christmasCounterDelay.toInt());
             break;
           case 3:
             uint8_t rand = random(1,3);
             Serial.println("randomTextSize= " + String(rand));
             if (rand == 1) {
               textScroll_8x16(christmasCounterScrollText.c_str(), christmasCounterDelay.toInt());
             } else {
               textScroll_16x20(christmasCounterScrollText.c_str(), christmasCounterDelay.toInt());
             }
             break;
         }
          state = 1;
          clkTimeLeadtime = millis();
        } 
      } else {
        clkTimeLeadtime = millis();
        state = 1;
      }
      break;
 
  } 
}

// =======================================================================
