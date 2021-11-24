#include <Arduino.h>
#include "OneWire.h"
#include "DallasTemperature.h"
#include "PubSubClient.h"
#include "WiFi.h"
#include "string.h"



#include "BluetoothSerial.h"

#define FustSzenzor       12
#define VizSzenzor        13
#define Gyujtorud         16
#define Ventilator        17
#define Adagolo           0
#define Keringeto         4

#define BLC_SIZE 4


// dioty servere gepen keresztul is fel lehet menni, ott kiir minden adatot ami szukseges
const char* wifiName = "TP-Link_B938";              // WiFi nev
const char* wifiPass = "g0lfp0l0";             // WiFi kod
const char* diotyUser = "tilben97biker@gmail.com";  // dioty-s username, alltalaban az email cimed
const char* diotyPass = "0dc9d70b";                 // kod amit emailbe kuld a dioty
const uint16_t diotyPort = 1883;                    // port alltalaban mindig 1883 
const char* myServer = "mqtt.dioty.co";             // ez nem valtozik
const char* in = "/tilben97biker@gmail.com/in";     // /email_cimed/in
const char* out1 = "/tilben97biker@gmail.com/fust";   // /email_cimed/temp1
const char* out2 = "/tilben97biker@gmail.com/viz";  // /email_cimed/temp2
const char* out3 = "/tilben97biker@gmail.com/allapot";  // /email_cimed/temp3
/////////////////////////////////////////////////////////////////////////////////////////////
WiFiClient espClient;
PubSubClient mqttClient(espClient);

BluetoothSerial BLC;

OneWire onewire(FustSzenzor);
DallasTemperature sensor1(&onewire);

OneWire onewire2(VizSzenzor);
DallasTemperature sensor2(&onewire2);

bool relay_signal = false;
bool wifi_ok = false;

uint8_t read_buff[BLC_SIZE];
bool led_pislogas = false;
float fust_temperature = 0;
float viz_temperature = 0;
bool begyulladt = false;
long old_millis = 0, new_millis = 0;
long delta_time = 0;
String uzenet;
char kimeno_uzenet[100];
int feladat = 3;
uint8_t viz_ref = 30;

