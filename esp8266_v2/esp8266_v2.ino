/**
  ESP8266 program
  Author: Petr Křehlík
*/

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <ESP8266HTTPClient.h>

#include <WiFiClientSecureBearSSL.h>
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
        if (Serial.available())
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

    String recvStr = "";
    char ch;
    do
    {
      if (Serial.available())
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

        client->setInsecure();
        HTTPClient https;

        //Serial.println(millis());
        if (https.begin(*client, recvStr)) {
          int httpCode = https.GET();

          if (httpCode > 0) {
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
              String payload = https.getString();
              ESP_Send(payload);
            }
          } 

          https.end();
        } 
      }
    }
  }
  delay(100);
}
