

/* ESP-SCC-CUL
 *  Adapter um mehrere CULs per SoftwareSerial im Wifi bereitzustellen 
 *  
 */
#include <Wire.h>
#include <ESP8266WiFi.h>



//Wifi
const int ServerPort  = 23;
const char* WifiSSID      = "NewBigBoss";
const char* WifiPassword  = "Waldgeist99";
#define WIFIWAITTIME       20 //Sekunden

//CUL Interface
#define I2CFREQ         400000
#define CULCOUNT        3
const char CULADDR[CULCOUNT] = {0x7e, 0x7d, 0x7c};
#define MAXMSGSIZE      128

#define LED_PIN         2  //WEMOS D4 Buildin LED/Pull-Up
#define LED_OFF         digitalWrite(LED_PIN,HIGH)
#define LED_ON          digitalWrite(LED_PIN,LOW)

#define BUTTON_PIN      0  //WEMOS D3 Pull-Up


//WifiServer
WiFiServer TCPServer(ServerPort);
WiFiClient TCPClient;
// Buffer für CUL Messages
char SocketToI2C[MAXMSGSIZE];
int SocketToI2CCount = 0 ;
char I2CToSocket[CULCOUNT][MAXMSGSIZE];
int I2CToSocketCount[CULCOUNT];



void setup() {
  // Debug Console auf HW Serial 
  Serial.begin(115200);

  //Infos
  Serial.println("ESP-Multi-I2CCUL-Bridge by B.Stark");
  Serial.println("Version: Proof of Concept");
  Serial.print("ESP ChipID: ");
  Serial.println(ESP.getChipId(),HEX);
  Serial.print("ESP FreeHeap: ");
  Serial.println(ESP.getFreeHeap());

  //Wifi Verbinden
  WiFi.begin(WifiSSID,WifiPassword);
  Serial.print("Verbinde zu SSID: ");
  Serial.print(WifiSSID);
  //warten auf Wifi
  int ConCount = 0;
  while ( WiFi.status() != WL_CONNECTED && ConCount++ < WIFIWAITTIME) {
    Serial.print(".");
    delay(1000);  
  }
  if ( ConCount == WIFIWAITTIME + 1 ) {
    Serial.println("Keine Verbindung");
    Serial.println(WiFi.status());
    // RESTART
    while(true);
  }
  Serial.println("OK");
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

  pinMode(BUTTON_PIN,INPUT);
  
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
      LED_ON;
    } else { // ansonsten ablehnen
      WiFiClient TCPClientStop = TCPServer.available();
      LED_OFF;
      Serial.println("Verbindung abgewiesen");
    }
  }
  if (!TCPClient.connected()) LED_OFF;
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
    for (int pos=i;pos < SocketToI2CCount+1; pos++)
      Msg[j++]=SocketToI2C[pos];
    Msg[j++]='\n';
    Msg[j]= '\0'; //Ende
    Wire.beginTransmission(CULADDR[i]);
    Wire.write(Msg);
    int error = Wire.endTransmission();
    Serial.print(" Error: ");
    Serial.print(error);
    Serial.print(" Msg: ");
    Serial.println(Msg);
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
      } else if (I2CToSocketCount[CulNo] == MAXMSGSIZE-1) {
        Serial.println("I2CToSocket Buffer Überlauf");
        I2CToSocketCount[CulNo] = 0;
      } else {
          // in den Buffer schreiben
          I2CToSocket[CulNo][I2CToSocketCount[CulNo]++] = data;
      }
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
      Wire.requestFrom(CULADDR[i],32);
      // Daten holen bis der CUL Buffer leer ist
      while(Wire.available()) 
        handleI2CData(i,Wire.read());
    }//for
   }//if (TCPClient.connected())

  if (digitalRead(BUTTON_PIN) == LOW) Serial.println("Button gedrückt");
}