// Itt erkezik be az uzenet //////////////////////////////////////////////////////////////////
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char py[100];
  memccpy(py, payload, 1 , sizeof(payload));
  if(payload[0] == 't' && payload[1] == 'r' && payload[2] == 'u' && payload[3] == 'e')
  {
    feladat = 1;
    uzenet = "";
    uzenet += "Futes elinditva";
    Serial.println(uzenet);
    BLC.println(uzenet);
    uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
    if(wifi_ok)
      mqttClient.publish(out3, kimeno_uzenet);
  }
  else if (payload[0] == 'f' && payload[1] == 'a' && payload[2] == 'l' && payload[3] == 's' && payload[4] == 'e')
  {
    feladat = 3;
    uzenet = "";
    uzenet += "Futes kikapcsolva";
    Serial.println(uzenet);
    BLC.println(uzenet);
    uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
    if(wifi_ok)
      mqttClient.publish(out3, kimeno_uzenet);
  }
  else
  {
    viz_ref = atoi(py);

    uzenet = "";
    uzenet += "Vizhomerseklet referencia erteke ";
    uzenet += viz_ref;
    uzenet += " ºC";
    Serial.println(uzenet);
    BLC.println(uzenet);
    uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
    if(wifi_ok)
      mqttClient.publish(out3, kimeno_uzenet);
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////
//  BLUETOOTH KOMUNIKACIO   ////////////////////////////////////////////////////////////////////
void Uart_Interrupt()
{
  if(BLC.available())
  {
    BLC.readBytes(read_buff, 4);
    if(read_buff[0] == '*' && read_buff[3] == '#')
    {
      if(read_buff[1] == 'S' && read_buff[2] == 'T')
      {
        feladat = 3;
        uzenet = "";
        uzenet += "Futes kikapcsolva";
        Serial.println(uzenet);
        BLC.println(uzenet);
        uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
        if(wifi_ok)
          mqttClient.publish(out3, kimeno_uzenet);
      }
      else if(read_buff[1] == 'G' && read_buff[2] == 'O')
      {
        feladat = 1;
        uzenet = "";
        uzenet += "Futes elinditva";
        Serial.println(uzenet);
        BLC.println(uzenet);
        uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
        if(wifi_ok)
          mqttClient.publish(out3, kimeno_uzenet);
      }
      else
      {
        viz_ref = read_buff[1];
        uzenet = "";
        uzenet += "Vizhomerseklet referencia erteke ";
        uzenet += viz_ref;
        uzenet += " ºC";
        Serial.println(uzenet);
        BLC.println(uzenet);
        uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
        if(wifi_ok)
          mqttClient.publish(out3, kimeno_uzenet);
      }
      
    }
  }
}
//  Hiba kezelo ///////////////////////////////////////////////////////////////////////////////
void viz_error(int hiba_uzenet) {
  switch (hiba_uzenet)
  {
  case 1:   //  FAGY  
      feladat = 1;
      uzenet = "";
      uzenet += "Viz fagyas veszely!!!";
      Serial.println(uzenet);
      BLC.println(uzenet);
      uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
      if(wifi_ok)
        mqttClient.publish(out3, kimeno_uzenet);
    feladat = 3;
    break;
  case 2:   //   FORR
    feladat = 1;
    uzenet = "";
    uzenet += "Viz forrras veszely!!!";
    Serial.println(uzenet);
    BLC.println(uzenet);
    uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
    if(wifi_ok)
      mqttClient.publish(out3, kimeno_uzenet);
    feladat = 3;
    break;
  
  default:
    break;
  }
}
void fust_error(int hiba_uzenet) {
  switch (hiba_uzenet)
  {
  case 1:   //  SZENZOR HIBA
    feladat = 1;
    uzenet = "";
    uzenet += "Fust szenzor hiba!!!";
    Serial.println(uzenet);
    BLC.println(uzenet);
    uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
    if(wifi_ok)
      mqttClient.publish(out3, kimeno_uzenet);
    feladat = 3;
    break;
  case 2:   //  KIALUDT A TUZ
    feladat = 1;
    uzenet = "";
    uzenet += "Kialudt a tuz!!!";
    Serial.println(uzenet);
    BLC.println(uzenet);
    uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
    if(wifi_ok)
      mqttClient.publish(out3, kimeno_uzenet);
    feladat = 3;
    break;
  }
}

void setup() {
  Serial.begin(9600);
  BLC.begin("Pellet kazan");
  WiFi.begin((char*)wifiName, wifiPass);
  Serial.print("Connecting to the WiFi...");
  BLC.print("Connecting to the WiFi...");
  int cnt = 0;
  while(WiFi.status() != WL_CONNECTED && cnt < 10) {    // WiFi kapcsolodas
    cnt++;
    Serial.print(".");
    delay(200);
  }
  if(WiFi.status() == WL_CONNECTED)                      // Sikeres kapcsolat
  {
    wifi_ok = true;
    Serial.print("\nConnected to the WiFi ---->  ");
    BLC.print("\nConnected to the WiFi ---->  ");
    Serial.println(WiFi.localIP());
    BLC.print(WiFi.localIP());
    mqttClient.setServer(myServer, diotyPort);        // Server beallitas

    mqttClient.setCallback(mqttCallback);             // Itt allitod be a bejovo uzenet fogadasat...
    Serial.print("Connecting to the MQTT Client...");
    BLC.print("Connecting to the MQTT Client...");
    while (!mqttClient.connected()) {
      Serial.print(".");
      BLC.print(".");
      if(mqttClient.connect("ESP32", diotyUser, diotyPass))   // MQTT severe csatlakozas
      {
        Serial.print("\nConnected to MQTT Client");
        BLC.print("\nConnected to MQTT Client");
        mqttClient.subscribe(in);                     // a bejovo uzenet topic-a, feliratkozol ra
      }
    }
  }
  else                                                // Sikertelen kapcsolodas
  {
    wifi_ok = false;
    Serial.print("Can't connect to the WiFi");
    BLC.print("Can't connect to the WiFi");
  }
  
  delay(500);
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(Gyujtorud, OUTPUT);
  pinMode(Ventilator, OUTPUT);
  pinMode(Adagolo, OUTPUT);
  pinMode(Keringeto, OUTPUT);
  sensor1.begin();
  sensor2.begin();

  digitalWrite(BUILTIN_LED, !relay_signal);
  digitalWrite(Adagolo, !relay_signal);
  digitalWrite(Keringeto, relay_signal);
  digitalWrite(Ventilator, !relay_signal);
  digitalWrite(Gyujtorud, !relay_signal);

  delay(200);

}

void loop() {

    Uart_Interrupt();

    sensor1.requestTemperatures();
    sensor2.requestTemperatures();
    fust_temperature = sensor1.getTempCByIndex(0);
    viz_temperature = sensor2.getTempCByIndex(0);

  // Uzenet letrehozasa
  uzenet = "";
  uzenet += "Allapot: ";

  switch (feladat)
  {
  case 1:   //  Begyujtas
      //  Begyujtas folyamata //

    digitalWrite(Adagolo, relay_signal);
    delay(250);
    digitalWrite(Adagolo, !relay_signal);
    digitalWrite(Gyujtorud, relay_signal);
    delay(1000);
    digitalWrite(Ventilator, relay_signal);
    while(fust_temperature < 30) {
      sensor1.requestTemperatures();
      sensor2.requestTemperatures();
      fust_temperature = sensor1.getTempCByIndex(0);
      viz_temperature = sensor2.getTempCByIndex(0);
      led_pislogas = !led_pislogas;
      digitalWrite(BUILTIN_LED, led_pislogas);
      uzenet += "Begyujtas";
      BLC.println(uzenet);
      Serial.println(uzenet);
      uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
      if(wifi_ok)
        mqttClient.publish(out3, kimeno_uzenet);
      //  Uzenet letrehozasa
      uzenet = "";
      uzenet += "Fust homerseklet: ";
      uzenet += fust_temperature;
      uzenet += " ºC";
      // Uzenet kuldes
      Serial.println(uzenet);
      BLC.println(uzenet);
      uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
      if(wifi_ok)
        mqttClient.publish(out1, kimeno_uzenet);
      //  Uzenet letrehozasa
      uzenet = "";
      uzenet += "Viz homerseklet: ";
      uzenet += viz_temperature;
      uzenet += " ºC";
      // Uzenet kuldes
      Serial.println(uzenet);
      BLC.println(uzenet);
      uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
      if(wifi_ok)
        mqttClient.publish(out2, kimeno_uzenet);
      //MQTT loop
      if(wifi_ok)
        mqttClient.loop();
      delay(200);
    } 
    digitalWrite(Adagolo, relay_signal);
    delay(500);
    digitalWrite(Adagolo, !relay_signal);
    digitalWrite(Gyujtorud, !relay_signal);
    digitalWrite(BUILTIN_LED, !relay_signal);
    feladat = 2;
    break;
  case 2:   //  Uzemeltetes
    digitalWrite(Ventilator, relay_signal);
    if(viz_temperature > viz_ref)
        digitalWrite(Keringeto, !relay_signal);
    if(viz_temperature <= 3)
        viz_error(1);
    else if(viz_temperature >= 90)
        viz_error(2);
    if(fust_temperature == 0)
        fust_error(1);
    else if(fust_temperature <= 20)
        fust_error(2);

    new_millis = millis();
    delta_time = new_millis - old_millis;
    if(delta_time > 62000)        // 2s-ig mukodik az adagolo
    {
      digitalWrite(Adagolo, !relay_signal);
      old_millis = millis();
      led_pislogas = !led_pislogas;
      digitalWrite(BUILTIN_LED, led_pislogas);
    }
    else if(delta_time > 60000)   // minden 1 percben adagol
    {
      digitalWrite(Adagolo, relay_signal);
      led_pislogas = !led_pislogas;
      digitalWrite(BUILTIN_LED, led_pislogas);
    }
    else
      digitalWrite(Adagolo, !relay_signal);
    uzenet += "Uzem allapot";
    break; 
  case 3:   //  Leallitas
    digitalWrite(BUILTIN_LED, relay_signal);
    digitalWrite(Adagolo, !relay_signal);
    digitalWrite(Keringeto, relay_signal);
    digitalWrite(Ventilator, !relay_signal);
    digitalWrite(Gyujtorud, !relay_signal);
    uzenet += "Leallitva";
    break;
  }

  Serial.println(uzenet);
  BLC.println(uzenet);
  uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
  if(wifi_ok)
    mqttClient.publish(out3, kimeno_uzenet);
  //  Uzenet letrehozasa
  uzenet = "";
  uzenet += "Fust homerseklet: ";
  uzenet += fust_temperature;
  uzenet += " ºC";
  // Uzenet kuldes
  Serial.println(uzenet);
  BLC.println(uzenet);
  uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
  if(wifi_ok)
    mqttClient.publish(out1, kimeno_uzenet);
  //  Uzenet letrehozasa
  uzenet = "";
  uzenet += "Viz homerseklet: ";
  uzenet += viz_temperature;
  uzenet += " ºC";
  // Uzenet kuldes
  Serial.println(uzenet);
  BLC.println(uzenet);
  uzenet.toCharArray(kimeno_uzenet, sizeof(kimeno_uzenet));
  if(wifi_ok)
    mqttClient.publish(out2, kimeno_uzenet);
  //MQTT loop
  if(wifi_ok)
    mqttClient.loop();
  // 100 ms befagyasztas
  delay(100);
}