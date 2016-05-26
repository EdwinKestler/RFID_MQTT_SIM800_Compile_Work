#include <mqtt.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson/releases/tag/v5.0.7

const int verde = 13; // Pin led verde
const int rojo = 12;// Pin led rojo
const int azul = 11;// Pin led azul
String ISO8601;
String IDModulo;
byte data2;
String ATComRsp;

char atCommand[50];
byte mqttMessage[250];
int mqttMessageLength = 0;

//Definir pines de Software Serial.

SoftwareSerial GSMSrl(7, 8); //Rx, TX Arduino --> Tx, Rx En SIM800
SoftwareSerial RFID(2,3); //Rx, TX Arduino --> Tx, Rx En RDM6300

//definir  parametros de conexion a red GSM
byte GPRSserialByte;

//Banderas de verificacion
boolean CheckSim800 = false;
boolean mqttSent = false;

//definir  parametros de conexion a servicio de MQtt
const char* server = "67.228.191.108";
const char* port =  "1883";
char* clientId = "2GNode";
char * topic = "prueba";
char payload[250];


//definir Parametros de Lector de RFID
int readVal = 0; // individual character read from serial
unsigned int readData[10]; // data read from serial
int counter = -1; // counter to keep position in the buffer
char tagId[11]; // final tag ID converted to a string

boolean isGPRSReady(){
  Serial.println(F("AT"));
  GSMSrl.println(F("AT"));
  GSMSrl.println(F("AT+CGATT?"));
  GPRSread();
  GSMSrl.println(F("AT+CGREG?"));
  GPRSread();
  Serial.print(F("data2:"));
  Serial.println(data2);
  delay (200);
  Serial.println(F("Check OK"));
  Serial.print(F("AT command RESP:"));
  Serial.println(ATComRsp);
  delay(100);
  if (data2 > -1){
    Serial.println(F("GPRS OK"));
    return true;
  }
  else {
    Serial.println(F("GPRS NOT OK"));
    return false;
  }
}

void setup(){
  pinMode(rojo, OUTPUT);
  digitalWrite(rojo, HIGH);
  //iniciar puerto serial para debug
  Serial.begin (19200);
  Serial.println(F("Serial Ready"));
  //iniciar puerto para transmicion de data GSM
  GSMSrl.begin(9600);
  CheckSim800 = isGPRSReady();
  delay(500);
  wakeUpModem ();
  ConnectToAPN();
  BringUpGPRS();
  GetIPAddress();
  Serial.println(F("GSM Ready"));
  RFID.begin(9600);
  Serial.println(F("RFID Ready"));
  GetIMEI();
  pinMode(verde, OUTPUT);
  pinMode(azul, OUTPUT);
  digitalWrite(verde, LOW);
  digitalWrite(azul, LOW);  
  digitalWrite(rojo, LOW);
}

void wakeUpModem (){
  GSMSrl.println(F("AT")); // Sends AT command to wake up cell phone
  GPRSread();
  delay(800); // Wait a second
}

void ConnectToAPN(){
  GSMSrl.println(F("AT+CSTT=\"broadband.tigo.gt\",\"\",\"\"")); // Puts phone into GPRS mode
  GPRSread();
  delay(1000); // Wait a second
}

void BringUpGPRS(){
  GSMSrl.println(F("AT+CIICR"));  
  GPRSread();
  delay(1000);  
}

void GetIPAddress(){
  GSMSrl.println(F("AT+CIFSR"));
  GPRSread();
  delay(1000);
}


boolean  GetIMEI (){
  String imei = "123456789";
  IDModulo = imei;
  Serial.println("IDModulo:" +IDModulo);
  return true;
}

boolean GetTime (){
  String Time = "01/05/2016 15:30";
  ISO8601 = Time;
  Serial.println("time:" +ISO8601);
  return true;
}

void buildJson() {
  StaticJsonBuffer<250> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonObject& d = root.createNestedObject("d");
  JsonObject& data = d.createNestedObject("data");
  data["imei"] = IDModulo;
  data["tag"] = tagId;
  data["timestamp"] = ISO8601;
  root.printTo(payload, sizeof(payload));
  Serial.println(F("publishing device metadata:")); 
  Serial.println(payload);
}

