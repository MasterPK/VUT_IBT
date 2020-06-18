/*
   Main software for MEGA2560
   Created by Petr Krehlik
*/

/*
   User definitions
*/

//ESP library
#include "ESP.h"

// USER SETTINGS
#define STATION_TOKEN "h17222VeMwugFvl7"
#define MAX_USERS 40
#define IP "https://192.168.1.103"

/*
   End of user definitions
*/

// Includes
#include <SPI.h>
#include <MFRC522.h>
#include "DHT.h"

/*
   Debug section
   Debug can be disabled or enabled with flag below
   Debug messages are always sent to USB
*/
#define DebugPort Serial
#define DEBUG_SERIAL true //false-Disabled, true-Active
#define DEBUG_NEXTION false //false-Disabled, true-Active
#define BAUD_RATE 115200
void printDebug(String message) {
  if (DEBUG_SERIAL) {
    Serial.println(message);
  }
  if (DEBUG_NEXTION) {
    Nextion_Send(message);
  }
}

/*
   END of debug section
*/

/*
   ESP section
*/

#define ESP Serial3 // Serial port where is connected ESP8266/NodeMCU

void ESP_Send(String message) {
  ESP.print(message + "\n");
}

/*
   END of ESP section
*/

/*
   Nextion HMI Section
*/
#define Nextion Serial1

/*
   Send String message to HMI
*/
void Nextion_Send(String message) {
  Nextion.print(message);
  Nextion.write(0xff);
  Nextion.write(0xff);
  Nextion.write(0xff);
  delay(100);
}

/*
   Receive message from HMI if there is one
   Returns recieved message or empty string on error
*/
String Nextion_receive() { //returns generic

  boolean answer = false; // flag
  char bite; // char temp
  String cmd; // string temp
  String result; //final string
  byte countEnd = 0; // end counter
  unsigned long previous; // time of start
  int timeout = 100; // max waiting time in ms
  previous = millis(); // set current time

  do {
    if (Nextion.available() > 0) {
      bite = Nextion.read();
      cmd += bite;
      if ((byte)bite == 0xff) countEnd++;
      if (countEnd == 3) answer = true;
    }
  }
  while (!answer && !((unsigned long)(millis() - previous) >= timeout)); // wait for string terminated by 0xFFFFFF or timeout

  //if
  if (answer)
  {
    for (int i = 1; i < cmd.length(); i++)
    {
      if ((byte)cmd[i] == 0xff)
      {
        break;
      }
      else
      {
        result += cmd[i];
      }
    }
    return result;
  }
  else
  {
    return String();
  }

}


/*
   END of Nextion HMI Section
*/

/*
   JSON
*/

/*
 * Data storage
 */

#include <ArduinoJson.h>
struct user {
  char rfid[9];
  int pin;
  byte perm;
};
struct user users[MAX_USERS];
StaticJsonDocument<2100> doc;

/*
   ESPQueue and data processing section
*/
String read_rfid;

String ESPQueue_Queue[10]; // Array with commnads to be send
int ESPQueue_Type[10] = {false,};
bool init_data = false;
/*
   0=No response expected(Deprecated, DONT USE!), For more see documentation
   1=No use
   2=Generic response expected ("OK","ERR")
   3=Special case (Update of RFIDs)
*/
#define GENERIC_RESPONSE 2
#define RFID_RESPONSE 3

boolean ESPQueue_State = false; //False=Not active, True = Active
int ESPQueue_Count = 0; // Count of waiting commands
unsigned long ESPQueue_Timout = 0; //Temp variable for timout
boolean ESPQueue_ERR = false;

