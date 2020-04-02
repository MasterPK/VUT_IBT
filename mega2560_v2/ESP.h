/*
 * ESP handle library for Arduino MEGA2560 and ESP8266
 * Created by Petr Krehlik © 2020
 */

#ifndef ESP_h
#define ESP_h

#include "Arduino.h"
class ESP
{
private:
    byte _serial;
    void sendMsg(String msg);
    String receiveMsg();
public:
    ESP(byte serial);
    ~ESP();
    bool add(String command, int type);
    void handle();
};


#endif
