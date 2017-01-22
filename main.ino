

/* ESP-SCC-CUL
 *  Adapter um mehrere CULs per SoftwareSerial im Wifi bereitzustellen 
 *  
 */
#include <Wire.h>
#include <ESP8266WiFi.h>



//Wifi
const int ServerPort  = 23;
const char* WifiSSID      = "****";
const char* WifiPassword  = "****";
#define WIFIWAITTIME       20 //Sekunden

//CUL Interface
#define I2CFREQ        400000
#define CULCOUNT       3
const char CULADDR[CULCOUNT] = {0x7e, 0x7d, 0x7c};
#define CULDELAY      10    //MS  - Zeit zwischen Senden/Empfangen - damit der CUL Zeit hat zu Arbeiten


//WifiServer
WiFiServer TCPServer(ServerPort);
WiFiClient TCPClient;
// Buffer für CUL Messages
char SocketToI2C[256];
int SocketToI2CCount = 0 ;
char I2CToSocket[CULCOUNT][256];
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
  Serial.print("Warte an: ");
  Serial.print(WiFi.localIP());
  Serial.print(":");
  Serial.println(ServerPort);


  //counter auf 0 setzen
  for(int i=0; i < CULCOUNT;i++) I2CToSocketCount[i]=0;
}



void loop() {
  //gibt es einen client
  if (TCPServer.hasClient()) {
    //Wenn noch keiner Verbunden
    if (!TCPClient.connected()){
      TCPClient = TCPServer.available();
      Serial.println("Client Verbunden");
    } else { // ansonsten ablehnen
      WiFiClient TCPClientStop = TCPServer.available();
      TCPClientStop.stop();
      Serial.println("Verbindung abgewiesen");
    }
  }
  
    // Sind Daten am Client
  if (TCPClient.connected() && TCPClient.available()){
      while (TCPClient.available()){
        SocketToI2C[SocketToI2CCount] = TCPClient.read();
        // Wenn \n dann senden
        if (SocketToI2C[SocketToI2CCount] == '\n') {
          //Sende Daten an Serielle Schnittstelle
          // an welche Schnitstelle
          int i=0;
          while (SocketToI2C[i] == '*' && i < CULCOUNT-1)
            i++;
          Serial.print(" SocketToI2C: ");
          Serial.print(SocketToI2CCount);
          Serial.print(" Device:");
          Serial.print(CULADDR[i],HEX);
          Serial.print(" Position: ");
          Serial.print(i);

          char Msg[257];
          int j=0;
          for (int pos=i;pos < SocketToI2CCount+1; pos++) {
            Msg[j++]=SocketToI2C[pos];
          }
          Msg[j]= '\0'; //Ende
          Wire.beginTransmission(CULADDR[i]);
          Wire.write(Msg);
          int error = Wire.endTransmission();

          Msg[j-2]='\0'; //remove \r\n - last 2 bytes
          Serial.print(" Error: ");
          Serial.print(error);
          Serial.print(" Msg: ");
          Serial.println(Msg);
          SocketToI2CCount = 0;
        //Wenn Bufferoverflow
        } else if (SocketToI2CCount == 255) {
          Serial.println("SocketToI2C Buffer Überlauf");
          SocketToI2CCount = 0;
        //ansonsten weiter mit nächster stelle
        }else{
          SocketToI2CCount++;
        }
      }//while (TCPClient.available())
  }//if  if (TCPClient.connected() && TCPClient.available())

//Delay um den CULs Zeit zum Arbeiten zu geben
delay(CULDELAY);
 
//i2c lesen
if (TCPClient.connected()) {
  for (int i=0; i < CULCOUNT; i++)
  {  
    // Daten holen bis der CUL Buffer leer ist 0x04
    bool quit=false;
    while(!quit) 
    {
      // immer 4Bytes holen - Adruino FIX da feste Byte Anzahl benötigt
      Wire.requestFrom(CULADDR[i],8);
      if (Wire.available() == 0) quit = true;
      while(Wire.available()) {
        char c = Wire.read();
        //Serial.println(c,HEX);
        if ( c == '\n' && I2CToSocket[i][I2CToSocketCount[i]-1] == '\r' ) {
          //senden
          Serial.print(" I2CToSocket: ");
          Serial.print(I2CToSocketCount[i]);
          Serial.print(" Device:");
          Serial.print(CULADDR[i],HEX);
          Serial.print(" Position: ");
          Serial.print(i);
          Serial.print(" Msg: ");
          //Buffer zusammen bauen
          char Msg[257];
          int j=0;
          for (int asteriks=0; asteriks<i ;asteriks++) //SCC Sternchen hinzufügen
            Msg[j++]='*';
          for (int pos=0; pos < I2CToSocketCount[i]; pos++) 
          {
            Msg[j++]=I2CToSocket[i][pos];
          }
          Msg[j++]='\n';
          Msg[j]='\0'; //Ende
          TCPClient.print(Msg);
          
          Msg[j-2]='\0'; //remove \r\n - last 2 bytes
          Serial.println(Msg);
          I2CToSocketCount[i]=0;
        } else if ( c == 0x04) {
          // keine weiteren nachrichten
          //nicht schreiben nicht erhöhen
          // Schleife beenden
          quit = true;
          break; //while(Wire.available())
        } else if (I2CToSocketCount[i] == 255) {
          Serial.println("I2CToSocket Buffer Überlauf");
          I2CToSocketCount[i] = 0;
        } else {
          // in den Buffer schreiben
          I2CToSocket[i][I2CToSocketCount[i]++] = c;
        }
         
      } //while(Wire.available())
    } //while(!quit) 
  } //for
} //if (TCPClient.connected())

//Delay um den CULs Zeit zum Arbeiten zu geben
delay(CULDELAY);
 
}
