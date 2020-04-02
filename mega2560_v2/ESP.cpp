#include "Arduino.h"
#include "ESP.h"

ESP::ESP(byte serial){
  _serial = serial;
}

void ESP::sendMsg(String msg) {
  /*switch(_serial){
    case 0:
    case 1:
    case 2:
    case 3:
  }*/
}