boolean ESPQueue_Add(String command, int type) {

  if (type < 0 || type > 9) {
    return false;
  }
  if (ESPQueue_Count == 10) {
    printDebug("ERR: ESPQueue is full and will be deleted in order to continue! Its critical error and something is really bad!");
    for (int i = 0; i < 20; i++) {
      ESPQueue_Queue[ESPQueue_Count] = "";
      ESPQueue_Type[ESPQueue_Count++] = 0;
    }
    ESPQueue_Count = 0;
    ESPQueue_State = false;
    ESPQueue_ERR = true;
    return false;
  }

  if (ESPQueue_ERR) {
    ESPQueue_Count = 0;
    ESPQueue_State = false;
    for (int i = 0; i < 20; i++) {
      ESPQueue_Queue[ESPQueue_Count] = "";
      ESPQueue_Type[ESPQueue_Count++] = 0;
    }
    ESPQueue_ERR = false;
  }
  ESPQueue_Queue[ESPQueue_Count] = command;
  ESPQueue_Type[ESPQueue_Count++] = type;
  printDebug("ESPQueue_Add: " + command);
  printDebug("ESPQueue_Waiting: " + (String)ESPQueue_Count);
  return true;
}
String recievedString = "";
bool newLineFlag = false;
void ESPQueue_Handle_Err(String msg = "")
{
  ESPQueue_Add(ESPQueue_Queue[0], ESPQueue_Type[0]);
  printDebug("ESPQueue_Done: " + ESPQueue_Queue[0]);
  for (int i = 0; i < ESPQueue_Count; i++) {
    ESPQueue_Queue[i] = ESPQueue_Queue[i + 1];
    ESPQueue_Type[i] = ESPQueue_Type[i + 1];
  }
  ESPQueue_Count--;
  ESPQueue_State = false;
  printDebug("ESPQueue_Waiting: " + (String)ESPQueue_Count);
  recievedString = "";
  delay(5000);
}

void ESPQueue_Handle() {

  if (millis() - ESPQueue_Timout > 10000 && ESPQueue_State) {
    printDebug("ERR: ESPQueue timeout! Retrying...!");
    ESPQueue_Handle_Err();
    return;
  }

  recievedString = "";
  if (ESP.available()) {

    newLineFlag = false;
    
    //String recievedString = ESP.readStringUntil('\n');
    char ch;
    unsigned long a_time, p_time;
    p_time = millis();
    do
    {
      if (ESP.available()) // TODO: Timeout
      {
        ch = ESP.read();
        if (ch == '\n')
        {
          break;
        }
        if (ch == '\r')
        {
          ESP.print('\r');
          ESP.flush();
          continue;
        }
        recievedString += (String)ch;
      }
      else
      {
        a_time = millis();
        if (a_time - p_time > 10000)
        {
          return;
        }
        delay(100);
      }
    } while (true);

    printDebug("RECEIVED ESP: " + recievedString);
    DeserializationError err = deserializeJson(doc, recievedString);
    recievedString = "";
    if (err) {
      printDebug("deserializeJson() failed with code ");
      printDebug(err.c_str());
      ESPQueue_Handle_Err();
      return;
    }
    JsonObject obj = doc.as<JsonObject>();
    if (!checkHash(obj["m"].as<String>(), obj["h"].as<String>()))
    {
      printDebug("Check HASH failed!");
      ESPQueue_Handle_Err();
      return;
    }

    if (ESPQueue_State) {
      /*if (millis() - ESPQueue_Timout > 10000) {
        printDebug("ERR: ESPQueue timeout! Retrying...!");
        ESPQueue_Handle_Err();
        return;

        }*/
      if (ESPQueue_Type[0] == 2) {
        if (obj["m"]["s"].as<String>() == "ok") {
          printDebug("RECEIVED STATUS: OK");
          ESPQueue_State = false;
        } else if (obj["m"]["s"].as<String>() == "err") {
          printDebug("RECEIVED STATUS: ERR");
          printDebug(obj["error"].as<String>());
          ESPQueue_State = false;
        }
      } else if (ESPQueue_Type[0] == 3) {
        if (obj["m"]["s"].as<String>() == "ok")
        {
          for (int i = 0; i < MAX_USERS; i++)
          {
            memcpy(users[i].rfid,0,9);
            users[i].pin=0;
            users[i].perm=0;
          }
          JsonArray usersJson = obj["m"]["u"].as<JsonArray>();
          for (int i = 0; i < obj["m"]["c"].as<int>(); i++)
          {
            
            (usersJson[i][0].as<String>()).toCharArray(users[i].rfid,9);
            users[i].perm=usersJson[i][1].as<int>();
            if(users[i].perm>1)
            {
              users[i].pin=usersJson[i][2].as<int>();
            }
          }
          init_data = true;
        }
        ESPQueue_State = false;
      }
      if (ESPQueue_State == false) {
        printDebug("ESPQueue_Done: " + ESPQueue_Queue[0]);
        for (int i = 0; i < ESPQueue_Count; i++) {
          ESPQueue_Queue[i] = ESPQueue_Queue[i + 1];
          ESPQueue_Type[i] = ESPQueue_Type[i + 1];
        }
        ESPQueue_Count--;
        printDebug("ESPQueue_Waiting: " + (String)ESPQueue_Count);
      }
    }
    recievedString = "";
  }
  if (ESPQueue_State == false && ESPQueue_Count != 0) {

    printDebug("ESPQueue_Processing: " + ESPQueue_Queue[0]);
    printDebug("ESP REQUEST:" + (String)IP + ESPQueue_Queue[0] + "&token=" + STATION_TOKEN);
    ESP_Send((String)IP + ESPQueue_Queue[0] + "&token=" + STATION_TOKEN);
    ESPQueue_Timout = millis();
    ESPQueue_State = true;
  }

}



