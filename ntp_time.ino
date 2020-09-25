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
  const uint8_t *pSrc1 = numbers + 16 * dig[0];
  drawImage( 3, 2, 8, 16, pSrc1);
  const uint8_t *pSrc2 = numbers + 16 * dig[1];
  drawImage(12, 2, 8, 16, pSrc2);
  const uint8_t *pSrc3 = numbers + 16 * dig[2];
  drawImage(24, 2, 8, 16, pSrc3);
  const uint8_t *pSrc4 = numbers + 16 * dig[3];
  drawImage(33, 2, 8, 16, pSrc4);
  const uint8_t *pSrc5 = numbers + 16 * dig[4];
  drawImage(45, 2, 8, 16, pSrc5);
  const uint8_t *pSrc6 = numbers + 16 * dig[5];
  drawImage(54, 2, 8, 16, pSrc6);

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