// convert the int values read from serial to ASCII chars
void parseTag() {
  int i;
  for (i = 0; i < 10; ++i) {
    tagId[i] = readData[i];
  }
  tagId[10] = 0;
}

// this function clears the rest of data on the serial, to prevent multiple scans
void clearSerial() {
   while (Serial.read() >= 0) {
    ; // do nothing
  }
  Serial.flush();
   while (GSMSrl.read() >= 0) {
    ; // do nothing
  }
  GSMSrl.flush();
  while (RFID.read() >= 0) {
    ; // do nothing
  }
  RFID.flush();
  //asm volatile ("  jmp 0");
}

void printTag() {
  Serial.print(F("Tag value: "));
  Serial.println(tagId);
  digitalWrite(azul, HIGH);
  delay (15);
  digitalWrite(azul, LOW);
}

void loop (){
  if(CheckSim800 = true){
  digitalWrite(verde, HIGH);
  }
    
  if (RFID.available() > 0) {
    // read the incoming byte:
    readVal = RFID.read();
    digitalWrite(verde, LOW);
    
    switch (readVal) {
      case 2:
      counter = 0; // start reading
      break;
      case 3:
      // process the tag we just read
      parseTag();
      GetTime();
      buildJson();
      sendMQTTMessage();
      // clear serial to prevent multiple reads
      clearSerial();
      // reset reading state
      counter = -1;
      break;
      default: 
      // save valuee
      readData[counter] = readVal;
      // increment counter
      ++counter;
      break;
    }
  }  
}

void sendMQTTMessage(){
 StablishTCPconnection ();
 SendMqttConnectMesage();
 sendMqttMessage();
 CloseTCPConnection();
}

void StablishTCPconnection (){
  strcpy(atCommand, "AT+CIPSTART=\"TCP\",\"");
  strcat(atCommand, server);
  strcat(atCommand, "\",\"");
  strcat(atCommand, port);
  strcat(atCommand, "\"");
  GSMSrl.println(atCommand);
  GPRSread();
  delay(1000);  
}

void SendMqttConnectMesage (){
  GSMSrl.println(F("AT+CIPSEND"));
  Serial.println(F("AT+CIPSEND"));
  delay(1000);
  mqttMessageLength = 16 + strlen(clientId);
  //Serial.println(mqttMessageLength);
  delay(100);
  mqtt_connect_message(mqttMessage, clientId);
  for (int j = 0; j < mqttMessageLength; j++) {
    GSMSrl.write(mqttMessage[j]); // Message contents
    Serial.write(mqttMessage[j]); // Message contents
    //Serial.println("");
  }
  GSMSrl.write(byte(26)); // (signals end of message)
  Serial.println(F("Sent"));
  delay(1000);
}

void sendMqttMessage () {
  digitalWrite(azul, HIGH);
  GSMSrl.println(F("AT+CIPSEND"));
  Serial.println(F("AT+CIPSEND"));
  delay(1000);
  mqttMessageLength = 4 + strlen(topic) + strlen(payload);
  Serial.println(mqttMessageLength);
  delay(100);
  mqtt_publish_message(mqttMessage, topic, payload);
  for (int k = 0; k < mqttMessageLength; k++) {
    GSMSrl.write(mqttMessage[k]);
    Serial.write((byte)mqttMessage[k]);
  }
  GSMSrl.write(byte(26)); // (signals end of message)
  Serial.println(F(""));
  Serial.println(F("-------------Sent-------------")); // Message contents
  delay(1000); 
  digitalWrite(azul, LOW);
}

void CloseTCPConnection (){
  GSMSrl.println(F("AT+CIPCLOSE"));
  Serial.println(F("AT+CIPCLOSE"));
  delay(1000);
}

void GPRSread (){  
  if (GSMSrl.available()){
    while(GSMSrl.available()>0){
      data2 = (char)GSMSrl.read();
      Serial.write(data2);
      ATComRsp += char(data2);
    }
    Serial.println(ATComRsp);
    delay(200);
  }
  ATComRsp ="";
  GSMSrl.flush();
  Serial.flush();
}

