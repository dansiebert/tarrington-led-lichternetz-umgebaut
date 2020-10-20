// =======================================================================

void wipeHorizontal() {
  clearMatrix();
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

void horTextScroll_16x20(const char *s, uint8_t q, uint8_t sdelay) {          // s = Text (Array); q = Textlänge
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

void hTextScroll16x20(const char *s, uint8_t sdelay) {                        // s = Text (Array); sdelay = Speed (Verzögerung)
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

void hTextScroll8x16(const char *s, uint8_t sdelay) {                         // s = Text (Array); sdelay = Speed (Verzögerung)
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

    charWidth = smallFont[(msglineindex - 32) * 17 + 16];                  // Zeichenbreite (17. Byte eines Zeichens im Font)
    for (uint8_t row = 0; row < NUM_ROWS; row++) {                          // nächstes Zeichen zeilenweise in Puffer laden (1 Byte)
      buf1[row] = smallFont[(msglineindex - 32) * 17 + row];
    }
    for (uint8_t shift = 0; shift < charWidth; shift++) {                   // Bit fuer Bit um Zeichenbreite des aktuellen Zeichens nach links shiften
      for (uint8_t row = 0; row < NUM_ROWS; row++) {                        // dabei Zeile für Zeile durchgehen
        uint8_t *pDst = displaybuf + row * 8;                               // Pointer auf erstes (linkes) Byte der aktuellen Zeile des Displaypuffers setzen
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

void snowFall() {   // Schneeflocke vertikal scrollen
  clearMatrix();
  clkTime6 = millis();
  
  uint8_t starimage[8];
  uint8_t startdelay[8];
  uint8_t speeddelay[8];
  uint8_t startdelaycount[8];
  uint8_t speeddelaycount[8];
  for (uint8_t zone = 0; zone < 8; zone++) {
    starimage[zone] = 0;
    startdelay[zone] = random(0,50);
    speeddelay[zone] = random(0,5);
    startdelaycount[zone] = 0;
    speeddelaycount[zone] = 0;
  }

  while (millis() < (snowDuration.toInt() * 1000) + clkTime6) {
    for (uint8_t zone = 0; zone < 8; zone++) {
      if ( startdelaycount[zone] >= startdelay[zone] ) {                     // Start der Flocke um eine zufaellige Zeit verzögern
        drawImage( zone * 8, 2, 8, 20, stars20 + starimage[zone] * 20);      // passendes Image aus stars-Font-Array zeichnen
        if (speeddelaycount[zone] >= speeddelay[zone]) {
          speeddelaycount[zone] = 0;
          starimage[zone]++;                                                 // Flocke eins runter
        }
        speeddelaycount[zone]++;
        if ( starimage[zone] == 28 ) {                                       // letzter Zustand erreicht (Flocke unten rausgescrollt)
          starimage[zone] = 0;
          startdelaycount[zone] = 0;
          startdelay[zone] = random(0,50);
          speeddelay[zone] = random(0,5);
        }
      }
      startdelaycount[zone]++;
    }
    delay(snowDelay.toInt());
  }
  
  delay(1000);
  clearMatrix();
}

// =======================================================================

void snowFall2() {   // 1 Schneeflocke an verschiedenen Positionen vertikal scrollen
  clearMatrix();
  clkTime6 = millis();
  while (millis() < (snowDuration.toInt() * 1000) + clkTime6) {
    uint8_t x = random(0,7);
    Serial.println("snow_pos= " + String(x * 8));
    for (uint16_t i = 0; i < 23; i++) {
      //Serial.println("snow_char= " + String(i));
      const uint8_t *pSrc = stars16 + i * 16;
      drawImage( x * 8, 2, 8, 16, pSrc);
      pSrc++;
      delay(snowDelay.toInt());
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
  clkTime8 = millis();
  while (millis() < (starDuration.toInt() * 1000) + clkTime8) {
    for (uint16_t i = 0; i < starCount.toInt(); i++) {
      //Serial.println("star_i= " + String(i));
      //Serial.println("Deleting old position " + String(xPos[i]) + " , " + String(yPos[i]));
      drawPoint(xPos[i], yPos[i], 0);
      xPos[i] = random(0,63);
      yPos[i] = random(0,19);
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

// =======================================================================

