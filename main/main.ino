

/* ESP-SCC-CUL
 *  Adapter um mehrere CULs per SoftwareSerial im Wifi bereitzustellen 
 *  
 */
#include <Wire.h>
#include <ESP8266WiFi.h>

//needed for WifiManager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>  

//Wifi
const int ServerPort  = 23;

//CUL Interface
#define I2CFREQ         400000
#define CULCOUNT        3
const char CULADDR[CULCOUNT] = {0x7e, 0x7d, 0x7c};
#define MAXMSGSIZE      128 // Maxsize of a CUL Message
#define I2CBYTECOUNT    8  // Get this amount of Bytes at each read request

#define CUL_BOOTLOADER_WAIT 10 //Sec. to wait after Releasing reset for CUL Bootloader to process

#define LED_PIN         2  //WEMOS D4 Buildin LED/Pull-Up
#define LED_OFF         digitalWrite(LED_PIN,HIGH)
#define LED_ON          digitalWrite(LED_PIN,LOW)

#define BUTTON_PIN      0  //WEMOS D3 Pull-Up

#define RESET_PIN       16  //WEMOS D0
#define RESET_HOLD      digitalWrite(RESET_PIN,HIGH)
#define RESET_RELEASE   digitalWrite(RESET_PIN,LOW)


//WifiServer
WiFiServer TCPServer(ServerPort);
WiFiClient TCPClient;
// Buffer für CUL Messages
char SocketToI2C[MAXMSGSIZE];
int SocketToI2CCount = 0 ;
char I2CToSocket[CULCOUNT][MAXMSGSIZE];
int I2CToSocketCount[CULCOUNT];

bool hasTCPClient = false;

void setup() {
  // Debug Console auf HW Serial 
  Serial.begin(115200);

  //General Info

  Serial.println("ESP-Multi-I2CCUL-Bridge by B.Stark");
  Serial.println("Version: Proof of Concept");
  Serial.print("ESP ChipID: ");
  Serial.println(ESP.getChipId(),HEX);
  Serial.print("ESP FreeHeap: ");
  Serial.println(ESP.getFreeHeap());

  //Hold Reset of CULS low
  pinMode(RESET_PIN,OUTPUT);
  RESET_HOLD;
  Serial.println("Hold CUL in Reset");

  // Button for WifiManager Reset
  pinMode(BUTTON_PIN,INPUT);
  
  Serial.println("Starting WifiManager");
  //WifiManager verbindungen
  WiFiManager wifiManager;
  wifiManager.autoConnect("ESP-I2CCUL-Bridge");
  Serial.println("Connected!");

    //Infos
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Subnet Mask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  //I2C starten
  Wire.begin();
  Wire.setClock(I2CFREQ);
  Serial.print("I2C Freq: ");
  Serial.println(I2CFREQ);
  Serial.print("I2C Devices: ");
  for (int i=0; i<CULCOUNT;i++) 
  { 
    Serial.print(CULADDR[i],HEX);
    Serial.print(", ");
  }
  Serial.println();

  //Server starten
  TCPServer.begin();
  Serial.print("Wait for connection on: ");
  Serial.print(WiFi.localIP());
  Serial.print(":");
  Serial.println(ServerPort);


  //counter auf 0 setzen
  for(int i=0; i < CULCOUNT;i++) I2CToSocketCount[i]=0;

  //Init LED
  ledInit();
 
}

void ledInit() {
  pinMode(LED_PIN,OUTPUT); //LED Ausgang
  LED_OFF;
}

void handleClientConnection() {
  //gibt es einen neuen client
  if (TCPServer.hasClient()) {
    //Wenn noch keiner Verbunden
    if (!TCPClient.connected()){
      TCPClient = TCPServer.available();
      Serial.println("Client Verbunden");
      hasTCPClient = true;
      LED_ON;
      RESET_RELEASE;
      Serial.println("Release CUL Reset"); 
      Serial.print("Wait ");
      Serial.print(CUL_BOOTLOADER_WAIT);
      Serial.println(" Sec. for CUL Bootloader to process");
      delay(CUL_BOOTLOADER_WAIT * 1000);
      Serial.println("Start processing");
    } else { // ansonsten ablehnen
      WiFiClient TCPClientStop = TCPServer.available();
      Serial.println("Verbindung abgewiesen");
    }
  }
  if (!TCPClient.connected() && hasTCPClient) { //hasTCPClient as helper for previously connected Clients
    hasTCPClient = false;
    LED_OFF;
    RESET_HOLD;
    Serial.println("Client disconnected");
    Serial.println("Hold CUL in Reset");
  }
}

