#include <Arduino.h>

// DEV setzen, wenn fuer Entwicklungsmatrix (64x16) kompiliert werden soll
#define DEV

#include "defaultsettings.h"
#include "fonts.h"
#include "christmas-symbols_16x16_bin.h"
#include "star-symbols.h"
#include "inputparameters.h"
#include "website.h"

#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <Timezone.h>
#include <TimeLib.h>   // https://github.com/PaulStoffregen/Time
#include <WiFiUdp.h>

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

// create different instances
WiFiClient client;
AsyncWebServer server(80);
DNSServer dns;
WiFiUDP Udp;

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

int h, m, s, w, mo, ye, d;
long localMillisAtUpdate = 0;
static const char ntpServerName[] = "de.pool.ntp.org";    // Finde lokale Server unter http://www.pool.ntp.org/zone/ch
const int timeZone = 0;                                   // 0 wenn mit <Timezone.h> gearbeitet wird sonst stimmt es nachher nicht
unsigned int localPort = 8888;                            // lokaler Port zum Abhören von UDP-Paketen
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

// Einstellungen entsprechend Zeitzone und Sommerzeit.
// TimeZone Einstellungen Details https://github.com/JChristensen/Timezone
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Time (Frankfurt, Paris)
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Time (Frankfurt, Paris)
Timezone CE(CEST, CET);