/*
   END of ESPQueue section
*/


/*
   MFRC522
*/
#define SS_PIN 10 // RC522 SS_PIN
#define RST_PIN 9 // RC522 RST_PIN
// SCK  D52
// MOSI D51
// MISO D50

MFRC522 mfrc522(SS_PIN, RST_PIN);
void dump_byte_array(byte *buffer, byte bufferSize)
{
  read_rfid = "";
  for (byte i = 0; i < bufferSize; i++)
  {
    read_rfid = read_rfid + String(buffer[i], HEX);
  }
}

/*
   Check codes:
   0: No access
   1: Normal access without two-way
   2: Normal access with two-way
   3: Admin mode with two-way
*/
String pin;
int CheckRFID()
{
  for(int i = 0 ;i<MAX_USERS;i++)
  {
    if(strcmp(users[i].rfid,read_rfid.c_str())==0){
      if(users[i].perm == 2 || users[i].perm == 3){
        pin=users[i].pin;
      }
      return users[i].perm;
    }
  }
  return 0;
}

/*
   DHT11 thermometer
*/
#define DHTTYPE DHT11
#define DHTPIN 2
DHT dht(DHTPIN, DHTTYPE);


float getTemperature()
{
  float t = dht.readTemperature();
  if (isnan(t)) {
    printDebug("ERR: Failed to read from DHT sensor!");
    return -1;
  }
  else
  {
    return t;
  }
  Nextion_Send("temp.txt=\"" + (String)t + "\"");
}

void updateTemperatureHMI()
{
  int temp;
  if ((temp = getTemperature()) >= 0)
  {
    Nextion_Send("intro.tempVar.txt=\"" + (String)temp + "\"");
    Nextion_Send("temp.txt=\"" + (String)temp + "\"");
  }

}
/*
   EEPROM
   Pointers:
   0:Bell activation
   1-4:Relay activation

   Realy settings:
   0: Deactivated
   1: Activated to open doors
   2: Activated to bell


   IMPORTANT!!!
   EEPROM have to be initialized to zeros before first use of this program!
*/
#include <EEPROM.h>
#define EEPROM_BELL 0
#define EEPROM_R1 1
#define EEPROM_R2 2
#define EEPROM_R3 3
#define EEPROM_R4 4
int bell, r1, r2, r3, r4;

/*
   Relay System
*/

#define RELAY1 3
#define RELAY2 4
#define RELAY3 5
#define RELAY4 6

void OpenDoors()
{
  printDebug("OPEN_DOORS");
  if (r1 == 1)
  {
    digitalWrite(RELAY1, LOW);
  }
  if (r2 == 1)
  {
    digitalWrite(RELAY2, LOW);
  }
  if (r3 == 1)
  {
    digitalWrite(RELAY3, LOW);
  }
  if (r4 == 1)
  {
    digitalWrite(RELAY4, LOW);
  }
  delay(1000);
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  digitalWrite(RELAY3, HIGH);
  digitalWrite(RELAY4, HIGH);
}