void handleSocketData(char data) {
  // Wenn \n dann senden  
  if (data == '\n') {
    //Sende Daten an Serielle Schnittstelle
    // an welche Schnitstelle
    int i=0;
    while (SocketToI2C[i] == '*' && i < CULCOUNT-1)
      i++;
    Serial.print("SocketToI2C: ");
    Serial.print(SocketToI2CCount);
    Serial.print(" Device:");
    Serial.print(CULADDR[i],HEX);
    Serial.print(" Position: ");
    Serial.print(i);
    //Direkt SocketToI2CCount zu modifizieren sollte nicht so speicher intensiv sein
    char Msg[MAXMSGSIZE+2]; // \n und \0
    int j=0;
    for (int pos=i;pos < SocketToI2CCount; pos++) //+1
      Msg[j++]=SocketToI2C[pos];
    Msg[j++]='\n';
    Msg[j]= '\0'; //Ende
    Wire.beginTransmission(CULADDR[i]);
    Wire.write(Msg);
    int error = Wire.endTransmission();
    Serial.print(" Error: ");
    Serial.print(error);
    Serial.print(" Msg: ");
    Serial.print(Msg);
    SocketToI2CCount = 0;
    //Wenn Bufferoverflow
   } else if (SocketToI2CCount == MAXMSGSIZE-1) {
    Serial.println("SocketToI2C Buffer Überlauf");
    SocketToI2CCount = 0;
    //ansonsten weiter mit nächster stelle
   }else{
    SocketToI2C[SocketToI2CCount++] = data;
   }
}

bool handleI2CData(int CulNo, char data) {
    bool returnCULBufferIsEmpty = false;
      if ( data == '\n' ) {
        //senden
        Serial.print("I2CToSocket: ");
        Serial.print(I2CToSocketCount[CulNo]);
        Serial.print(" Device:");
        Serial.print(CULADDR[CulNo],HEX);
        Serial.print(" Position: ");
        Serial.print(CulNo);
        Serial.print(" Msg: ");
        //Buffer zusammen bauen 
        //Direkt den Buffer modifizieren??? nicht so Speicherintensiv
        char Msg[MAXMSGSIZE+2]; // + \n und \0
        int j=0;
        for (int asteriks=0; asteriks<CulNo ;asteriks++) //SCC Sternchen hinzufügen
          Msg[j++]='*';
        for (int pos=0; pos < I2CToSocketCount[CulNo]; pos++) 
          Msg[j++]=I2CToSocket[CulNo][pos];
        Msg[j++]='\n';
        Msg[j]='\0'; //Ende
        TCPClient.print(Msg);
        Serial.print(Msg);
        //Set Buffer pos to 0
        I2CToSocketCount[CulNo] = 0;
      } else if ( data == 0x04) {
        // keine weiteren nachrichten
        //nicht schreiben nicht erhöhen
        returnCULBufferIsEmpty = true;
      } else if (I2CToSocketCount[CulNo] == MAXMSGSIZE-1) {
        Serial.println("I2CToSocket Buffer Überlauf");
        I2CToSocketCount[CulNo] = 0;
      } else {
          // in den Buffer schreiben
          I2CToSocket[CulNo][I2CToSocketCount[CulNo]++] = data;
      }
      return returnCULBufferIsEmpty;
}

void loop() {
  handleClientConnection();
  // Nur wenn Client vorhande
  if (TCPClient.connected()) {
    while (TCPClient.available())
      handleSocketData(TCPClient.read());
    //i2c lesen
    for (int i=0; i < CULCOUNT; i++) {
      //Read 32bit at once - Arduino workaround
      bool culBufferWasEmpty = false;
      while(!culBufferWasEmpty) {  
        Wire.requestFrom(CULADDR[i],I2CBYTECOUNT);
        // Daten holen bis der CUL Buffer leer ist
        if(!Wire.available()) culBufferWasEmpty = true; //Not Empty but nothing received
        while(Wire.available()) {
          bool ret = handleI2CData(i,Wire.read());
          if(ret) culBufferWasEmpty = true;
        }
      } // while(culBufferWasEmpty)
    }//for
   }//if (TCPClient.connected())

  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Reset Setting and restart");
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    ESP.restart();   
  }
}