String scrollString;
String dayName[] = {"Err", "Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};

unsigned long clkTimeEffect = 0;
unsigned long clkTimePre = 0;
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
    if ( row == NUM_ROWS ) row = 0;            // Anzahl Zeilen

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
  if (var == "NETWORK") {
    return WiFi.SSID();
  } else if (var == "RSSI") {
    return String(getWifiQuality());
  } else if (var == "SCROLLTEXT1OPTIONS") {
    return scrollText1;
  } else if (var == "SCROLLTEXT2OPTIONS") {
    return scrollText2;
  } else if (var == "SCROLLTEXT3OPTIONS") {
    return scrollText3;
  } else if (var == "SCROLLTEXT1_CHECKBOX_OPTIONS") {
    return scrolltext1Checkbox;
  } else if (var == "SCROLLTEXT2_CHECKBOX_OPTIONS") {
    return scrolltext2Checkbox;
  } else if (var == "SCROLLTEXT3_CHECKBOX_OPTIONS") {
    return scrolltext3Checkbox;
  } else if (var == "SCROLLSPEEDOPTIONS") {
//    String sSpeed = scrollSpeed;
//    String scrollSpeedOptions = "<option value='45'>normal</option><option value='60'>langsam</option><option value='30'>schnell</option>";
//    scrollSpeedOptions.replace(sSpeed + "'", sSpeed + "' selected" );
    return scrollSpeed;
  } else if (var == "TEXT1DELAYOPTIONS") {
    return text1Delay;
  } else if (var == "TEXT2DELAYOPTIONS") {
    return text2Delay;
  } else if (var == "TEXT3DELAYOPTIONS") {
    return text3Delay;
  } else if (var == "TEXT1FONTOPTIONS") {
      String fFont = text1Font;
      String textFontOptions = "<option value='1'>8x16</option><option value='2'>16x20</option><option value='3'>random</option>";
      textFontOptions.replace(fFont + "'", fFont + "' selected" );
    return textFontOptions;
  } else if (var == "TEXT2FONTOPTIONS") {
      String fFont = text2Font;
      String textFontOptions = "<option value='1'>8x16</option><option value='2'>16x20</option><option value='3'>random</option>";
      textFontOptions.replace(fFont + "'", fFont + "' selected" );
    return textFontOptions;
  } else if (var == "TEXT3FONTOPTIONS") {
      String fFont = text3Font;
      String textFontOptions = "<option value='1'>8x16</option><option value='2'>16x20</option><option value='3'>random</option>";
      textFontOptions.replace(fFont + "'", fFont + "' selected" );
    return textFontOptions;
  } else if (var == "PRETIMEWEATHEROPTIONS") {
    return preTimeWeather;
  } else if (var == "PRETIMETEXT1OPTIONS") {
    return preTimeText1;
  } else if (var == "PRETIMETEXT2OPTIONS") {
    return preTimeText2;
  } else if (var == "PRETIMETEXT3OPTIONS") {
    return preTimeText3;
  } else if (var == "PRETIMESNOWOPTIONS") {
    return preTimeSnow;
  } else if (var == "PRETIMESNOWFALL2OPTIONS") {
    return preTimeSnowFall2;
  } else if (var == "PRETIMESTAROPTIONS") {
    return preTimeStar;
  } else if (var == "PRETIMEGROWINGSTAR16X15OPTIONS") {
    return preTimeGrowingStar16x15;
  } else if (var == "PRETIMECHRISTMASSYMBOLSOPTIONS") {
    return preTimeChristmasSymbols;
  } else if (var == "SNOWDURATIONOPTIONS") {
    return snowDuration;
  } else if (var == "SNOWDELAYOPTIONS") {
    return snowDelay;
  } else if (var == "SNOWFALL2DURATIONOPTIONS") {
    return snowFall2Duration;
  } else if (var == "SNOWFALL2DELAYOPTIONS") {
    return snowFall2Delay;
  } else if (var == "STARDURATIONOPTIONS") {
    return starDuration;
  } else if (var == "STARDELAYOPTIONS") {
    return starDelay;
  } else if (var == "STARCOUNTOPTIONS") {
    return starCount;
  } else if (var == "GROWINGSTAR16X15DURATIONOPTIONS") {
    return growingStar16x15Duration;
  } else if (var == "GROWINGSTAR16X15DELAYOPTIONS") {
    return growingStar16x15Delay;
  } else if (var == "CHRISTMASSYMBOLSDURATIONOPTIONS") {
    return christmasSymbolsDuration;
  } else if (var == "CHRISTMASSYMBOLSDELAYOPTIONS") {
    return christmasSymbolsDelay;
  } else if (var == "CITYIDOPTIONS") {
    return cityID;
  } else if (var == "WEATHERLOCATION") {
    return weatherLocation;
  } else if (var == "ENABLE_DATE_INPUT") {
    return dateCheckbox;
  } else if (var == "ENABLE_WEATHER_INPUT") {
    return weatherCheckbox;
  } else if (var == "ENABLE_TEMP_INPUT") {
    return tempCheckbox;
  } else if (var == "ENABLE_RAIN_INPUT") {
    return rainCheckbox;
  } else if (var == "ENABLE_WIND_INPUT") {
    return windCheckbox;
  } else if (var == "ENABLE_HUMIDITY_INPUT") {
    return humidityCheckbox;
  } else if (var == "ENABLE_PRESSURE_INPUT") {
    return pressureCheckbox;
  } else if (var == "ENABLE_LOCATION_INPUT") {
    return locationCheckbox;
  } else if (var == "ENABLE_DOTS_INPUT") {
    return dotsCheckbox;
  } else if (var == "ENABLE_SNOW_INPUT") {
    return snowCheckbox;
  } else if (var == "ENABLE_SNOWFALL2_INPUT") {
    return snowFall2Checkbox;
  } else if (var == "ENABLE_STAR_INPUT") {
    return starCheckbox;
  } else if (var == "ENABLE_GROWINGSTAR16X15_INPUT") {
    return growingStar16x15Checkbox;
  } else if (var == "ENABLE_CHRISTMASSYMBOLS_INPUT") {
    return christmasSymbolsCheckbox;
  } else if (var == "ENABLE_MIRROR_INPUT") {
    return mirrorCheckbox;
  } else if (var == "ENABLE_REVERSE_INPUT") {
    return reverseCheckbox;
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
    f.println("scrollText1=" + scrollText1);
    f.println("scrollText2=" + scrollText2);
    f.println("scrollText3=" + scrollText3);
    f.println("scrolltext1Checkbox=" + scrolltext1Checkbox);
    f.println("scrolltext2Checkbox=" + scrolltext2Checkbox);
    f.println("scrolltext3Checkbox=" + scrolltext3Checkbox);
    f.println("mirrorCheckbox=" + mirrorCheckbox);
    f.println("reverseCheckbox=" + reverseCheckbox);
    f.println("snowCheckbox=" + snowCheckbox);
    f.println("snowFall2Checkbox=" + snowFall2Checkbox);
    f.println("starCheckbox=" + starCheckbox);
    f.println("growingStar16x15Checkbox=" + growingStar16x15Checkbox);
    f.println("christmasSymbolsCheckbox=" + christmasSymbolsCheckbox);
    f.println("dateCheckbox=" + dateCheckbox);
    f.println("weatherCheckbox=" + weatherCheckbox);
    f.println("tempCheckbox=" + tempCheckbox);
    f.println("rainCheckbox=" + rainCheckbox);
    f.println("windCheckbox=" + windCheckbox);
    f.println("humidityCheckbox=" + humidityCheckbox);
    f.println("pressureCheckbox=" + pressureCheckbox);
    f.println("locationCheckbox=" + locationCheckbox);
    f.println("dotsCheckbox=" + dotsCheckbox);
    f.println("scrollSpeed=" + String(scrollSpeed));
    f.println("text1Delay=" + String(text1Delay));
    f.println("text2Delay=" + String(text2Delay));
    f.println("text3Delay=" + String(text3Delay));
    f.println("text1Font=" + String(text1Font));
    f.println("text2Font=" + String(text2Font));
    f.println("text3Font=" + String(text3Font));
    f.println("preTimeWeather=" + String(preTimeWeather));
    f.println("preTimeText1=" + String(preTimeText1));
    f.println("preTimeText2=" + String(preTimeText2));
    f.println("preTimeText3=" + String(preTimeText3));
    f.println("preTimeSnow=" + String(preTimeSnow));
    f.println("preTimeSnowFall2=" + String(preTimeSnowFall2));
    f.println("preTimeStar=" + String(preTimeStar));
    f.println("preTimeGrowingStar16x15=" + String(preTimeGrowingStar16x15));
    f.println("preTimeChristmasSymbols=" + String(preTimeChristmasSymbols));
    f.println("snowDuration=" + String(snowDuration));
    f.println("snowDelay=" + String(snowDelay));
    f.println("snowFall2Duration=" + String(snowFall2Duration));
    f.println("snowFall2Delay=" + String(snowFall2Delay));
    f.println("starDuration=" + String(starDuration));
    f.println("starDelay=" + String(starDelay));
    f.println("starCount=" + String(starCount));
    f.println("growingStar16x15Duration=" + String(growingStar16x15Duration));
    f.println("growingStar16x15=" + String(growingStar16x15Delay));
    f.println("christmasSymbolsDuration=" + String(christmasSymbolsDuration));
    f.println("christmasSymbolsDelay=" + String(christmasSymbolsDelay));
    f.println("cityID=" + String(cityID));
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
    if (configline.indexOf("scrollText1=") >= 0) {
      scrollText1 = configline.substring(configline.lastIndexOf("scrollText1=") + 12);
      scrollText1 = scrollText1.substring(0,scrollText1.length()-1); // letztes Zeichen (cr) abschneiden
      Serial.println("scrollText1= " + scrollText1);
    }
    if (configline.indexOf("scrollText2=") >= 0) {
      scrollText2 = configline.substring(configline.lastIndexOf("scrollText2=") + 12);
      scrollText2 = scrollText2.substring(0,scrollText2.length()-1);
      Serial.println("scrollText2= " + scrollText2);
    }
    if (configline.indexOf("scrollText3=") >= 0) {
      scrollText3 = configline.substring(configline.lastIndexOf("scrollText3=") + 12);
      scrollText3 = scrollText3.substring(0,scrollText3.length()-1);
      Serial.println("scrollText3= " + scrollText3);
    }
    if (configline.indexOf("scrolltext1Checkbox=") >= 0) {
      scrolltext1Checkbox = configline.substring(configline.lastIndexOf("scrolltext1Checkbox=") + 20);
      scrolltext1Checkbox.trim();
      Serial.println("scrolltext1Checkbox= " + scrolltext1Checkbox);
    }
    if (configline.indexOf("scrolltext2Checkbox=") >= 0) {
      scrolltext2Checkbox = configline.substring(configline.lastIndexOf("scrolltext2Checkbox=") + 20);
      scrolltext2Checkbox.trim();
      Serial.println("scrolltext2Checkbox= " + scrolltext2Checkbox);
    }
    if (configline.indexOf("scrolltext3Checkbox=") >= 0) {
      scrolltext3Checkbox = configline.substring(configline.lastIndexOf("scrolltext3Checkbox=") + 20);
      scrolltext3Checkbox.trim();
      Serial.println("scrolltext3Checkbox= " + scrolltext3Checkbox);
    }
    if (configline.indexOf("dateCheckbox=") >= 0) {
      dateCheckbox = configline.substring(configline.lastIndexOf("dateCheckbox=") + 13);
      dateCheckbox.trim();
      Serial.println("dateCheckbox= " + dateCheckbox);
    }
    if (configline.indexOf("weatherCheckbox=") >= 0) {
      weatherCheckbox = configline.substring(configline.lastIndexOf("weatherCheckbox=") + 16);
      weatherCheckbox.trim();
      Serial.println("weatherCheckbox= " + weatherCheckbox);
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
    if (configline.indexOf("locationCheckbox=") >= 0) {
      locationCheckbox = configline.substring(configline.lastIndexOf("locationCheckbox=") + 17);
      locationCheckbox.trim();
      Serial.println("locationCheckbox= " + locationCheckbox);
    }
    if (configline.indexOf("dotsCheckbox=") >= 0) {
      dotsCheckbox = configline.substring(configline.lastIndexOf("dotsCheckbox=") + 13);
      dotsCheckbox.trim();
      Serial.println("dotsCheckbox= " + dotsCheckbox);
    }
    if (configline.indexOf("snowCheckbox=") >= 0) {
      snowCheckbox = configline.substring(configline.lastIndexOf("snowCheckbox=") + 13);
      snowCheckbox.trim();
      Serial.println("snowCheckbox= " + snowCheckbox);
    }
    if (configline.indexOf("snowFall2Checkbox=") >= 0) {
      snowFall2Checkbox = configline.substring(configline.lastIndexOf("snowFall2Checkbox=") + 18);
      snowFall2Checkbox.trim();
      Serial.println("snowFall2Checkbox= " + snowFall2Checkbox);
    }
    if (configline.indexOf("starCheckbox=") >= 0) {
      starCheckbox = configline.substring(configline.lastIndexOf("starCheckbox=") + 13);
      starCheckbox.trim();
      Serial.println("starCheckbox= " + starCheckbox);
    }
    if (configline.indexOf("growingStar16x15Checkbox=") >= 0) {
      growingStar16x15Checkbox = configline.substring(configline.lastIndexOf("growingStar16x15Checkbox=") + 25);
      growingStar16x15Checkbox.trim();
      Serial.println("growingStar16x15Checkbox= " + growingStar16x15Checkbox);
    }
    if (configline.indexOf("christmasSymbolsCheckbox=") >= 0) {
      christmasSymbolsCheckbox = configline.substring(configline.lastIndexOf("christmasSymbolsCheckbox=") + 25);
      christmasSymbolsCheckbox.trim();
      Serial.println("christmasSymbolsCheckbox= " + christmasSymbolsCheckbox);
    }
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
    if (configline.indexOf("scrollSpeed=") >= 0) {
      scrollSpeed = configline.substring(configline.lastIndexOf("scrollSpeed=") + 12).toInt();
      Serial.println("scrollSpeed= " + String(scrollSpeed));
    }
    if (configline.indexOf("text1Delay=") >= 0) {
      text1Delay = configline.substring(configline.lastIndexOf("text1Delay=") + 11).toInt();
      Serial.println("text1Delay= " + String(text1Delay));
    }
    if (configline.indexOf("text2Delay=") >= 0) {
      text2Delay = configline.substring(configline.lastIndexOf("text2Delay=") + 11).toInt();
      Serial.println("text2Delay= " + String(text2Delay));
    }
    if (configline.indexOf("text3Delay=") >= 0) {
      text3Delay = configline.substring(configline.lastIndexOf("text3Delay=") + 11).toInt();
      Serial.println("text3Delay= " + String(text3Delay));
    }
    if (configline.indexOf("text1Font=") >= 0) {
      text1Font = configline.substring(configline.lastIndexOf("text1Font=") + 10).toInt();
      Serial.println("text1Font= " + String(text1Font));
    }
    if (configline.indexOf("text2Font=") >= 0) {
      text2Font = configline.substring(configline.lastIndexOf("text2Font=") + 10).toInt();
      Serial.println("text2Font= " + String(text2Font));
    }
    if (configline.indexOf("text3Font=") >= 0) {
      text3Font = configline.substring(configline.lastIndexOf("text3Font=") + 10).toInt();
      Serial.println("text3Font= " + String(text3Font));
    }
    if (configline.indexOf("preTimeWeather=") >= 0) {
      preTimeWeather = configline.substring(configline.lastIndexOf("preTimeWeather=") + 15).toInt();
      Serial.println("preTimeWeather= " + String(preTimeWeather));
    }
    if (configline.indexOf("preTimeText1=") >= 0) {
      preTimeText1 = configline.substring(configline.lastIndexOf("preTimeText1=") + 13).toInt();
      Serial.println("preTimeText1= " + String(preTimeText1));
    }
    if (configline.indexOf("preTimeText2=") >= 0) {
      preTimeText2 = configline.substring(configline.lastIndexOf("preTimeText2=") + 13).toInt();
      Serial.println("preTimeText2= " + String(preTimeText2));
    }
    if (configline.indexOf("preTimeText3=") >= 0) {
      preTimeText3 = configline.substring(configline.lastIndexOf("preTimeText3=") + 13).toInt();
      Serial.println("preTimeText3= " + String(preTimeText3));
    }
    if (configline.indexOf("preTimeSnow=") >= 0) {
      preTimeSnow = configline.substring(configline.lastIndexOf("preTimeSnow=") + 12).toInt();
      Serial.println("preTimeSnow= " + String(preTimeSnow));
    }
    if (configline.indexOf("preTimeSnowFall2=") >= 0) {
      preTimeSnowFall2 = configline.substring(configline.lastIndexOf("preTimeSnowFall2=") + 17).toInt();
      Serial.println("preTimeSnowFall2= " + String(preTimeSnowFall2));
    }
    if (configline.indexOf("preTimeStar=") >= 0) {
      preTimeStar = configline.substring(configline.lastIndexOf("preTimeStar=") + 12).toInt();
      Serial.println("preTimeStar= " + String(preTimeStar));
    }
    if (configline.indexOf("preTimeGrowingStar16x15=") >= 0) {
      preTimeGrowingStar16x15 = configline.substring(configline.lastIndexOf("preTimeGrowingStar16x15=") + 24).toInt();
      Serial.println("preTimeGrowingStar16x15= " + String(preTimeGrowingStar16x15));
    }
    if (configline.indexOf("preTimeChristmasSymbols=") >= 0) {
      preTimeChristmasSymbols = configline.substring(configline.lastIndexOf("preTimeChristmasSymbols=") + 24).toInt();
      Serial.println("preTimeChristmasSymbols= " + String(preTimeChristmasSymbols));
    }
    if (configline.indexOf("snowDuration=") >= 0) {
      snowDuration = configline.substring(configline.lastIndexOf("snowDuration=") + 13).toInt();
      Serial.println("snowDuration= " + String(snowDuration));
    }
    if (configline.indexOf("snowDelay=") >= 0) {
      snowDelay = configline.substring(configline.lastIndexOf("snowDelay=") + 10).toInt();
      Serial.println("snowDelay= " + String(snowDelay));
    }
    if (configline.indexOf("snowFall2Duration=") >= 0) {
      snowFall2Duration = configline.substring(configline.lastIndexOf("snowFall2Duration=") + 18).toInt();
      Serial.println("snowFall2Duration= " + String(snowFall2Duration));
    }
    if (configline.indexOf("snowFall2Delay=") >= 0) {
      snowFall2Delay = configline.substring(configline.lastIndexOf("snowFall2Delay=") + 15).toInt();
      Serial.println("snowFall2Delay= " + String(snowFall2Delay));
    }
    if (configline.indexOf("starDuration=") >= 0) {
      starDuration = configline.substring(configline.lastIndexOf("starDuration=") + 13).toInt();
      Serial.println("starDuration= " + String(starDuration));
    }
    if (configline.indexOf("starDelay=") >= 0) {
      starDelay = configline.substring(configline.lastIndexOf("starDelay=") + 10).toInt();
      Serial.println("starDelay= " + String(starDelay));
    }
    if (configline.indexOf("starCount=") >= 0) {
      starCount = configline.substring(configline.lastIndexOf("starCount=") + 10).toInt();
      Serial.println("starCount= " + String(starCount));
    }
    if (configline.indexOf("growingStar16x15Duration=") >= 0) {
      growingStar16x15Duration = configline.substring(configline.lastIndexOf("growingStar16x15Duration=") + 25).toInt();
      Serial.println("growingStar16x15Duration= " + String(growingStar16x15Duration));
    }
    if (configline.indexOf("growingStar16x15Delay=") >= 0) {
      growingStar16x15Delay = configline.substring(configline.lastIndexOf("growingStar16x15Delay=") + 12).toInt();
      Serial.println("growingStar16x15Delay= " + String(growingStar16x15Delay));
    }
    if (configline.indexOf("christmasSymbolsDuration=") >= 0) {
      christmasSymbolsDuration = configline.substring(configline.lastIndexOf("christmasSymbolsDuration=") + 25).toInt();
      Serial.println("christmasSymbolsDuration= " + String(christmasSymbolsDuration));
    }
    if (configline.indexOf("christmasSymbolsDelay=") >= 0) {
      christmasSymbolsDelay = configline.substring(configline.lastIndexOf("christmasSymbolsDelay=") + 22).toInt();
      Serial.println("christmasSymbolsDelay= " + String(christmasSymbolsDelay));
    }
    if (configline.indexOf("cityID=") >= 0) {
      cityID = configline.substring(configline.lastIndexOf("cityID=") + 7);
      cityID.trim();
      Serial.println("cityID= " + String(cityID));
    }
  }  
  fr.close();
  Serial.println();
}

// =======================================================================

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48; // NTP-Zeit in den ersten 48 Bytes der Nachricht
byte packetBuffer[NTP_PACKET_SIZE]; //Puffer für eingehende und ausgehende Pakete

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip Adresse

  while (Udp.parsePacket() > 0) ; // alle zuvor empfangenen Pakete verwerfen
  Serial.println("Transmit NTP Request");
  // einen zufälligen Server aus dem Pool holen
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // Paket in den Puffer einlesen
      unsigned long secsSince1900;
      // vier Bytes ab Position 40 in eine lange Ganzzahl umwandeln
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("keine NTP Antwort");
  return 0; // gibt 0 zurück, wenn die Zeit nicht ermittelt werden kann.
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // alle Bytes im Puffer auf 0 setzen
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialisieren von Werten, die für die Bildung von NTP-Requests benötigt werden.
  // (siehe URL oben für Details zu den Paketen)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // alle NTP-Felder wurden jetzt mit Werten versehen
  // Sie können ein Paket senden, das einen Zeitstempel anfordert.:
  Udp.beginPacket(address, 123); //NTP-Requests sollen auf Port 123 erfolgen
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void getTimeLocal()
{
  time_t tT = now();
  time_t tTlocal = CE.toLocal(tT);
  w = weekday(tTlocal);
  d = day(tTlocal);
  mo = month(tTlocal);
  ye = year(tTlocal);
  h = hour(tTlocal);
  m = minute(tTlocal);
  s = second(tTlocal);
  localMillisAtUpdate = millis();
}

byte dig[6] = {0,0,0,0,0,0};
int dots = 0;
long dotTime = 0;

void showClock() {
  getTimeLocal();

  dig[0] = h / 10;
  dig[1] = h % 10;
  dig[2] = m / 10;
  dig[3] = m % 10;
  dig[4] = s / 10;
  dig[5] = s % 10;

  #ifndef DEV
  for (uint8_t x = 0; x <64; x++){
    drawPoint(x, 0, 1);
    drawPoint(x, 19, 1);
  }
  #endif
  for (uint8_t y = 0; y < NUM_ROWS; y++){
    drawPoint(0, y, 1);
    drawPoint(63, y, 1);
  } 
  
  if (h > 9) {
    drawImage(  3, Y_OFFSET, 8, 16, numbers + 16 * dig[0]);
  }
  drawImage( 12, Y_OFFSET, 8, 16, numbers + 16 * dig[1]);
  drawImage( 24, Y_OFFSET, 8, 16, numbers + 16 * dig[2]);
  drawImage( 33, Y_OFFSET, 8, 16, numbers + 16 * dig[3]);
  drawImage( 45, Y_OFFSET, 8, 16, numbers + 16 * dig[4]);
  drawImage( 54, Y_OFFSET, 8, 16, numbers + 16 * dig[5]);
  
  if (dotsCheckbox == "checked") {
    //toggle colons
    if (millis() - dotTime > 500) {
      // Serial.println("Toggeling colons...");
      dotTime = millis();
      dots = !dots;
    }
  } else {
    dots = 1;
  }
  if (dots) {
    drawPoint(21,5,1);
    drawPoint(21,10,1);
    drawPoint(22,5,1);
    drawPoint(22,10,1);
    drawPoint(42,5,1);
    drawPoint(42,10,1);
    drawPoint(43,5,1);
    drawPoint(43,10,1);
    drawPoint(21,6,1);
    drawPoint(21,11,1);
    drawPoint(22,6,1);
    drawPoint(22,11,1);
    drawPoint(42,6,1);
    drawPoint(42,11,1);
    drawPoint(43,6,1);
    drawPoint(43,11,1);
  } else {
    drawPoint(21,5,0);
    drawPoint(21,10,0);
    drawPoint(22,5,0);
    drawPoint(22,10,0);
    drawPoint(42,5,0);
    drawPoint(42,10,0);  
    drawPoint(43,5,0);
    drawPoint(43,10,0);  
    drawPoint(21,6,0);
    drawPoint(21,11,0);
    drawPoint(22,6,0);
    drawPoint(22,11,0);
    drawPoint(42,6,0);
    drawPoint(42,11,0);  
    drawPoint(43,6,0);
    drawPoint(43,11,0);  
  }
  
  //String TimeString = "Time: " + String(dig[0]) + String(dig[1]) + ":" + String(dig[2]) + String(dig[3]) + ":" + String(dig[4]) + String(dig[5]);
  //Serial.println(TimeString);
  //delay(200);
}
/*-------- End NTP code ----------*/

// =======================================================================

/* --- online weather function --- */
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
    if (locationCheckbox == "checked") {
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
/* --- end online weather function --- */

// =======================================================================
/* --- effect functions --- */

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

void h_TextScroll_16x20_v1(const char *s, uint8_t q, uint8_t sdelay) {        // s = Text (Array); q = Textlänge
  clearLastScroll();                                                          // Reste vom vorherigen Scrollen loeschen
  Serial.println("Start horTextScroll_16x20...");
  for (uint8_t k = 0; k < q-1; k++) {                                         // Message Zeichen für Zeichen durchlaufen
    msglineindex = s[k];
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
/*
    Serial.print("stringLength ");
    Serial.print(q);
    Serial.print(" ... char ");
    Serial.print(k);
    Serial.print(" --> ");
    Serial.write(msglineindex);                               // https://stackoverflow.com/questions/46301534/in-arduino-how-to-print-character-for-givien-ascii-number
    Serial.print(" --> ASCII: ");
    Serial.println(msglineindex);
*/
    uint8_t bytecount = 0;
    charWidth = ArialRound[(msglineindex - 32) * 41 + 40];                 // Zeichenbreite (41. Byte jedes Zeichens im Font)
    for (uint8_t row = 0; row < NUM_ROWS; row++) {                          // nächstes Zeichen in Puffer laden (2 Byte breit)
      buf1[row] = ArialRound[(msglineindex - 32) * 41 + row + bytecount];  // erstes Byte des 2 Byte breiten Zeichens
      bytecount++;
      buf2[row] = ArialRound[(msglineindex - 32) * 41 + row + bytecount];  // zweites Byte des 2 Byte breiten Zeichens
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
  }
  clearMatrix();
  delay(1000);
}

// =======================================================================

void h_TextScroll_16x20_v2(const char *s, uint8_t sdelay) {                        // s = Text (Array); sdelay = Speed (Verzögerung)
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
    charWidth = ArialRound[(msglineindex - 32) * 41 + 40];                 // Zeichenbreite (41. Byte jedes Zeichens im Font)
    for (uint8_t row = 0; row < NUM_ROWS; row++) {                          // nächstes Zeichen in Puffer laden (2 Byte breit)
      buf1[row] = ArialRound[(msglineindex - 32) * 41 + row + bytecount];  // erstes Byte des 2 Byte breiten Zeichens
      bytecount++;
      buf2[row] = ArialRound[(msglineindex - 32) * 41 + row + bytecount];  // zweites Byte des 2 Byte breiten Zeichens
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

void h_TextScroll_8x16(const char *s, uint8_t sdelay) {                       // s = Text (Array); sdelay = Speed (Verzögerung)
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
  Serial.println("Start drawing christmasSymbols");
  clkTimeEffect = millis();
  while (millis() < (christmasSymbolsDuration.toInt() * 1000) + clkTimeEffect) {
    uint8_t x = random(0,48);
    uint8_t y = random(0,4);
    uint8_t s = random(0,4);
    drawImage( x, y, 16, 16, christmasSymbols16x16 + s * 32);
    delay(1000);
    clearMatrix();
  }
}  
  
// =======================================================================  

void growingStar_16x15() {
  Serial.println("Start growingStar_16x15");
  clkTimeEffect = millis();
  while (millis() < (growingStar16x15Duration.toInt() * 1000) + clkTimeEffect) {
    uint8_t x = random(0,48);
    uint8_t y = random(0,6);
    for (uint8_t i = 0; i < 9; i++) {
      drawImage( x, y, 16, 15, growingStar16x15 + i * 30);
      delay(growingStar16x15Delay.toInt());
    }
    delay(300);
    /*
    for (uint8_t i = 0; i < 9; i++) {
      drawImage( x, y, 16, 15, growingStar16x15 + (8 - i) * 30);
      delay(growingStar16x15Delay.toInt());
    }
    */
    clearMatrix();
  }
}  
  
// =======================================================================  

void snowFall() {   // Schneeflocken vertikal scrollen
  clearMatrix();
  clkTimeEffect = millis();
  
  uint8_t starimage[8];
  uint8_t startdelay[8];
  uint8_t speeddelay[8];
  uint8_t startdelaycount[8];
  uint8_t speeddelaycount[8];
  uint8_t starDone[8];
  uint8_t allStarsDone = 0;

  for (uint8_t zone = 0; zone < 8; zone++) {
    starimage[zone] = 0;
    startdelay[zone] = random(0,51);
    speeddelay[zone] = random(0,5);
    startdelaycount[zone] = 0;
    speeddelaycount[zone] = 0;
    starDone[zone] = 0;
  }

  while ( allStarsDone < 8 ) {
    for (uint8_t zone = 0; zone < 8; zone++) {
      if ( (millis() > (snowDuration.toInt() * 1000) + clkTimeEffect) and starimage[zone] == 0 ) {   // wenn Zeit abgelaufen und Flocke unten raus
        starDone[zone] = 1;
      }
      if ( startdelaycount[zone] >= startdelay[zone] and starDone[zone] == 0 ) {   // Start der Flocke um eine zufaellige Zeit verzögern
        drawImage( zone * 8, 2, 8, 20, stars20 + starimage[zone] * 20);      // passendes Image aus stars-Font-Array zeichnen
        if (speeddelaycount[zone] >= speeddelay[zone]) {
          speeddelaycount[zone] = 0;
          starimage[zone]++;                                                 // Flocke eins runter
        }
        speeddelaycount[zone]++;
        if ( starimage[zone] == 28 ) {                                       // letzter Zustand erreicht (Flocke unten rausgescrollt)
          starimage[zone] = 0;
          startdelaycount[zone] = 0;
          startdelay[zone] = random(0,51);
          speeddelay[zone] = random(0,6);
        }
      }
      startdelaycount[zone]++;
      allStarsDone = 0;
      for (uint8_t i = 0; i < 8; i++) {   // checken, ob alle Flocken fertig gescrollt sind
        allStarsDone += starDone[i];
      }
    }
    delay(snowDelay.toInt());
  }
  
  delay(1000);
  clearMatrix();
}

// =======================================================================

void snowFall2() {   // 1 Schneeflocke an verschiedenen Positionen vertikal scrollen
  clearMatrix();
  clkTimeEffect = millis();
  while (millis() < (snowDuration.toInt() * 1000) + clkTimeEffect) {
    uint8_t x = random(0,8);
    for (uint16_t i = 0; i < NUM_ROWS + 8; i++) {   // bei DEV (16 Zeilen) bis 24 zaehlen, sonst bis 28
      //Serial.println("snow_char= " + String(i));
      #ifdef DEV
        const uint8_t *pSrc = stars16 + i * 16;
      #else
        const uint8_t *pSrc = stars20 + i * 20;
      #endif
      drawImage( x * 8, 2, 8, NUM_ROWS, pSrc);
      pSrc++;
      delay(snowFall2Delay.toInt());
    }
  }
  delay(1000);
  clearMatrix();
}

// =======================================================================

void starrySky() {
  clearMatrix();
  uint8_t xPos[1000];
  uint8_t yPos[1000];
  for (uint16_t i = 0; i < starCount.toInt(); i++) {
    xPos[i] = 0;
    yPos[i] = 0;
  }
  clkTimeEffect = millis();
  while (millis() < (starDuration.toInt() * 1000) + clkTimeEffect) {
    for (uint16_t i = 0; i < starCount.toInt(); i++) {
      //Serial.println("star_i= " + String(i));
      //Serial.println("Deleting old position " + String(xPos[i]) + " , " + String(yPos[i]));
      drawPoint(xPos[i], yPos[i], 0);
      xPos[i] = random(0,64);
      yPos[i] = random(0,20);
      //Serial.println("Setting new position " + String(xPos[i]) + " , " + String(yPos[i]));
      drawPoint(xPos[i], yPos[i], 1);
      delay(starDelay.toInt());
    }
  }
  for (uint16_t i = 0; i < starCount.toInt(); i++) {
    delay(starDelay.toInt());
    drawPoint(xPos[i], yPos[i], 0);
  }
  delay(1000);
  clearMatrix();
}
/* --- end effect functions --- */

// =======================================================================

void testVerticalScroll() {
  for (uint8_t j = 0; j < 5; j++){
    for (uint8_t i = 0; i < 16; i++){
      //drawImage(xoffset, yoffset, width, height, *image);
      drawImage(32, 0 + i, 8, 16 - i, numbers + j * 16);
      delay(500);
      clearMatrix();
    }
  }
}

// =======================================================================

void setup() {  
  Serial.begin(115200);
  delay(10);
  Serial.println('\n');

  initLittleFS();
  readConfig();

  AsyncWiFiManager wifiManager(&server,&dns);
  
  if(!wifiManager.autoConnect("LED-Matrix")) {
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
    if (request->hasParam(PARAM_INPUT_01)) {
      scrollText1 = request->getParam(PARAM_INPUT_01)->value();
      if (request->hasParam(PARAM_INPUT_02)) {
        scrollText2 = request->getParam(PARAM_INPUT_02)->value();
      }
      if (request->hasParam(PARAM_INPUT_03)) {
        scrollText3 = request->getParam(PARAM_INPUT_03)->value();
      }
      if (request->hasParam(PARAM_INPUT_14)) {
        scrollSpeed = request->getParam(PARAM_INPUT_14)->value();
      }
      if (request->hasParam(PARAM_INPUT_30)) {
        text1Delay = request->getParam(PARAM_INPUT_30)->value();
      }
      if (request->hasParam(PARAM_INPUT_31)) {
        text2Delay = request->getParam(PARAM_INPUT_31)->value();
      }
      if (request->hasParam(PARAM_INPUT_32)) {
        text3Delay = request->getParam(PARAM_INPUT_32)->value();
      }
      if (request->hasParam(PARAM_INPUT_43)) {
        text1Font = request->getParam(PARAM_INPUT_43)->value();
      }
      if (request->hasParam(PARAM_INPUT_44)) {
        text2Font = request->getParam(PARAM_INPUT_44)->value();
      }
      if (request->hasParam(PARAM_INPUT_45)) {
        text3Font = request->getParam(PARAM_INPUT_45)->value();
      }
      if (request->hasParam(PARAM_INPUT_17)) {
        preTimeWeather = request->getParam(PARAM_INPUT_17)->value();
      }
      if (request->hasParam(PARAM_INPUT_18)) {
        preTimeText1 = request->getParam(PARAM_INPUT_18)->value();
      }
      if (request->hasParam(PARAM_INPUT_19)) {
        preTimeText2 = request->getParam(PARAM_INPUT_19)->value();
      }
      if (request->hasParam(PARAM_INPUT_21)) {
        preTimeText3 = request->getParam(PARAM_INPUT_21)->value();
      }
      if (request->hasParam(PARAM_INPUT_22)) {
        preTimeSnow = request->getParam(PARAM_INPUT_22)->value();
      }
      if (request->hasParam(PARAM_INPUT_35)) {
        preTimeSnowFall2 = request->getParam(PARAM_INPUT_35)->value();
      }
      if (request->hasParam(PARAM_INPUT_23)) {
        preTimeStar = request->getParam(PARAM_INPUT_23)->value();
      }
      if (request->hasParam(PARAM_INPUT_48)) {
        preTimeGrowingStar16x15 = request->getParam(PARAM_INPUT_48)->value();
      }
      if (request->hasParam(PARAM_INPUT_38)) {
        preTimeChristmasSymbols = request->getParam(PARAM_INPUT_38)->value();
      }
      if (request->hasParam(PARAM_INPUT_24)) {
        snowDuration = request->getParam(PARAM_INPUT_24)->value();
      }
      if (request->hasParam(PARAM_INPUT_25)) {
        snowDelay = request->getParam(PARAM_INPUT_25)->value();
      }
      if (request->hasParam(PARAM_INPUT_36)) {
        snowFall2Duration = request->getParam(PARAM_INPUT_36)->value();
      }
      if (request->hasParam(PARAM_INPUT_37)) {
        snowFall2Delay = request->getParam(PARAM_INPUT_37)->value();
      }
      if (request->hasParam(PARAM_INPUT_26)) {
        starDuration = request->getParam(PARAM_INPUT_26)->value();
      }
      if (request->hasParam(PARAM_INPUT_27)) {
        starDelay = request->getParam(PARAM_INPUT_27)->value();
      }
      if (request->hasParam(PARAM_INPUT_28)) {
        starCount = request->getParam(PARAM_INPUT_28)->value();
      }
      if (request->hasParam(PARAM_INPUT_46)) {
        growingStar16x15Duration = request->getParam(PARAM_INPUT_46)->value();
      }
      if (request->hasParam(PARAM_INPUT_47)) {
        growingStar16x15Delay = request->getParam(PARAM_INPUT_47)->value();
      }
      if (request->hasParam(PARAM_INPUT_39)) {
        christmasSymbolsDuration = request->getParam(PARAM_INPUT_39)->value();
      }
      if (request->hasParam(PARAM_INPUT_40)) {
        christmasSymbolsDelay = request->getParam(PARAM_INPUT_40)->value();
      }
      if (request->hasParam(PARAM_INPUT_20)) {
        cityID = request->getParam(PARAM_INPUT_20)->value();
      }
      if (request->hasParam(PARAM_INPUT_04)) {
        scrolltext1Checkbox = request->getParam(PARAM_INPUT_04)->value();
      } else {
        scrolltext1Checkbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_05)) {
        scrolltext2Checkbox = request->getParam(PARAM_INPUT_05)->value();
      } else {
        scrolltext2Checkbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_06)) {
        scrolltext3Checkbox = request->getParam(PARAM_INPUT_06)->value();
      } else {
        scrolltext3Checkbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_3)) {
        weatherCheckbox = request->getParam(PARAM_INPUT_3)->value();
        updCnt = 0;
      } else {
        weatherCheckbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_6)) {
        tempCheckbox = request->getParam(PARAM_INPUT_6)->value();
      } else {
        tempCheckbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_7)) {
        rainCheckbox = request->getParam(PARAM_INPUT_7)->value();
      } else {
        rainCheckbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_8)) {
        windCheckbox = request->getParam(PARAM_INPUT_8)->value();
      } else {
        windCheckbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_9)) {
        humidityCheckbox = request->getParam(PARAM_INPUT_9)->value();
      } else {
        humidityCheckbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_10)) {
        pressureCheckbox = request->getParam(PARAM_INPUT_10)->value();
      } else {
        pressureCheckbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_12)) {
        locationCheckbox = request->getParam(PARAM_INPUT_12)->value();
      } else {
        locationCheckbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_13)) {
        dateCheckbox = request->getParam(PARAM_INPUT_13)->value();
      } else {
        dateCheckbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_15)) {
        dotsCheckbox = request->getParam(PARAM_INPUT_15)->value();
      } else {
        dotsCheckbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_16)) {
        snowCheckbox = request->getParam(PARAM_INPUT_16)->value();
      } else {
        snowCheckbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_41)) {
        snowFall2Checkbox = request->getParam(PARAM_INPUT_41)->value();
      } else {
        snowFall2Checkbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_29)) {
        starCheckbox = request->getParam(PARAM_INPUT_29)->value();
      } else {
        starCheckbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_49)) {
        growingStar16x15Checkbox = request->getParam(PARAM_INPUT_49)->value();
      } else {
        growingStar16x15Checkbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_42)) {
        christmasSymbolsCheckbox = request->getParam(PARAM_INPUT_42)->value();
      } else {
        christmasSymbolsCheckbox = "unchecked";
      }
      if (request->hasParam(PARAM_INPUT_33)) {
        mirrorCheckbox = request->getParam(PARAM_INPUT_33)->value();
        mirror = 1;
      } else {
        mirrorCheckbox = "unchecked";
        mirror = 0;
      }
      if (request->hasParam(PARAM_INPUT_34)) {
        reverseCheckbox = request->getParam(PARAM_INPUT_34)->value();
        mask = 0xff;
      } else {
        reverseCheckbox = "unchecked";
        mask = 0x00;
      }
    }
    Serial.println("scrollText1 = " + scrollText1);
    Serial.println("scrollText2 = " + scrollText2);
    Serial.println("scrollText3 = " + scrollText3);
    Serial.println("scrolltext1Checkbox= " + scrolltext1Checkbox);
    Serial.println("scrolltext2Checkbox= " + scrolltext2Checkbox);
    Serial.println("scrolltext3Checkbox= " + scrolltext3Checkbox);
    Serial.println("dateCheckbox= " + dateCheckbox);
    Serial.println("weatherCheckbox= " + weatherCheckbox);
    Serial.println("tempCheckbox= " + tempCheckbox);
    Serial.println("rainCheckbox= " + rainCheckbox);
    Serial.println("windCheckbox= " + windCheckbox);
    Serial.println("humidityCheckbox= " + humidityCheckbox);
    Serial.println("pressureCheckbox= " + pressureCheckbox);
    Serial.println("locationCheckbox= " + locationCheckbox);
    Serial.println("dotsCheckbox= " + dotsCheckbox);
    Serial.println("snowCheckbox= " + snowCheckbox);
    Serial.println("snowFall2Checkbox= " + snowFall2Checkbox);
    Serial.println("starCheckbox= " + starCheckbox);
    Serial.println("christmasSymbolsCheckbox= " + christmasSymbolsCheckbox);
    Serial.println("mirrorCheckbox= " + mirrorCheckbox);
    Serial.println("reverseCheckbox= " + reverseCheckbox);
    Serial.println("scrollSpeed= " + String(scrollSpeed));
    Serial.println("text1Delay= " + String(text1Delay));
    Serial.println("text2Delay= " + String(text2Delay));
    Serial.println("text3Delay= " + String(text3Delay));
    Serial.println("text1Font= " + String(text1Font));
    Serial.println("text2Font= " + String(text2Font));
    Serial.println("text3Font= " + String(text3Font));
    Serial.println("preTimeWeather= " + String(preTimeWeather));
    Serial.println("preTimeText1= " + String(preTimeText1));
    Serial.println("preTimeText2= " + String(preTimeText2));
    Serial.println("preTimeText3= " + String(preTimeText3));
    Serial.println("preTimeSnow= " + String(preTimeSnow));
    Serial.println("preTimeSnowFall2= " + String(preTimeSnowFall2));
    Serial.println("preTimeStar= " + String(preTimeStar));
    Serial.println("preTimeChristmasSymbols= " + String(preTimeChristmasSymbols));
    Serial.println("snowDuration= " + String(snowDuration));
    Serial.println("snowFall2Duration= " + String(snowFall2Duration));
    Serial.println("starDuration= " + String(starDuration));
    Serial.println("christmasSymbolsDuration= " + String(christmasSymbolsDuration));
    Serial.println("snowDelay= " + String(snowDelay));
    Serial.println("snowFall2Delay= " + String(snowFall2Delay));
    Serial.println("starDelay= " + String(starDelay));
    Serial.println("christmasSymbolsDelay= " + String(christmasSymbolsDelay));
    Serial.println("starCount= " + String(starCount));
    Serial.println("cityID= " + String(cityID));
    writeConfig();
    request->redirect("/");
  });
  
  server.onNotFound(notFound);

  AsyncElegantOTA.begin(&server);   // je nach Library (IDE oder GitHub)
  // AsyncElegantOTA.begin(server);    // je nach Library (IDE oder GitHub)
  server.begin();
  Serial.println("HTTP webserver started.");
  
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("Waiting for time sync...");
  setSyncProvider(getNtpTime);
  setSyncInterval(86400);                            // Anzahl der Sekunden zwischen dem erneuten Synchronisieren ein. 86400 = 1 Tag

  delay(2000);
  wipeLeftShift();
  delay(500);

  String startString = "Webserver-IP: " + WiFi.localIP().toString() + "                       ";
  h_TextScroll_8x16(startString.c_str(), 25);

  clkTimePre = millis();
}