void zvonek()
{
  printDebug("BELL_RINGING");
  if (r1 == 2)
  {
    digitalWrite(RELAY1, LOW);
  }
  if (r2 == 2)
  {
    digitalWrite(RELAY2, LOW);
  }
  if (r3 == 2)
  {
    digitalWrite(RELAY3, LOW);
  }
  if (r4 == 2)
  {
    digitalWrite(RELAY4, LOW);
  }
  delay(1000);
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  digitalWrite(RELAY3, HIGH);
  digitalWrite(RELAY4, HIGH);
}

/*
   Show BELL if atleast one relay is set to BELL
*/

void bellSetup()
{

  if (r1 == 2 || r2 == 2 || r3 == 2 || r4 == 2)
  {
    //Nextion_Send("vis i_wireless,0");
    //Nextion_Send("vis btn_zvonek,1");
    Nextion_Send("intro.bell.val=1");
  }
  else
  {
    Nextion_Send("intro.bell.val=0");
  }
}

/*
   MD5
*/
#include <MD5.h>

bool checkHash(String string, String hash)
{
  unsigned char tmp[128];
  string.toCharArray(tmp, 128);
  unsigned char* new_hash = MD5::make_hash(tmp);
  char *md5str = MD5::make_digest(new_hash, 16);
  if ((String)md5str == hash)
  {
    free(new_hash);
    free(md5str);
    return true;
  }
  else
  {
    free(new_hash);
    free(md5str);
    return false;
  }

}

void NextionHandle()
{
  String txt = Nextion_receive();
  if (txt == "")
  {
    return;
  }

  if (txt == "R1")
  {
    if (r1 == 0)
    {
      EEPROM.update(EEPROM_R1, 1);
      r1 = 1;
      Nextion_Send("b1.txt=\"DVERE\"");
    }
    else if (r1 == 1)
    {
      EEPROM.update(EEPROM_R1, 2);
      r1 = 2;
      Nextion_Send("b1.txt=\"ZVONEK\"");
    }
    else if (r1 == 2)
    {
      EEPROM.update(EEPROM_R1, 0);
      r1 = 0;
      Nextion_Send("b1.txt=\"-\"");
    }
  }
  else if (txt == "R2")
  {
    if (r2 == 0)
    {
      EEPROM.update(EEPROM_R2, 1);
      r2 = 1;
      Nextion_Send("b2.txt=\"DVERE\"");
    }
    else if (r2 == 1)
    {
      EEPROM.update(EEPROM_R2, 2);
      r2 = 2;
      Nextion_Send("b2.txt=\"ZVONEK\"");
    }
    else if (r2 == 2)
    {
      EEPROM.update(EEPROM_R2, 0);
      r2 = 0;
      Nextion_Send("b2.txt=\"-\"");
    }
  }
  else if (txt == "R3")
  {
    if (r3 == 0)
    {
      EEPROM.update(EEPROM_R3, 1);
      r3 = 1;
      Nextion_Send("b3.txt=\"DVERE\"");
    }
    else if (r3 == 1)
    {
      EEPROM.update(EEPROM_R3, 2);
      r3 = 2;
      Nextion_Send("b3.txt=\"ZVONEK\"");
    }
    else if (r3 == 2)
    {
      EEPROM.update(EEPROM_R3, 0);
      r3 = 0;
      Nextion_Send("b3.txt=\"-\"");
    }
  }
  else if (txt == "R4")
  {
    if (r4 == 0)
    {
      EEPROM.update(EEPROM_R4, 1);
      r4 = 1;
      Nextion_Send("b4.txt=\"DVERE\"");
    }
    else if (r4 == 1)
    {
      EEPROM.update(EEPROM_R4, 2);
      r4 = 2;
      Nextion_Send("b4.txt=\"ZVONEK\"");
    }
    else if (r4 == 2)
    {
      EEPROM.update(EEPROM_R4, 0);
      r4 = 0;
      Nextion_Send("b4.txt=\"-\"");
    }
  }
  else if (txt == "A0")
  {
    //Nextion_Send("page loading");
    //Nextion_Send("b1.txt=\"...\"");
    //Nextion_Send("tsw b1,0");
    ESPQueue_Add("/api/get-users?", RFID_RESPONSE);
  }
  else if (txt == "A1")
  {

    if (r1 == 0)
    {
      Nextion_Send("b1.txt=\"-\"");
    }
    else if (r1 == 1)
    {
      Nextion_Send("b1.txt=\"DVERE\"");
    }
    else if (r1 == 2)
    {
      Nextion_Send("b1.txt=\"ZVONEK\"");
    }

    if (r2 == 0)
    {
      Nextion_Send("b2.txt=\"-\"");
    }
    else if (r2 == 1)
    {
      Nextion_Send("b2.txt=\"DVERE\"");
    }
    else if (r2 == 2)
    {
      Nextion_Send("b2.txt=\"ZVONEK\"");
    }

    if (r3 == 0)
    {
      Nextion_Send("b3.txt=\"-\"");
    }
    else if (r3 == 1)
    {
      Nextion_Send("b3.txt=\"DVERE\"");
    }
    else if (r3 == 2)
    {
      Nextion_Send("b3.txt=\"ZVONEK\"");
    }

    if (r4 == 0)
    {
      Nextion_Send("b4.txt=\"-\"");
    }
    else if (r4 == 1)
    {
      Nextion_Send("b4.txt=\"DVERE\"");
    }
    else if (r4 == 2)
    {
      Nextion_Send("b4.txt=\"ZVONEK\"");
    }
  }
  else if (txt == "A2")
  {
    delay(200);
    bellSetup();
    Nextion_Send("page intro");

    return;
  }
  else if (txt == "A3")
  {
    Nextion_Send("page register_rfid");
    String txt = Nextion_receive();
    while (true)
    {

      if ( mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
        printDebug("RFID READ: " + read_rfid);
        mfrc522.PICC_HaltA();
        Nextion_Send("newRfid.txt=\"" + read_rfid + "\"");
      }

      if (txt == "")
      {
        txt = Nextion_receive();
        continue;
      }

      if (txt == "R1")
      {
        if (read_rfid != "")
        {
          ESPQueue_Add((String)"/api/add-new-rfid/?" + "&rfid=" + read_rfid, GENERIC_RESPONSE);
        }
        Nextion_Send("page admin_menu");
        break;
      }

      if (txt == "R2")
      {
        Nextion_Send("page admin_menu");
        break;
      }

      txt = Nextion_receive();
    }


  }
  else if (txt == "A4")
  {
    OpenDoors();
  }
  else if (txt == "Z")
  {
    zvonek();
  }
  else if (txt == "FF")
  {
    Nextion_Send("page intro");
  }
  bellSetup();
}

