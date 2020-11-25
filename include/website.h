#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>LED Lichternetz Konfiguration</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <table>
    <tr><td>Verbunden mit:</td><td>%NETWORK%</td></tr>
    <tr><td>Signal-St&auml;rke:</td><td>%RSSI% &#037;</td></tr>
  </table>
  <br><br>
  <form action="/get">
    <table>
      <tr><td>Scrolltext 1</td><td><input type="text" size="80" name="scrollText1" value="%SCROLLTEXT1OPTIONS%" required onchange="this.form.submit();"></td></tr>
      <tr><td>Scrolltext 2</td><td><input type="text" size="80" name="scrollText2" value="%SCROLLTEXT2OPTIONS%" required onchange="this.form.submit();"></td></tr>
      <tr><td>Scrolltext 3</td><td><input type="text" size="80" name="scrollText3" value="%SCROLLTEXT3OPTIONS%" required onchange="this.form.submit();"></td></tr>
    </table>
    <br><br>
    <table>
      <tr>
        <td></td>
        <td>aktiv</td>
        <td>Vorlauf</td>
        <td>Speed</td>
        <td>Dauer</td>
        <td>Font</td>
      </tr>
      <tr>
        <td>Wetter </td>
        <td><input type="checkbox" name="weatherCheckbox" value="checked" %ENABLE_WEATHER_INPUT% onClick="this.form.submit();"></td>
        <td><input name="preTimeWeather" type="number" min="0" max="300" value="%PRETIMEWEATHEROPTIONS%"></td>
        <!-- <td><select name="scrollSpeed">%SCROLLSPEEDOPTIONS%</select></td> -->
        <td><input name="scrollSpeed" type="number" min="5" max="1000" value=%SCROLLSPEEDOPTIONS%></td>
        <td></td>
      </tr>
      <tr>
        <td>Text 1 </td>
        <td><input type="checkbox" name="scrolltext1Checkbox" value="checked" %SCROLLTEXT1_CHECKBOX_OPTIONS% onClick="this.form.submit();"></td>
        <td><input name="preTimeText1" type="number" min="0" max="300" value="%PRETIMETEXT1OPTIONS%"></td>
        <td><input name="text1Delay" type="number" min="5" max="1000" value=%TEXT1DELAYOPTIONS%></td>
        <td></td>
        <td><select name="text1Font" onchange="this.form.submit();">%TEXT1FONTOPTIONS%</select></td>
      </tr>
      <tr>
        <td>Text 2 </td>
        <td><input type="checkbox" name="scrolltext2Checkbox" value="checked" %SCROLLTEXT2_CHECKBOX_OPTIONS% onClick="this.form.submit();"></td>
        <td><input name="preTimeText2" type="number" min="0" max="300" value="%PRETIMETEXT2OPTIONS%"></td>
        <td><input name="text2Delay" type="number" min="5" max="1000" value=%TEXT2DELAYOPTIONS%></td>
        <td></td>
        <td><select name="text2Font" onchange="this.form.submit();">%TEXT2FONTOPTIONS%</select></td>
      </tr>
      <tr>
        <td>Text 3 </td>
        <td><input type="checkbox" name="scrolltext3Checkbox" value="checked" %SCROLLTEXT3_CHECKBOX_OPTIONS% onClick="this.form.submit();"></td>
        <td><input name="preTimeText3" type="number" min="0" max="300" value="%PRETIMETEXT3OPTIONS%"></td>
        <td><input name="text3Delay" type="number" min="5" max="1000" value=%TEXT3DELAYOPTIONS%></td>
        <td></td>
        <td><select name="text3Font" onchange="this.form.submit();">%TEXT3FONTOPTIONS%</select></td>
      </tr>
      <tr>
        <td>Schneefall (multi) </td>
        <td><input type="checkbox" name="snowCheckbox" value="checked" %ENABLE_SNOW_INPUT% onClick="this.form.submit();"></td>
        <td><input name="preTimeSnow" type="number" min="0" max="300" value="%PRETIMESNOWOPTIONS%"></td>
        <td><input name="snowDelay" type="number" min="5" max="1000" value=%SNOWDELAYOPTIONS%></td>
        <td><input name="snowDuration" type="number" min="0" max="300" value="%SNOWDURATIONOPTIONS%"></td>
      </tr>
      <tr>
        <td>Schneefall (single) </td>
        <td><input type="checkbox" name="snowFall2Checkbox" value="checked" %ENABLE_SNOWFALL2_INPUT% onClick="this.form.submit();"></td>
        <td><input name="preTimeSnowFall2" type="number" min="0" max="300" value="%PRETIMESNOWFALL2OPTIONS%"></td>
        <td><input name="snowFall2Delay" type="number" min="5" max="1000" value=%SNOWFALL2DELAYOPTIONS%></td>
        <td><input name="snowFall2Duration" type="number" min="0" max="300" value="%SNOWFALL2DURATIONOPTIONS%"></td>
      </tr>
      <tr>
        <td>Symbole </td>
        <td><input type="checkbox" name="christmasSymbolsCheckbox" value="checked" %ENABLE_CHRISTMASSYMBOLS_INPUT% onClick="this.form.submit();"></td>
        <td><input name="preTimeChristmasSymbols" type="number" min="0" max="300" value="%PRETIMECHRISTMASSYMBOLSOPTIONS%"></td>
        <td><input name="christmasSymbolsDelay" type="number" min="5" max="1000" value=%CHRISTMASSYMBOLSDELAYOPTIONS%></td>
        <td><input name="christmasSymbolsDuration" type="number" min="0" max="300" value="%CHRISTMASSYMBOLSDURATIONOPTIONS%"></td>
      </tr>
      <tr>
        <td>Sternenhimmel </td>
        <td><input type="checkbox" name="starCheckbox" value="checked" %ENABLE_STAR_INPUT% onClick="this.form.submit();"></td>
        <td><input name="preTimeStar" type="number" min="0" max="300" value="%PRETIMESTAROPTIONS%"></td>
        <td><input name="starDelay" type="number" min="5" max="1000" value=%STARDELAYOPTIONS%></td>
        <td><input name="starDuration" type="number" min="0" max="300" value="%STARDURATIONOPTIONS%"></td>
      </tr>
      <tr>
        <td>Wachsende Schneeflocke </td>
        <td><input type="checkbox" name="growingStar16x15Checkbox" value="checked" %ENABLE_GROWINGSTAR16X15_INPUT% onClick="this.form.submit();"></td>
        <td><input name="preTimeGrowingStar16x15" type="number" min="0" max="300" value="%PRETIMEGROWINGSTAR16X15OPTIONS%"></td>
        <td><input name="growingStar16x15Delay" type="number" min="5" max="1000" value=%GROWINGSTAR16X15DELAYOPTIONS%></td>
        <td><input name="growingStar16x15Duration" type="number" min="0" max="300" value="%GROWINGSTAR16X15DURATIONOPTIONS%"></td>
      </tr>
    </table>
    <br><br>
    <table>
    <tr><td>Scrollrichtung umkehren</td><td><input type="checkbox" name="mirrorCheckbox" value="checked" %ENABLE_MIRROR_INPUT% onClick="this.form.submit();"></td></tr>
    <tr><td>Display invertieren</td><td><input type="checkbox" name="reverseCheckbox" value="checked" %ENABLE_REVERSE_INPUT% onClick="this.form.submit();"></td></tr>
    <tr><td>OWMorg City-ID</td><td><input name="cityID" type="text" size="7" value="%CITYIDOPTIONS%">&nbsp;%WEATHERLOCATION%</td></tr>
    <tr><td>Anzahl Sterne</td><td><input name="starCount" type="number" min="1" max="1000" value=%STARCOUNTOPTIONS%></td></tr>
    <tr><td>Datum anzeigen</td><td><input type="checkbox" name="dateCheckbox" value="checked" %ENABLE_DATE_INPUT% onClick="this.form.submit();"></td></tr>
    <tr><td>Temperatur anzeigen</td><td><input type="checkbox" name="tempCheckbox" value="checked" %ENABLE_TEMP_INPUT% onClick="this.form.submit();"></td></tr>
    <tr><td>Regen anzeigen</td><td><input type="checkbox" name="rainCheckbox" value="checked" %ENABLE_RAIN_INPUT% onClick="this.form.submit();"></td></tr>
    <tr><td>Wind anzeigen</td><td><input type="checkbox" name="windCheckbox" value="checked" %ENABLE_WIND_INPUT% onClick="this.form.submit();"></td></tr>
    <tr><td>Luftfeuchte anzeigen</td><td><input type="checkbox" name="humidityCheckbox" value="checked" %ENABLE_HUMIDITY_INPUT% onClick="this.form.submit();"></td></tr>
    <tr><td>Luftdruck anzeigen</td><td><input type="checkbox" name="pressureCheckbox" value="checked" %ENABLE_PRESSURE_INPUT% onClick="this.form.submit();"></td></tr>
    <tr><td>Ort anzeigen</td><td><input type="checkbox" name="locationCheckbox" value="checked" %ENABLE_LOCATION_INPUT% onClick="this.form.submit();"></td></tr>
    <tr><td>Uhr animieren</td><td><input type="checkbox" name="animClockCheckbox" value="checked" %ENABLE_ANIMCLOCK_INPUT% onClick="this.form.submit();"></td></tr>
    <tr><td>Blinkende Doppelpunkte</td><td><input type="checkbox" name="dotsCheckbox" value="checked" %ENABLE_DOTS_INPUT% onClick="this.form.submit();"></td></tr>
    </table>
    <br><br>
    <a href="/ota">Firmwareupdate</a>
    <br><br>
    <input type="submit" value="speichern">
  </form>
</body></html>)rawliteral";