// =======================================================================

void loop() {
  AsyncElegantOTA.loop();
  showClock();

  /*
  // Intervall fuer Online Wetter Daten Aktualisierung
  if (millis() > timeBetweenUpdates.toInt() * 1000 + clkTimeWeatherUpdate) {
    clkTimeWeatherUpdate = millis();
    if (weatherCheckbox == "checked") {
      Serial.println("Getting online weather data...");
      getWeatherData();
    }
  }
  */

  switch (state) {   // https://www.instructables.com/id/Finite-State-Machine-on-an-Arduino/
    case 1:   // Wetter
      if (weatherCheckbox == "checked") {
        if (millis() > (preTimeWeather.toInt() * 1000) + clkTimePre) {
          wipeRandom();
          updCnt--;
          Serial.println("updCnt=" + String(updCnt));
          if (updCnt <= 0) {
            updCnt = 10;
            getWeatherData();
          }
          scrollString = "";
          if (dateCheckbox == "checked") {
            scrollString += dayName[w]  + ", " + String(d) + "." + String(mo) + "." + String(ye) + "    ";
          }
          scrollString += weatherString;
          scrollString += "     ";
          // String fuer Scroll-Funktion in Array wandeln
          uint8_t stringlength = scrollString.length() + 1;
          char charBuf[stringlength];
          scrollString.toCharArray(charBuf, stringlength);
          // scrollen
          Serial.println("Start scrolling online weather data...");
          // horTextScroll_16x20(charBuf, stringlength);
          h_TextScroll_16x20_v1(charBuf, stringlength, scrollSpeed.toInt());
          Serial.println("Stop scrolling online weather data.");
          state = 2;
          clkTimePre = millis();
        }
      } else {
        clkTimePre = millis();
        state = 2;
      }
      break;
  
    case 2:   // Scrolltext 1
      if (scrolltext1Checkbox == "checked") {
        if (millis() > (preTimeText1.toInt() * 1000) + clkTimePre) {
          wipeRandom();
          Serial.println("Start scrolling scrollText1...");
          switch (text1Font.toInt()){
            case 1:
              h_TextScroll_8x16(scrollText1.c_str(), text1Delay.toInt());
              break;
            case 2:
              h_TextScroll_16x20_v2(scrollText1.c_str(), text1Delay.toInt());
              break;
            case 3:
              uint8_t rand = random(1,3);
              Serial.println("random: " + String(rand));
              if (rand == 1) {
                h_TextScroll_8x16(scrollText1.c_str(), text1Delay.toInt());
              } else {
                h_TextScroll_16x20_v2(scrollText1.c_str(), text1Delay.toInt());
              }
              break;
          }
          Serial.println("Stop scrolling scrollText1.");
          state = 3;
          clkTimePre = millis();
        }
      } else {
        clkTimePre = millis();
        state = 3;
      }
      break;
      
    case 3:   // Scrolltext 2
      if (scrolltext2Checkbox == "checked") {
        if (millis() > (preTimeText2.toInt() * 1000) + clkTimePre) {
          wipeRandom();
          Serial.println("Start scrolling scrollText2...");
          switch (text2Font.toInt()){
            case 1:
              h_TextScroll_8x16(scrollText2.c_str(), text2Delay.toInt());
              break;
            case 2:
              h_TextScroll_16x20_v2(scrollText2.c_str(), text2Delay.toInt());
              break;
            case 3:
              uint8_t rand = random(1,3);
              Serial.println("random: " + String(rand));
              if (rand == 1) {
                h_TextScroll_8x16(scrollText2.c_str(), text2Delay.toInt());
              } else {
                h_TextScroll_16x20_v2(scrollText2.c_str(), text2Delay.toInt());
              }
              break;
          }
          Serial.println("Stop scrolling scrollText2.");
          state = 4;
          clkTimePre = millis();
        }
      } else {
        clkTimePre = millis();
        state = 4;
      }
      break;
  
    case 4:   // Scrolltext 3
      if (scrolltext3Checkbox == "checked") {
        if (millis() > (preTimeText3.toInt() * 1000) + clkTimePre) {
          wipeRandom();
          Serial.println("Start scrolling scrollText3...");
          switch (text3Font.toInt()){
            case 1:
              h_TextScroll_8x16(scrollText3.c_str(), text3Delay.toInt());
              break;
            case 2:
              h_TextScroll_16x20_v2(scrollText3.c_str(), text3Delay.toInt());
              break;
            case 3:
              uint8_t rand = random(1,3);
              Serial.println("random: " + String(rand));
              if (rand == 1) {
                h_TextScroll_8x16(scrollText3.c_str(), text3Delay.toInt());
              } else {
                h_TextScroll_16x20_v2(scrollText3.c_str(), text3Delay.toInt());
              }
              break;
          }
          Serial.println("Stop scrolling scrollText3.");
          state = 5;
          clkTimePre = millis();
        }
      } else {
        clkTimePre = millis();
        state = 5;
      }
      break;
  
    case 5:   // Schneefall viele Flocken
      if (snowCheckbox == "checked") {
        if (millis() > (preTimeSnow.toInt() * 1000) + clkTimePre) {
          wipeRandom();
          Serial.println("Start falling snow...");
          snowFall();
          Serial.println("Stop falling snow.");
          state = 6;
          clkTimePre = millis();
        }
      } else {
        clkTimePre = millis();
        state = 6;
      }
      break;
  
    case 6:   // Weihnachtssymbole
      if (christmasSymbolsCheckbox == "checked") {
        if (millis() > (preTimeChristmasSymbols.toInt() * 1000) + clkTimePre) {
          wipeRandom();
          Serial.println("Start drawChristmasSymbols...");
          drawChristmasSymbols();
          Serial.println("Stop drawChristmasSymbols.");
          state = 7;
          clkTimePre = millis();
        }
      } else {
        clkTimePre = millis();
        state = 7;
      }
      break;
  
    case 7:   // Schneefall einzelne Flocken
      if (snowFall2Checkbox == "checked") {
        if (millis() > (preTimeSnowFall2.toInt() * 1000) + clkTimePre) {
          wipeRandom();
          Serial.println("Start snowFall2...");
          snowFall2();
          Serial.println("Stop snowFall2.");
          state = 8;
          clkTimePre = millis();
        }
      } else {
        clkTimePre = millis();
        state = 8;
      }
      break;
  
    case 8:   // Sternenhimmel
      if (starCheckbox == "checked") {
        if (millis() > (preTimeStar.toInt() * 1000) + clkTimePre) {
          wipeRandom();
          Serial.println("Start star sky...");
          starrySky();
          Serial.println("Stop star sky.");
          state = 9;
          clkTimePre = millis();
        }
      } else {
        clkTimePre = millis();
        state = 9;
      }
      break;
  
    case 9:   // wachsende Schneeflocke
      if (growingStar16x15Checkbox == "checked") {
        if (millis() > (preTimeGrowingStar16x15.toInt() * 1000) + clkTimePre) {
          wipeRandom();
          Serial.println("Start growingStar_16x15...");
          growingStar_16x15();
          Serial.println("Stop growingStar_16x15.");
          state = 1;
          clkTimePre = millis();
        } 
      } else {
        clkTimePre = millis();
        state = 1;
      }
      break;
  } 
}

// =======================================================================
