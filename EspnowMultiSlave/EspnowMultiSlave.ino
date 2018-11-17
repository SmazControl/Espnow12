// first 30 byte Receiving  Data ESPNOW SLAVE
// second 30 byte Sending   Data 

/**
   ESPNOW - Basic communication - Slave
   Date: 26th September 2017
   Author: Arvind Ravulavaru <https://github.com/arvindr21>
   Purpose: ESPNow Communication between a Master ESP32 and multiple ESP32 Slaves
   Description: This sketch consists of the code for the Slave module.
   Resources: (A bit outdated)
   a. https://espressif.com/sites/default/files/documentation/esp-now_user_guide_en.pdf
   b. http://www.esploradores.com/practica-6-conexion-esp-now/
   << This Device Slave >>
   Flow: Master
   Step 1 : ESPNow Init on Master and set it in STA mode
   Step 2 : Start scanning for Slave ESP32 (we have added a prefix of `slave` to the SSID of slave for an easy setup)
   Step 3 : Once found, add Slave as peer
   Step 4 : Register for send callback
   Step 5 : Start Transmitting data from Master to Slave(s)
   Flow: Slave
   Step 1 : ESPNow Init on Slave
   Step 2 : Update the SSID of Slave with a prefix of `slave`
   Step 3 : Set Slave in AP mode
   Step 4 : Register for receive callback and wait for data
   Step 5 : Once data arrives, print it in the serial monitor
   Note: Master and Slave have been defined to easily understand the setup.
         Based on the ESPNOW API, there is no concept of Master and Slave.
         Any devices can act as master or salve.
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

IPAddress    apIP(42, 42, 42, 42);

extern "C" {
    #include <espnow.h>
     #include <user_interface.h>
}

// Define a web server at port 80 for HTTP
ESP8266WebServer server(80);

#define CHANNEL 1

byte firstData[30]; //Receiving
byte secondData[30] = {125,126}; //Sending 125,126 are Example Data

    // keep in sync with slave struct
struct __attribute__((packed)) DataStruct {
    char text[90];
    unsigned int time;
};

DataStruct receivedData;

DataStruct replyData;

String Mc[10];
int McNum = 0;

void handleRoot() {
   
  char html[1000];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  Serial.println("handleRoot");

  String StringMc="";
  for (int i=0; i<=McNum; i++) {
    StringMc = Mc[McNum] + "<p><br>";
    Serial.println(Mc[McNum]);
  }
  char charBuf[50];
  StringMc.toCharArray(charBuf, 50);


// Build an HTML page to display on the web-server root address
  snprintf ( html, 1000,
 
"<html>\
  <head>\
    <meta http-equiv='refresh' content='10'/>\
    <title>ESP8266 WiFi Network</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; font-size: 1.5em; Color: #000000; }\
      h1 { Color: #AA0000; }\
    </style>\
  </head>\
  <body>\
    <h1>ESP8266 Wi-Fi Access Point and Web Server Demo</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <p>Receiving Data: %02u, %02u</p>\
    <p>%s</p>\
    <p>This page refreshes every 10 seconds. Click <a href=\"javascript:window.location.reload();\">here</a> to refresh the page now.</p>\
  </body>\
</html>",

    hr, min % 60, sec % 60, firstData[0], firstData[1], charBuf
  );
  server.send ( 200, "text/html", html );
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
}



// Init ESP Now with fallback
void InitESPNow() {
  WiFi.disconnect();
  if (esp_now_init() == 0) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    // Retry InitESPNow, add a counte and then restart?
    // InitESPNow();
    // or Simply Restart
    ESP.restart();
  }
}

// config AP SSID
void configDeviceAP() {
  String Prefix = "Slave:";
  String Mac = WiFi.macAddress();
  String SSID = Prefix + Mac;
  String Password = "123456789";
  bool result = WiFi.softAP(SSID.c_str(), Password.c_str(), CHANNEL, 0);
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(SSID));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("ESPNow/Basic/Slave Example");
  //Set device in AP mode to begin with
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));   // subnet FF FF FF 00  
  
  // configure device AP mode
  configDeviceAP();
  // This is the mac address of the Slave in AP Mode
  Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());
  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info.
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(receiveCallBackFunction);
  strcpy(replyData.text, "Goodnight John-Boy");
  Serial.print("Message "); Serial.println(replyData.text);

  /* You can remove the password parameter if you want the AP to be open. */
  //Use ssid of Espnow 
  //WiFi.softAP(ssid, password);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  server.on ( "/", handleRoot );
  server.onNotFound ( handleNotFound );
  
  server.begin();
  Serial.println("HTTP server started");

  Serial.println("End of setup - waiting for messages");  
}

// callback when data is recv from Master
void receiveCallBackFunction(uint8_t *senderMac, uint8_t *incomingData, uint8_t len) {
    String Mc_buff = "";
    memcpy(&receivedData, incomingData, sizeof(receivedData));
    Serial.print("NewMsg ");
    Serial.print("MacAddr ");
    for (byte n = 0; n < 6; n++) {
        Serial.print (senderMac[n], HEX);
        Mc_buff = Mc_buff+String(senderMac[n],HEX);
    }
    bool found = false;
    for (int i=0; i<=McNum; i++) {
      if (Mc_buff == Mc[i]) {
        found = true;
      }
      if (!found) {
        McNum++;
        Mc[McNum] = Mc_buff;
      }
    }
    Serial.print("  MsgLen ");
    Serial.println(len);
    Serial.print("  Name ");
    Serial.println(receivedData.text);
    Serial.print("  Time ");
    Serial.print(receivedData.time);
    Serial.println();

    for (int i=0; i<90; i=i+3) {
      String S = String(receivedData.text[i]) + String(receivedData.text[i+1]) + String(receivedData.text[i+2]);
      firstData[i/3] = S.toInt();
    }

    Serial.println("  Example Data");
    Serial.println(firstData[0]);
    Serial.println(firstData[1]);
    

    sendReply(senderMac);
}


void sendReply(uint8_t *macAddr) {

    // bring Data Array to replyData.text
    String DataText = "";
    for (int i=0; i<30; i++) {
      if (secondData[i]<30) {
        DataText=DataText + " ";       
      }
      if (secondData[i]<10) {
        DataText=DataText + " ";       
      }
      DataText=DataText + String(secondData[i]);
    }
    DataText.toCharArray(replyData.text,300);
  
        // create a peer with the received mac address
    esp_now_add_peer(macAddr, ESP_NOW_ROLE_COMBO, CHANNEL, NULL, 0);

    replyData.time = millis();
    uint8_t byteArray[sizeof(replyData)];
    memcpy(byteArray, &replyData, sizeof(replyData));

    esp_now_send(NULL, byteArray, sizeof(replyData)); // NULL means send to all peers
    Serial.println("sendReply sent data");

        // data sent so delete the peer
    esp_now_del_peer(macAddr);
}

void loop() {
  server.handleClient();
  // Chill
}
