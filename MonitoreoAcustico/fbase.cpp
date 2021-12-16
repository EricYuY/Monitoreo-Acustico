#include "fbase.h"
#include "addons/TokenHelper.h"
#include <WiFi.h>
#include "time.h"
#include "ntpHora.h"

#define WIFI_SSID "Yu"
#define WIFI_PASSWORD "45212415"

#define API_KEY "AIzaSyA6i6gQC4gCjiquv3wyFJZMTfWtK03tJPs"
#define FIREBASE_PROJECT_ID "monitoreo-acustico"
#define USER_EMAIL "eric.yu@pucp.edu.pe"
#define USER_PASSWORD "12345678"

static Network *instance=NULL;

Network::Network() {
  instance = this;
}

void WiFiEventConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("WIFI CONNECTED! ESPERANDO IP LOCAL:");
}

void WiFiEventGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.print("LOCAL IP ADDRESS: ");
  Serial.println(WiFi.localIP());
  instance->firebaseInit();
}

void WiFiEventDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("WIFI DISCONNECTED!");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void FirestoreTokenStatusCallback(TokenInfo info){
  Serial.printf("Token Info: type = %s, status = %s\n", getTokenType(info).c_str(), getTokenStatus(info).c_str());
}

void Network::initWiFi() {
  WiFi.disconnect();
  WiFi.onEvent(WiFiEventConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(WiFiEventGotIP, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(WiFiEventDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void Network::firebaseInit(){
  config.api_key = API_KEY;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.token_status_callback = FirestoreTokenStatusCallback;

  Firebase.begin(&config, &auth);
}


void Network::firestoreDataUpdate(int timeframe,double SPL, double LAeq, double LA10, double LA90, double* octave, char* hora){
  
  if(WiFi.status() == WL_CONNECTED && Firebase.ready()){

    int i;
    String s = "";
    for (i = 0; i < 30; i++)s = s + hora[i];

  
    String documentPath1 = "Parámetros/" + s;
    String documentPath2 = "SPL/" + s;

    FirebaseJson content;
    FirebaseJson content2;

    if(timeframe==1){
      content.set("fields/LAeq/doubleValue", String(LAeq).c_str());
      content.set("fields/LA10/doubleValue", String(LA10).c_str());
      content.set("fields/LA90/doubleValue", String(LA90).c_str());
      content.set("fields/Octave1_125Hz/doubleValue", String(octave[0]).c_str());
      content.set("fields/Octave2_250Hz/doubleValue", String(octave[1]).c_str());
      content.set("fields/Octave3_500Hz/doubleValue", String(octave[2]).c_str());
      content.set("fields/Octave4_1kHz/doubleValue", String(octave[3]).c_str());
      content.set("fields/Octave5_2kHz/doubleValue", String(octave[4]).c_str());
      content.set("fields/Octave6_4kHz/doubleValue", String(octave[5]).c_str());
      content.set("fields/Octave7_8kHz/doubleValue", String(octave[6]).c_str());
      content.set("fields/Octave8_16kHz/doubleValue", String(octave[7]).c_str());

      if(Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath1.c_str(), content.raw(), "LAeq,LA10,LA90,Octave1_125Hz,Octave2_250Hz,Octave3_500Hz,Octave4_1kHz,Octave5_2kHz,Octave6_4kHz,Octave7_8kHz,Octave8_16kHz")){
        //Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
        return;
      }else{
        //Serial.println(fbdo.errorReason());
      }
  
      if(Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath1.c_str(), content.raw())){
        //Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
        return;
      }else{
        //Serial.println(fbdo.errorReason());
      }
    }

    if(timeframe ==0){
      content2.set("fields/SPL/doubleValue", String(SPL).c_str());
      
      if(Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath2.c_str(), content2.raw(), "SPL")){
        //Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
        return;
      }else{
        //Serial.println(fbdo.errorReason());
      }
  
      if(Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath2.c_str(), content2.raw())){
        //Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
        return;
      }else{
        //Serial.println(fbdo.errorReason());
      }
    }
  }
}
