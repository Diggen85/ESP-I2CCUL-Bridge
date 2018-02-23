

/* ESP-SCC-CUL
 *  Adapter um mehrere CULs über I2C an einem ESP im Wifi bereitzustellen
 *  
 *  FHEM Definition
 *      define CUL_ESP CUL X.X.X.X:23 0000
 *      define CUL_ESP2 STACKABLE_CC CUL_ESP
 *      define CUL_ESP3 STACKABLE_CC CUL_ESP2
 *      define CUL_ESP4 STACKABLE_CC CUL_ESP3
 *  
 *  - Feb. 2018 v0.1
 *              Fix für I2CCUL - TWI offline beim Senden
 *              Wifi Reset Button fix - Wifi Reset sollte bei neustart nun nicht mehr passieren
 *              Code Cleanups 
 */
#include <Wire.h>
#include <ESP8266WiFi.h>

//needed for WifiManager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>  

//Wifi
const int ServerPort  = 23;
#define WIFINAME        "ESP-I2CCUL-Bridge"

//CUL Interface
#define I2CFREQ         400000
#define CULCOUNT        4
const char CULADDR[CULCOUNT] = {0x70, 0x71, 0x72, 0x73};
//const char CULADDR[CULCOUNT] = {0x70, 0x71};
#define MAXMSGSIZE      256 // Maxsize of a CUL Message 128 is CUL Ringbuffersize
#define I2CBYTECOUNT    8  // Get this amount of Bytes at each read request
#define MSGWAITTIME     300  //ms to wait after sending msg over I2C 

#define LED_PIN         2  //WEMOS D4 Buildin LED/Pull-Up - GPIO2
#define LED_OFF         digitalWrite(LED_PIN,HIGH)
#define LED_ON          digitalWrite(LED_PIN,LOW)

#define BUTTON_PIN      0  //WEMOS D3 Pull-Up - GPIO0


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

  // Button for WifiManager Reset
  pinMode(BUTTON_PIN,INPUT);
  //counter auf 0 setzen
  for(int i=0; i < CULCOUNT;i++) 
    I2CToSocketCount[i]=0;
  //Init LED
  ledInit();

  //General Info
  Serial.println();
  Serial.println("ESP-Multi-I2CCUL-Bridge by Diggen85 (B.Stark)");
  Serial.println("Version: V0.1");
  Serial.print("ESP ChipID: ");
  Serial.println(ESP.getChipId(),HEX);
  Serial.print("ESP FreeHeap: ");
  Serial.println(ESP.getFreeHeap());

  
  // Wifi starten
  
  //WifiManager verbindungen
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);
  wifiManager.setAPCallback(callbackAP);
  wifiManager.autoConnect(WIFINAME);
  
  // Wifi Infos 
  Serial.println("Wifi Connected!");
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
  // I2C Info
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
  //Server Info
  Serial.print("Wait for connection on: ");
  Serial.print(WiFi.localIP());
  Serial.print(":");
  Serial.println(ServerPort); 
}

void ledInit() {
  pinMode(LED_PIN,OUTPUT); //LED Ausgang
  LED_OFF;
}

void callbackAP(WiFiManager *wiFiManager) {
Serial.println("Configuration Mode");
Serial.print("AP-Name: ");
Serial.println(WIFINAME);
Serial.println(wiFiManager->getConfigPortalSSID());
Serial.print("AP IP: ");
Serial.println(WiFi.softAPIP());
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
      Serial.println("Start processing");
    } else { // ansonsten ablehnen
      WiFiClient TCPClientStop = TCPServer.available();
      Serial.println("Verbindung abgewiesen");
    }
  }
  if (!TCPClient.connected() && hasTCPClient) { //hasTCPClient as helper for previously connected Clients
    hasTCPClient = false;
    LED_OFF;
    Serial.println("Client disconnected");
  }
}