void setup() {

  if (DEBUG_SERIAL) {
    DebugPort.begin(BAUD_RATE);
  }

  bell = EEPROM.read(EEPROM_BELL);
  r1 = EEPROM.read(EEPROM_R1);
  r2 = EEPROM.read(EEPROM_R2);
  r3 = EEPROM.read(EEPROM_R3);
  r4 = EEPROM.read(EEPROM_R4);

  // Relay init
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  digitalWrite(RELAY3, HIGH);
  digitalWrite(RELAY4, HIGH);

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);

  printDebug("LOADING SYSTEM...PLEASE WAIT");

  Nextion.begin(9600);
  printDebug("HMI SETUP: OK");
  Nextion_Send("rest");



  //Connection test with ESP
  ESP.begin(BAUD_RATE);

  String input = "";
  ESP_Send("C");

  unsigned long a_time, p_time;
  int i = 0;
  p_time = millis();
  while (input != "OK") {
    a_time = millis();
    if (a_time - p_time > 20000)
    {
      p_time = a_time;
      ESP_Send("C");
    }
    if (ESP.available() > 0) {
      input = ESP.readStringUntil('\n');
    }
  }

  printDebug("ESP SETUP: OK");


  ESPQueue_Add("/api/get-users?", RFID_RESPONSE);

  while (init_data == false) {
    ESPQueue_Handle();
    delay(10);
  }

  printDebug("DATA SETUP: OK");



  SPI.begin();
  mfrc522.PCD_Init();
  printDebug("RC522 SETUP: OK");

  dht.begin();
  int temp;
  if ((temp = getTemperature()) >= 0)
  {
    ESPQueue_Add("/api/save-temp?id_temp_sensor=1&temp=" + (String)temp, GENERIC_RESPONSE);
  }
  printDebug("DHT11 SETUP: OK");

  bellSetup();
  Nextion_Send("page intro");

  updateTemperatureHMI();
  printDebug("SYSTEM READY!");
}

