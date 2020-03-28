/**
   BasicHTTPSClient.ino

    Created on: 20.08.2018

*/

#include <Arduino.h>

#include <ESP32WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>

#include <WiFiClientSecureBearSSL.h>
// Fingerprint for demo URL, expires on June 2, 2021, needs to be updated well before this date
const uint8_t fingerprint[20] = {0xb6, 0x16, 0xcd, 0xfa, 0x31, 0x32, 0x00, 0xa6, 0xd1, 0x71, 0x8a, 0x62, 0x9f, 0x57, 0xbc, 0x7e, 0x0a, 0x2d, 0xe2, 0x5f};
//"B6:16:CD:FA:31:32:00:A6:D1:71:8A:62:9F:57:BC:7E:0A:2D:E2:5B"
ESP8266WiFiMulti WiFiMulti;

void ESP_Send(String message)
{
  int counter = 0;
  for (int i = 0; i < message.length(); i++)
  {
    if (counter == 60)
    {
      counter = 0;
      Serial.print('\r');
      Serial.flush();
      char ch;
      do
      {
        if (Serial.available()) // TODO: Timeout
        {
          ch = Serial.read();
          if (ch == '\r')
          {
            break;
          }
        }
        else
        {
          delay(100);
        }
      } while (true);
    }
    Serial.print(message[i]);
    Serial.flush();
    counter++;
  }
  Serial.print('\n');
  Serial.flush();

}

void setup() {

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  delay(100);

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("Krehlikovi", "axago240");
  WiFiMulti.addAP("KRE_ESP", "1234test");

  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(1000);
  }

}

void loop() {
  // wait for WiFi connection
  if (Serial.available()) {

    String recvStr = ""; //= Serial.readStringUntil('\n');
    char ch;
    do
    {
      if (Serial.available()) // TODO: Timeout
      {
        ch = Serial.read();
        if (ch == '\n')
        {
          break;
        }
        recvStr += (String)ch;
      }
      else
      {
        delay(100);
      }
    } while (true);

    if (recvStr == "C") {
      ESP_Send("OK");
    } else {
      if ((WiFiMulti.run() == WL_CONNECTED)) {
        std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

        //client->setFingerprint(fingerprint);
        client->setInsecure();
        HTTPClient https;

        //Serial.println(millis());
        if (https.begin(*client, recvStr)) {  // HTTPS


          // start connection and send HTTP header
          int httpCode = https.GET();

          // httpCode will be negative on error
          if (httpCode > 0) {
            // HTTP header has been send and Server response header has been handled

            // file found at server
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
              String payload = https.getString();
              ESP_Send(payload);
            }
          } else {
            //Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
          }

          https.end();
        } else {
          //Serial.printf("[HTTPS] Unable to connect\n");
        }
      }
    }
  }
  //Serial.println("Wait 10s before next round...");
  delay(100);
}
