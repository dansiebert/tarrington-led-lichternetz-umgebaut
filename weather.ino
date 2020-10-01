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
      weatherString = "Temp. in " + weatherLocation + ": " + String(onlinetemp, 1) + "`C    ";   // `-Zeichen ist im Font als Grad definiert
    } else {
      weatherString = "Temp.: " + String(onlinetemp, 1) + "`C    ";
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