unsigned long current_time, prev_update_time, prev_temperature_time, prev_temperatureDatabase_time;



void loop()
{

  // Main Handle of Queue
  ESPQueue_Handle();

  // Nextion HMI Handle
  NextionHandle();

  // Time procedures
  current_time = millis();

  // Users update
  if (current_time - prev_update_time > 600000) {

    prev_update_time = current_time;
    ESPQueue_Add("/api/get-users?", RFID_RESPONSE);
  };

  //Temp update on Nextion
  if (current_time - prev_temperature_time > 5000) {
    updateTemperatureHMI();
    prev_temperature_time = current_time;
  };

  //Temp update in database
  if (current_time - prev_temperatureDatabase_time > 3600000) {
    int temp;
    if ((temp = getTemperature()) >= 0)
    {
      ESPQueue_Add("/api/save-temp?id_temp_sensor=1&temp=" + (String)temp, GENERIC_RESPONSE);
    }
    prev_temperatureDatabase_time = current_time;
  };

  // New card reading
  //dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
  printDebug("RFID READ: " + read_rfid);
  mfrc522.PICC_HaltA();

  int check = CheckRFID();
  if (check == 3)
  {
    printDebug("ACCESS: ADMIN");
    Nextion_Send("page keyboard");
    String txt;
    unsigned long prev_time = millis();

    do
    {
      ESPQueue_Handle();
      current_time = millis();
      if ((current_time - prev_time) > 20000)
      {
        ESPQueue_Add((String)"/api/save-access/?" + "&user_rfid=" + read_rfid + "&status=" + (String)0, GENERIC_RESPONSE);
        Nextion_Send("page intro");
        return;
      }
      txt = Nextion_receive();

    }
    while (txt == "");

    if (pin == txt)
    {
      ESPQueue_Add((String)"/api/save-access/?" + "&user_rfid=" + read_rfid + "&status=" + (String)1, GENERIC_RESPONSE);
      Nextion_Send("page admin_menu");
    }
    else
    {
      ESPQueue_Add((String)"/api/save-access/?" + "&user_rfid=" + read_rfid + "&status=" + (String)0, GENERIC_RESPONSE);
      Nextion_Send("page access_no");
    }


  }
  else if (check == 2)
  {
    printDebug("ACCESS: 2W");
    Nextion_Send("page keyboard");
    String txt;
    unsigned long prev_time = millis();

    do
    {
      ESPQueue_Handle();
      current_time = millis();
      if ((current_time - prev_time) > 20000)
      {
        ESPQueue_Add((String)"/api/save-access/?" + "&user_rfid=" + read_rfid + "&status=" + (String)0, GENERIC_RESPONSE);
        Nextion_Send("page intro");
        return;
      }
      txt = Nextion_receive();
    }
    while (txt == "");
    if (pin == txt)
    {
      ESPQueue_Add((String)"/api/save-access/?" + "&user_rfid=" + read_rfid + "&status=" + (String)1, GENERIC_RESPONSE);
      Nextion_Send("page access_yes");
      OpenDoors();
    }
    else
    {
      ESPQueue_Add((String)"/api/save-access/?" + "&user_rfid=" + read_rfid + "&status=" + (String)0, GENERIC_RESPONSE);
      Nextion_Send("page access_no");
    }
  }
  else if (check == 1)
  {
    printDebug("ACCESS: YES");
    ESPQueue_Add((String)"/api/save-access/?" + "&user_rfid=" + read_rfid + "&status=" + (String)check, GENERIC_RESPONSE);
    Nextion_Send("page access_yes");
    OpenDoors();
  }
  else
  {
    printDebug("ACCESS: NO");
    ESPQueue_Add((String)"/api/save-access/?" + "&user_rfid=" + read_rfid + "&status=" + (String)check, GENERIC_RESPONSE);
    Nextion_Send("page access_no");
    delay(1000);
  }


}
