#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>LED Lichternetz Konfiguration</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <table>
    <tr><td>Verbunden mit:</td><td>%SSID%</td></tr>
    <tr><td>Signal-St&auml;rke:</td><td>%RSSI% &#037;</td></tr>
  </table>
  <br><br>
  <form action="/get">
    <table>
      <tr><td>Text 1 </td><td><input type="text" size="80" name="scrollText1" value="%SCROLLTEXT1_INPUT%" required onchange="this.form.submit();"></td></tr>
      <tr><td>Text 2 </td><td><input type="text" size="80" name="scrollText2" value="%SCROLLTEXT2_INPUT%" required onchange="this.form.submit();"></td></tr>
      <tr><td>Text 3 </td><td><input type="text" size="80" name="scrollText3" value="%SCROLLTEXT3_INPUT%" required onchange="this.form.submit();"></td></tr>
    </table>
    <br><br>
    <table>
      <tr>
        <td></td>
        <td></td>
        <td>Vorlauf </td>
        <td>Speed </td>
        <td>Dauer </td>
        <td>Font </td>
      </tr>
       <tr>
        <td>Schneefall (multi) </td>
        <td><input type="checkbox" name="snowFallMultiCheckbox" value="checked" %SNOWFALLMULTI_CHECKBOX% onClick="this.form.submit();"></td>
        <td><input name="snowFallMultiLeadtime" type="number" min="0" max="300" value="%SNOWFALLMULTI_LEADTIME%"></td>
        <td><input name="snowFallMultiDelay" type="number" min="5" max="1000" value=%SNOWFALLMULTI_DELAY%></td>
        <td><input name="snowFallMultiDuration" type="number" min="0" max="300" value="%SNOWFALLMULTI_DURATION%"></td>
      </tr>
      <tr>
        <td>Wetter </td>
        <td><input type="checkbox" name="weatherCheckbox" value="checked" %WEATHER_CHECKBOX% onClick="this.form.submit();"></td>
        <td><input name="weatherLeadtime" type="number" min="0" max="300" value="%WEATHER_LEADTIME%"></td>
        <td><input name="weatherDelay" type="number" min="5" max="1000" value=%WEATHER_DELAY%></td>
        <td></td>
        <td><select name="weatherFont" onchange="this.form.submit();">%WEATHER_FONT%</select></td>
      </tr>
      <tr>
        <td>Sternenhimmel </td>
        <td><input type="checkbox" name="starrySkyCheckbox" value="checked" %STARRYSKY_CHECKBOX% onClick="this.form.submit();"></td>
        <td><input name="starrySkyLeadtime" type="number" min="0" max="300" value="%STARRYSKY_LEADTIME%"></td>
        <td><input name="starrySkyDelay" type="number" min="5" max="1000" value=%STARRYSKY_DELAY%></td>
        <td><input name="starrySkyDuration" type="number" min="0" max="300" value="%STARRYSKY_DURATION%"></td>
      </tr>
      <tr>
        <td>Text 1 </td>
        <td><input type="checkbox" name="scrollText1Checkbox" value="checked" %SCROLLTEXT1_CHECKBOX% onClick="this.form.submit();"></td>
        <td><input name="scrollText1Leadtime" type="number" min="0" max="300" value="%SCROLLTEXT1_LEADTIME%"></td>
        <td><input name="scrollText1Delay" type="number" min="5" max="1000" value=%SCROLLTEXT1_DELAY%></td>
        <td></td>
        <td><select name="scrollText1Font" onchange="this.form.submit();">%SCROLLTEXT1_FONT%</select></td>
      </tr>
      <tr>
        <td>Symbole </td>
        <td><input type="checkbox" name="christmasSymbolsCheckbox" value="checked" %CHRISTMASSYMBOLS_CHECKBOX% onClick="this.form.submit();"></td>
        <td><input name="christmasSymbolsLeadtime" type="number" min="0" max="300" value="%CHRISTMASSYMBOLS_LEADTIME%"></td>
        <td><input name="christmasSymbolsDelay" type="number" min="5" max="1000" value=%CHRISTMASSYMBOLS_DELAY%></td>
        <td><input name="christmasSymbolsDuration" type="number" min="0" max="300" value="%CHRISTMASSYMBOLS_DURATION%"></td>
      </tr>
      <tr>
        <td>Text 2 </td>
        <td><input type="checkbox" name="scrollText2Checkbox" value="checked" %SCROLLTEXT2_CHECKBOX% onClick="this.form.submit();"></td>
        <td><input name="scrollText2Leadtime" type="number" min="0" max="300" value="%SCROLLTEXT2_LEADTIME%"></td>
        <td><input name="scrollText2Delay" type="number" min="5" max="1000" value=%SCROLLTEXT2_DELAY%></td>
        <td></td>
        <td><select name="scrollText2Font" onchange="this.form.submit();">%SCROLLTEXT2_FONT%</select></td>
      </tr>
      <tr>
        <td>Schneefall (Pixel) </td>
        <td><input type="checkbox" name="pixelFallCheckbox" value="checked" %PIXELFALL_CHECKBOX% onClick="this.form.submit();"></td>
        <td><input name="pixelFallLeadtime" type="number" min="0" max="300" value="%PIXELFALL_LEADTIME%"></td>
        <td><input name="pixelFallDelay" type="number" min="5" max="1000" value=%PIXELFALL_DELAY%></td>
        <td><input name="pixelFallDuration" type="number" min="0" max="300" value="%PIXELFALL_DURATION%"></td>
      </tr>
      <tr>
        <td>Wachsende Schneeflocke </td>
        <td><input type="checkbox" name="growingStarCheckbox" value="checked" %GROWINGSTAR_CHECKBOX% onClick="this.form.submit();"></td>
        <td><input name="growingStarLeadtime" type="number" min="0" max="300" value="%GROWINGSTAR_LEADTIME%"></td>
        <td><input name="growingStarDelay" type="number" min="5" max="1000" value=%GROWINGSTAR_DELAY%></td>
        <td><input name="growingStarDuration" type="number" min="0" max="300" value="%GROWINGSTAR_DURATION%"></td>
      </tr>
      <tr>
        <td>Text 3 </td>
        <td><input type="checkbox" name="scrollText3Checkbox" value="checked" %SCROLLTEXT3_CHECKBOX% onClick="this.form.submit();"></td>
        <td><input name="scrollText3Leadtime" type="number" min="0" max="300" value="%SCROLLTEXT3_LEADTIME%"></td>
        <td><input name="scrollText3Delay" type="number" min="5" max="1000" value=%SCROLLTEXT3_DELAY%></td>
        <td></td>
        <td><select name="scrollText3Font" onchange="this.form.submit();">%SCROLLTEXT3_FONT%</select></td>
      </tr>
     <tr>
        <td>Schneefall (single) </td>
        <td><input type="checkbox" name="snowFallSingleCheckbox" value="checked" %SNOWFALLSINGLE_CHECKBOX% onClick="this.form.submit();"></td>
        <td><input name="snowFallSingleLeadtime" type="number" min="0" max="300" value="%SNOWFALLSINGLE_LEADTIME%"></td>
        <td><input name="snowFallSingleDelay" type="number" min="5" max="1000" value=%SNOWFALLSINGLE_DELAY%></td>
        <td><input name="snowFallSingleDuration" type="number" min="0" max="300" value="%SNOWFALLSINGLE_DURATION%"></td>
      </tr>
    </table>
    <br><br>
    <table>
    <tr><td>Scrollrichtung umkehren</td><td><input type="checkbox" name="mirrorCheckbox" value="checked" %MIRROR_CHECKBOX% onClick="this.form.submit();"></td></tr>
    <tr><td>Display invertieren</td><td><input type="checkbox" name="reverseCheckbox" value="checked" %REVERSE_CHECKBOX% onClick="this.form.submit();"></td></tr>
    <tr><td>City-ID</td><td><input name="cityID" type="text" size="7" value="%CITYID%">&nbsp;%WEATHERLOCATION%</td></tr>
    <tr><td>Anzahl Sterne</td><td><input name="starrySkyStarCount" type="number" min="1" max="1000" value=%STARRYSKY_STARCOUNT%></td></tr>
    <tr><td>Datum</td><td><input type="checkbox" name="dateCheckbox" value="checked" %DATE_CHECKBOX% onClick="this.form.submit();"></td></tr>
    <tr><td>Temperatur</td><td><input type="checkbox" name="tempCheckbox" value="checked" %TEMP_CHECKBOX% onClick="this.form.submit();"></td></tr>
    <tr><td>Regen</td><td><input type="checkbox" name="rainCheckbox" value="checked" %RAIN_CHECKBOX% onClick="this.form.submit();"></td></tr>
    <tr><td>Wind</td><td><input type="checkbox" name="windCheckbox" value="checked" %WIND_CHECKBOX% onClick="this.form.submit();"></td></tr>
    <tr><td>Luftfeuchte</td><td><input type="checkbox" name="humidityCheckbox" value="checked" %HUMIDITY_CHECKBOX% onClick="this.form.submit();"></td></tr>
    <tr><td>Luftdruck</td><td><input type="checkbox" name="pressureCheckbox" value="checked" %PRESSURE_CHECKBOX% onClick="this.form.submit();"></td></tr>
    <tr><td>Ort</td><td><input type="checkbox" name="weatherLocationCheckbox" value="checked" %WEATHERLOCATION_CHECKBOX% onClick="this.form.submit();"></td></tr>
    <tr><td>Uhr animieren</td><td><input type="checkbox" name="animClockCheckbox" value="checked" %ANIMCLOCK_CHECKBOX% onClick="this.form.submit();"></td></tr>
    <tr><td>Blinkende Doppelpunkte</td><td><input type="checkbox" name="colonsCheckbox" value="checked" %COLONS_CHECKBOX% onClick="this.form.submit();"></td></tr>
    </table>
    <br><br>
    <a href="/ota">Firmwareupdate</a>
    <br><br>
    <input type="submit" value="speichern">
  </form>
</body></html>)rawliteral";