void sendSocketDataToI2C() {
    //Sende Daten an I2C Schnittstelle
    int Position=0;
    // wieviele *, max CULCOUNT -1 sternchen
    while (SocketToI2C[Position] == '*' && Position < CULCOUNT -1)
      Position++;
    int CULNo=Position;
    //Msg basteln mit \r\n \0 und ohne *
    char i2cMsg[MAXMSGSIZE];
    int j=0;
    for (;Position < SocketToI2CCount; Position++) //+1
      i2cMsg[j++]=SocketToI2C[Position];
    //i2cMsg[j++]='\r'; sollt schon enthalten sein
    i2cMsg[j++]='\n';
    i2cMsg[j]= '\0'; //Ende
    SocketToI2CCount = 0;
    
    Wire.beginTransmission(CULADDR[CULNo]);
    Wire.write(i2cMsg);
    int error = Wire.endTransmission(true);
    delay(MSGWAITTIME); //give CUL Time to process
    
    Serial.print("To Device:0x");
    Serial.print(CULADDR[CULNo],HEX);
    Serial.print(" Error: ");
    Serial.print(error);
    Serial.print(" I2CMsg: ");
    Serial.print (i2cMsg);
    // Debug
    int n =0;
    while(!i2cMsg[n]=='\0')
      Serial.print(i2cMsg[n++],HEX);
    Serial.println();
}

void handleSocketData(char data) {
  // Wenn \n dann senden  
  if (data == '\n') {
    sendSocketDataToI2C();
    //Wenn Bufferoverflow | -3 wegen \n\0
   } else if (SocketToI2CCount == MAXMSGSIZE-3) {
    Serial.println("SocketToI2C Buffer Überlauf");
    SocketToI2CCount = 0;
    //ansonsten weiter mit nächster stelle
   }else{
    SocketToI2C[SocketToI2CCount++] = data;
   }
}

void sendI2CDataToSocket(int CulNo) {
        //Buffer zusammen bauen 
        char Msg[MAXMSGSIZE]; // 
        int j=0;
        for (int asteriks=0; asteriks<CulNo ;asteriks++) //SCC Sternchen hinzufügen CulNo -1, Datne vom ersten CUL ohne *
          Msg[j++]='*';
        for (int pos=0; pos < I2CToSocketCount[CulNo]; pos++) 
          Msg[j++]=I2CToSocket[CulNo][pos];
        //Msg[j++]='\r'; sollte bereits drin sein
        Msg[j++]='\n';
        Msg[j]='\0'; //Ende
       int sentBytes = TCPClient.print(Msg);
        
        //Set Buffer pos to 0
        I2CToSocketCount[CulNo] = 0;
          //senden
        Serial.print("From Device:0x");
        Serial.print(CULADDR[CulNo],HEX);
        Serial.print(" Msg: ");
        Serial.print(Msg);
        
        // Debug
        int n =0;
        while(!Msg[n]=='\0')
          Serial.print(Msg[n++],HEX);
        Serial.println();
}


bool handleI2CData(int CulNo, char data) {
    bool returnCULBufferIsEmpty = false;
      if ( data == '\n' ) {
        sendI2CDataToSocket(CulNo);
      } else if ( data == 0x04) {
        // keine weiteren nachrichten
        //nicht schreiben nicht erhöhen
        returnCULBufferIsEmpty = true;
        //-3 wegen \n\0
      } else if (I2CToSocketCount[CulNo] == MAXMSGSIZE-3) {
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
    // Daten vom Socket lesen
    while (TCPClient.available())
      handleSocketData(TCPClient.read());
    //Daten von I2C lesen
    for (int CULNo=0; CULNo < CULCOUNT; CULNo++) {
      bool culBufferIsEmpty = false;
      while(!culBufferIsEmpty) {  
        Wire.requestFrom(CULADDR[CULNo],I2CBYTECOUNT,true);
        // Daten holen bis der CUL Buffer leer ist
        if(!Wire.available()) culBufferIsEmpty = true; //Not Empty(0x04) but nothing received
        while(Wire.available()) {
          culBufferIsEmpty = handleI2CData(CULNo,Wire.read());
        } // while(Wire.available())
      }  // while(culBufferWasEmpty)
    } //for
   }//if (TCPClient.connected())


  if (digitalRead(BUTTON_PIN) == LOW) {
    //simple blocking debounce 
    delay(2*1000);
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("Reset Setting and restart");
      WiFiManager wifiManager;
      wifiManager.resetSettings();
      ESP.restart();  
      delay(3*1000); //Just wait for Reset 
    }
  }
  
}
