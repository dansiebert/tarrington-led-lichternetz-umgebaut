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
    charWidth = ArialRoundL[(msglineindex - 32) * 41 + 40];                 // Zeichenbreite (41. Byte jedes Zeichens im Font)
    for (uint8_t row = 0; row < NUM_ROWS; row++) {                          // nächstes Zeichen in Puffer laden (2 Byte breit)
      buf1[row] = ArialRoundL[(msglineindex - 32) * 41 + row + bytecount];  // erstes Byte des 2 Byte breiten Zeichens
      bytecount++;
      buf2[row] = ArialRoundL[(msglineindex - 32) * 41 + row + bytecount];  // zweites Byte des 2 Byte breiten Zeichens
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
/*
uint8_t Buffer[16]  = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};               // die 16 Zeilen des Fonts (8x16)
uint8_t Buffer2[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t Buffer3[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t Buffer4[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t Buffer5[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t Buffer6[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t Buffer7[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t Buffer8[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
char message[] = "   AAAA   ";
uint8_t MessageIndex;
uint8_t temp;

void horTextScroll_16x16() {
  Serial.println("Start horTextScroll_16x16...");
  for (uint8_t k = 0; k < (sizeof(message)-1); k++){                   // gesamte Message Zeichen für Zeichen durchgehen
    MessageIndex = message[k];                                         // k'ter Buchstabe der Message
    for (uint8_t scroll = 0; scroll < 8; scroll++) {                   // Zeichenbreite (hier 8 Bit) shiften
      for (uint8_t row = 0; row < NUM_ROWS; row++){                          // 16 Zeilen durchzählen
        temp = smallFont[(MessageIndex - 32) * 16 + row];              // aktuellen Zeile (8 Bit) des aktuellen Zeichens der Message
        Buffer2[row] = displaybuf[row * 8 + 6];                        // Byte in Zone 6 merken
        Buffer3[row] = displaybuf[row * 8 + 5];
        Buffer4[row] = displaybuf[row * 8 + 4];
        Buffer5[row] = displaybuf[row * 8 + 3];
        Buffer6[row] = displaybuf[row * 8 + 2];
        Buffer7[row] = displaybuf[row * 8 + 1];
        Buffer8[row] = displaybuf[row * 8 + 0];
        Buffer[row] = (Buffer[row] << 1) | (temp >> (7 - scroll));     // aktuelle Zeile shiften (Zeichen temp in Buffer reinlaufen lassen)
        Buffer2[row] = Buffer2[row] << 1;                              // Byte in Zone 1 Bit nach links
        Buffer3[row] = Buffer3[row] << 1;
        Buffer4[row] = Buffer4[row] << 1;
        Buffer5[row] = Buffer5[row] << 1;
        Buffer6[row] = Buffer6[row] << 1;
        Buffer7[row] = Buffer7[row] << 1;
        Buffer8[row] = Buffer8[row] << 1;
        bitWrite(Buffer2[row],0 , bitRead(Buffer[row],7));             // rausfliegendes Bit aus vorheriger Zone in diese Zone als letztes Bit reinnehmen
        bitWrite(Buffer3[row],0 , bitRead(Buffer2[row],7));
        bitWrite(Buffer4[row],0 , bitRead(Buffer3[row],7));
        bitWrite(Buffer5[row],0 , bitRead(Buffer4[row],7));
        bitWrite(Buffer6[row],0 , bitRead(Buffer5[row],7));
        bitWrite(Buffer7[row],0 , bitRead(Buffer6[row],7));
        bitWrite(Buffer8[row],0 , bitRead(Buffer7[row],7));
      }
      uint8_t* pDst = displaybuf + 7;
      const uint8_t* pSrc = Buffer;
      uint8_t* pDst2 = displaybuf + 6;
      const uint8_t* pSrc2 = Buffer2;
      uint8_t* pDst3 = displaybuf + 5;
      const uint8_t* pSrc3 = Buffer3;
      uint8_t* pDst4 = displaybuf + 4;
      const uint8_t* pSrc4 = Buffer4;
      uint8_t* pDst5 = displaybuf + 3;
      const uint8_t* pSrc5 = Buffer5;
      uint8_t* pDst6 = displaybuf + 2;
      const uint8_t* pSrc6 = Buffer6;
      uint8_t* pDst7 = displaybuf + 1;
      const uint8_t* pSrc7 = Buffer7;
      uint8_t* pDst8 = displaybuf + 0;
      const uint8_t* pSrc8 = Buffer8;
      for (uint8_t i = 0; i < NUM_ROWS; i++) {   // alle 16 Zeilen anzeigen (in displaybuf) übertragen
        *pDst = *pSrc;
        pDst += 8;
        pSrc++;
        *pDst2 = *pSrc2;
        pDst2 += 8;
        pSrc2++;
        *pDst3 = *pSrc3;
        pDst3 += 8;
        pSrc3++;
        *pDst4 = *pSrc4;
        pDst4 += 8;
        pSrc4++;
        *pDst5 = *pSrc5;
        pDst5 += 8;
        pSrc5++;
        *pDst6 = *pSrc6;
        pDst6 += 8;
        pSrc6++;
        *pDst7 = *pSrc7;
        pDst7 += 8;
        pSrc7++;
        *pDst8 = *pSrc8;
        pDst8 += 8;
        pSrc8++;
      }
      delay(90);
    }
  }
}
*/

// =======================================================================

void snowFall() {   // Schneeflocke vertikal scrollen
  clearMatrix();
  clkTime6 = millis();
  // s = Zaehler fuer Zustand der Flocke (Pointer auf Image aus stars-Array)
  // t = Zaehler, ob Verzögerung erreicht - dann wird Flocke gestartet
  // r = Zufallswert für Start-Verzoegerung einer Flocke
  // v = Zufalsswert fuer Fallgeschwindigkeit (Verzoegerung)
  // w = Zaehler, ob Speed-Verzögerung erreicht - dann Flocke eins weiter
  uint8_t s1 = 0;
  uint8_t s2 = 0;
  uint8_t s3 = 0;
  uint8_t s4 = 0;
  uint8_t s5 = 0;
  uint8_t s6 = 0;
  uint8_t s7 = 0;
  uint8_t s8 = 0;
  uint8_t t1 = 0;
  uint8_t t2 = 0;
  uint8_t t3 = 0;
  uint8_t t4 = 0;
  uint8_t t5 = 0;
  uint8_t t6 = 0;
  uint8_t t7 = 0;
  uint8_t t8 = 0;
  uint8_t r1 = random(0,50);
  uint8_t r2 = random(0,50);
  uint8_t r3 = random(0,50);  
  uint8_t r4 = random(0,50);
  uint8_t r5 = random(0,50);
  uint8_t r6 = random(0,50);  
  uint8_t r7 = random(0,50);
  uint8_t r8 = random(0,50);
  uint8_t v1 = random(0,5);
  uint8_t w1 = 0;
  uint8_t v2 = random(0,5);
  uint8_t w2 = 0;
  uint8_t v3 = random(0,5);
  uint8_t w3 = 0;
  uint8_t v4 = random(0,5);
  uint8_t w4 = 0;
  uint8_t v5 = random(0,5);
  uint8_t w5 = 0;
  uint8_t v6 = random(0,5);
  uint8_t w6 = 0;
  uint8_t v7 = random(0,5);
  uint8_t w7 = 0;
  uint8_t v8 = random(0,5);
  uint8_t w8 = 0;

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
      if ( startdelaycount[zone] >= startdelay[zone] ) {                 // Start der Flocke um eine zufaellige Zeit verzögern
        drawImage( zone * 8, 2, 8, 16, stars + starimage[zone] * 16);    // passendes Image aus stars-Font-Array zeichnen
        if (speeddelaycount[zone] >= speeddelay[zone]) {
          speeddelaycount[zone] = 0;
          starimage[zone]++;                                             // Flocke eins runter
        }
        speeddelaycount[zone]++;
        if ( starimage[zone] == 23 ) {                                   // letzter Zustand erreicht (Flocke unten rausgescrollt)
          starimage[zone] = 0;
          startdelaycount[zone] = 0;
          startdelay[zone] = random(0,50);
          speeddelay[zone] = random(0,5);
        }
      }
      startdelaycount[zone]++;
    }
    delay(snowDelay.toInt());

/*
    if ( t1 >= r1 ) {                              // Start der Flocke um eine zufaellige Zeit verzögern
      drawImage( 56, 2, 8, 16, stars + s1 * 16);   // passendes Image aus stars-Font-Array zeichnen
      if (w1 >= v1) {
        w1 = 0;
        s1++;                                      // Flocke eins runter
      }
      w1++;
      if ( s1 == 23 ) {                            // letzter Zustand erreicht (Flocke unten rausgescrollt)
        s1 = 0;
        t1 = 0;
        r1 = random(0,50);
        v1 = random(0,5);
      }
    }

    if ( t2 >= r2 ) {
      drawImage( 48, 2, 8, 16, stars + s2 * 16);
      if (w2 >= v2) {
        w2 = 0;
        s2++;                                      // Flocke eins runter
      }
      w2++;
      if ( s2 == 23 ) {                            // letzter Zustand erreicht (Flocke unten rausgescrollt)
        s2 = 0;
        t2 = 0;
        r2 = random(0,50);
        v2 = random(0,5);
      }
    }

    if ( t3 >= r3 ) {
      drawImage( 40, 2, 8, 16, stars + s3 * 16);
      if (w3 >= v3) {
        w3 = 0;
        s3++;                                      // Flocke eins runter
      }
      w3++;
      if ( s3 == 23 ) {                            // letzter Zustand erreicht (Flocke unten rausgescrollt)
        s3 = 0;
        t3 = 0;
        r3 = random(0,50);
        v3 = random(0,5);
      }
    }

    if ( t4 >= r4 ) {
      drawImage( 32, 2, 8, 16, stars + s4 * 16);
      if (w4 >= v4) {
        w4 = 0;
        s4++;                                      // Flocke eins runter
      }
      w4++;
      if ( s4 == 23 ) {                            // letzter Zustand erreicht (Flocke unten rausgescrollt)
        s4 = 0;
        t4 = 0;
        r4 = random(0,50);
        v4 = random(0,5);
      }
    }

    if ( t5 >= r5 ) {
      drawImage( 24, 2, 8, 16, stars + s5 * 16);
      if (w5 >= v5) {
        w5 = 0;
        s5++;                                      // Flocke eins runter
      }
      w5++;
      if ( s5 == 23 ) {                            // letzter Zustand erreicht (Flocke unten rausgescrollt)
        s5 = 0;
        t5 = 0;
        r5 = random(0,50);
        v5 = random(0,5);
      }
    }

    if ( t6 >= r6 ) {
      drawImage( 16, 2, 8, 16, stars + s6 * 16);
      if (w6 >= v6) {
        w6 = 0;
        s6++;                                      // Flocke eins runter
      }
      w6++;
      if ( s6 == 23 ) {                            // letzter Zustand erreicht (Flocke unten rausgescrollt)
        s6 = 0;
        t6 = 0;
        r6 = random(0,50);
        v6 = random(0,5);
      }
    }

    if ( t7 >= r7 ) {
      drawImage( 8, 2, 8, 16, stars + s7 * 16);
      if (w7 >= v7) {
        w7 = 0;
        s7++;                                      // Flocke eins runter
      }
      w7++;
      if ( s7 == 23 ) {                            // letzter Zustand erreicht (Flocke unten rausgescrollt)
        s7 = 0;
        t7 = 0;
        r7 = random(0,50);
        v7 = random(0,5);
      }
    }

    if ( t8 >= r8 ) {
      drawImage( 0, 2, 8, 16, stars + s8 * 16);
      if (w8 >= v8) {
        w8 = 0;
        s8++;                                      // Flocke eins runter
      }
      w8++;
      if ( s8 == 23 ) {                            // letzter Zustand erreicht (Flocke unten rausgescrollt)
        s8 = 0;
        t8 = 0;
        r8 = random(0,50);
        v8 = random(0,5);
      }
    }

    t1++;
    t2++;
    t3++;
    t4++;
    t5++;
    t6++;
    t7++;
    t8++;
    delay(snowDelay.toInt());
*/
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
      const uint8_t *pSrc = stars + i * 16;
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