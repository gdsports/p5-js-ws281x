/****************************************************************************
MIT License

Copyright (c) 2017 gdsports625@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
****************************************************************************/

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
ESP8266WebServer Webserver(80);
#include <ESP8266mDNS.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
WebServer Webserver(80);
#include <ESPmDNS.h>
#else
#error This works only on ESP8266 or ESP32
#endif
#include <WebSocketsServer.h>
#include <Hash.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>
#include <NeoPixelBus.h>

const uint16_t PixelCount = 128;

// Uart method is good for the Esp-01 or other pin restricted modules
// NOTE: These will ignore the PIN and use GPI02 pin
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart800KbpsMethod> strip(PixelCount);

#include "index_html.h"

WebSocketsServer webSocket = WebSocketsServer(81);

const char DEVICE_NAME[] = "p510print";

void handleRoot() {
  Webserver.send_P(200, "text/html", INDEX_HTML);
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += Webserver.uri();
  message += "\nMethod: ";
  message += (Webserver.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += Webserver.args();
  message += "\n";
  for (uint8_t i=0; i<Webserver.args(); i++){
    message += " " + Webserver.argName(i) + ": " + Webserver.arg(i) + "\n";
  }
  Webserver.send(404, "text/plain", message);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  static uint32_t lastMillis = 0;
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\r\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      }
      break;
    case WStype_TEXT:
      Serial.printf("%u length\r\n", length);
      {
        const size_t bufferSize = JSON_ARRAY_SIZE(192) + JSON_OBJECT_SIZE(2) + 490;
        DynamicJsonBuffer jsonBuffer(bufferSize);
        JsonObject& root = jsonBuffer.parseObject((char *)payload);

        // Test if parsing succeeds.
        if (!root.success()) {
          Serial.println("parseObject() failed");
        }
        else {
          int nLEDs = root["nLEDs"];
          JsonArray& rgb = root["rgb"];
          Serial.printf("nLEDs %d\r\n", nLEDs);
          for (int i = 0; i < nLEDs*3; i += 3) {
            strip.SetPixelColor(i/3, RgbColor(rgb[i], rgb[i+1], rgb[i+2]));
          }
          strip.Show();
        }
      }
      break;

    case WStype_BIN:
      Serial.printf("[%u] get binary length: %u\r\n", num, length);
#ifdef ESP8266
      hexdump(payload, length);
#endif
      break;
    default:
      Serial.printf("Invalid WStype [%d]\r\n", type);
      break;
  }
}

/**************************************************************************/
/*
    Arduino setup function (automatically called at startup)
*/
/**************************************************************************/
void webserver_setup()
{
  Webserver.on("/", handleRoot);

  Webserver.onNotFound(handleNotFound);

  Webserver.begin();
  Serial.println(F("HTTP server started"));

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println(F("WebSocket server started"));
}

void ledgrid_setup()
{
  // this resets all the neopixels to an off state
  strip.Begin();
  strip.Show();
}

void setup()
{
  Serial.begin(115200);
  Serial.println(F("\np5 LED 10print setup"));

  ledgrid_setup();

  // IOS and MacOS browsers may connect to p5button-a.local.
  // Android browsers may also work.
  // Windows and Linux probably need to know the IP address
  // of the ESP8266 (sigh). MDNS is also known as Bonjour
  // or zero-conf.
  if (MDNS.begin(DEVICE_NAME)) {
    Serial.print(F("MDNS responder started. Connect to "));
    Serial.print(DEVICE_NAME);
    Serial.println(F(".local"));
    // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("websock", "tcp", 81);
  }

  webserver_setup();
}

void loop()
{
  static bool Connected = false;

  if (WiFi.status() == WL_CONNECTED) {
    if (!Connected) {
      Serial.print(F("WiFi connected! IP address: "));
      Serial.println(WiFi.localIP());
      Connected = true;
    }
  }
  else {
    if (Connected) {
      Serial.println(F("WiFi not connected!"));
      Connected = false;
    }

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    //reset saved settings
    //wifiManager.resetSettings();

    //fetches ssid and pass from Flash and tries to connect
    //if it does not connect it starts an access point with the specified name
    //and goes into a blocking loop awaiting configuration
    wifiManager.autoConnect(DEVICE_NAME);

    Serial.print(F("WiFi connected! IP address: "));
    Serial.println(WiFi.localIP());
    Connected = true;
  }

  webSocket.loop();
  Webserver.handleClient();
}
