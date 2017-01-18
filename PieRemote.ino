/*
 * PieRemote -- An ESP8266 IR Remote powered by NETPIE
 * Created by NETPIE.io
 */

#define NETPIE

#include "config.h"
#include "PieRemote.h"
#include <Ticker.h>

#ifdef NETPIE
#include <MicroGear.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

WiFiClient client;
MicroGear microgear(client);
#endif

PieRemote *pieremote;
Ticker ticker;
char cmd[MAXCMDLENGT+1];

#ifdef NETPIE
void onMsghandler(char *topic, uint8_t* msg, unsigned int msglen) {
  Serial.print("Incoming message --> ");
  msg[msglen] = '\0';
  Serial.println((char *)msg);
  pieremote->executeCMD((char *)msg);
}

void onConnected(char *attribute, uint8_t* msg, unsigned int msglen) {
  Serial.println("Connected to NETPIE...");
  microgear.setAlias("pieremote");
  #ifdef LED_PIN
    ticker.detach();
    digitalWrite(LED_PIN, HIGH);
  #endif
}
#endif

void onNewSignalHandler(char *referer, char *msg, uint8_t result) {
  #ifdef NETPIE
    microgear.publish("/pieremote/new","/noname");
  #endif
  #ifdef LED_PIN
    ticker.detach();
    digitalWrite(LED_PIN, HIGH);
  #endif
}

void onIRListHandler(char *referer, char *msg, uint8_t result) {
  char rb[32];
  sprintf(rb,"/pieremote/ls%s",referer+3);
  #ifdef NETPIE
    microgear.publish(rb,msg);
  #endif
}

void onCmdResultHandler(char *referer, char *msg, uint8_t result) {
  #ifdef LED_PIN
    if (strcmp(referer,"new")==0) {
      if (result==CMD_OK) {
        ticker.attach(0.5, toggleLED);
      }
      else {
        ticker.detach();
        digitalWrite(LED_PIN, HIGH);
      }
    }
  #endif
}

uint16_t serialReadln(char *buff, uint16_t maxlen) {
  char *p = buff;
  while(p < buff+maxlen-1) {
    if (Serial.available()) {
      *p = Serial.read();
      if (*p != '\n') p++;
      else break;
    }
    else {
      if (p == buff) break;
    }
  }
  *p = '\0';
  return buff - p;
}

#ifdef LED_PIN
void toggleLED() {
  uint8_t state = digitalRead(LED_PIN);
  digitalWrite(LED_PIN, !state);
}
#endif

void  setup ( ) {
  Serial.begin(115200);

  #ifdef LED_PIN
    pinMode(LED_PIN, OUTPUT);
    #ifdef NETPIE
      ticker.attach(0.05, toggleLED);
    #else
      digitalWrite(LED_PIN, HIGH);
    #endif
  #endif
    
  #ifdef NETPIE
    WiFiManager wifiManager;
    wifiManager.setTimeout(180);
  #endif
  
  pieremote = new PieRemote();
  bool result = SPIFFS.begin();
  #ifdef DEBUG
    Serial.print("SPIFFS opened: ");
    Serial.println(result);
  #endif DEBUG

  #ifdef NETPIE
    microgear.on(MESSAGE,onMsghandler);
    microgear.on(CONNECTED,onConnected);  
    if(!wifiManager.autoConnect(APMODE_SSID)) {
      Serial.println("Failed to connect and hit timeout");
      delay(3000);
      ESP.reset();
    }
    Serial.println("Wifi ready, connecting to NETPIE");  
    microgear.init(KEY,SECRET,ALIAS);
    microgear.connect(APPID);

    pieremote->setCallback(NEWSIGNAL,onNewSignalHandler);
    pieremote->setCallback(IRLIST,onIRListHandler);
    pieremote->setCallback(CMDRESULT,onCmdResultHandler);
  #endif
}

void  loop ( ) {
  #ifdef NETPIE
    if (microgear.connected()) {
      microgear.loop();
    }
  #endif

  if (serialReadln(cmd,32)) {
    pieremote->executeCMD(cmd);
  }
  pieremote->tick();
}

