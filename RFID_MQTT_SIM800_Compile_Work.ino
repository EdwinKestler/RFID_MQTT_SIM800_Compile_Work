#include <mqtt.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson/releases/tag/v5.0.7

const int verde = 13; // Pin led verde
const int rojo = 12;// Pin led rojo
const int azul = 11;// Pin led azul
String IDModulo;


char atCommand[50];
byte mqttMessage[150];
int mqttMessageLength = 0;

//Definir pines de Software Serial.

SoftwareSerial GSMSrl(7, 8); //Rx, TX Arduino --> Tx, Rx En SIM800
SoftwareSerial RFID(2,3); //Rx, TX Arduino --> Tx, Rx En RDM6300

//definir  parametros de conexion a red GSM
byte GPRSserialByte;

//Banderas de verificacion
boolean mqttSent = false;
boolean SendData = false;

//definir  parametros de conexion a servicio de MQtt
const char* server = "184.173.18.156";
const char* port =  "1883";
char* clientId = "2GNode";
char * topic = "rfid2g";
char payload[100];


//definir Parametros de Lector de RFID
int readVal = 0; // individual character read from serial
unsigned int readData[12]; // data read from serial
int counter = -1; // counter to keep position in the buffer
char tagId[12]; // final tag ID converted to a string

void setup(){
  String Res;
  pinMode(rojo, OUTPUT);
  digitalWrite(rojo, HIGH);
  //iniciar puerto serial para debug
  Serial.begin (19200);
  Serial.println(F("Serial Ready"));
  //iniciar puerto para transmicion de data GSM
  GSMSrl.begin(9600);
  //Encender Modem
  Res = GPRScommnad ("AT");
  Serial.println(Res);
  delay(200);
  //3.2.35 AT+CSQ Signal Quality Report
  Res = GPRScommnad ("AT+CSQ");
  Serial.println(Res);
  delay(200);
  // 7.2.1 AT+CGATT Attach or Detach from GPRS Service 
   Res = GPRScommnad ("AT+CGATT?");
  Serial.println (Res);
  delay(200);
  //7.2.10 AT+CGREG Network Registration Status
  Res = GPRScommnad ("AT+CGREG?");
  Serial.println (Res);
  delay(200);
  Res = GPRScommnad ("AT+CSTT=\"broadband.tigo.gt\",\"\",\"\"");
  Serial.println (Res);
  delay(200);
  Res = GPRScommnad ("AT+CIICR");
  Serial.println (Res);
  delay(200);
  Res = GPRScommnad ("AT+CIFSR");
  Serial.println (Res);
  delay(200);
  Res = GPRScommnad ("AT+CLTS=1");
  Serial.println (Res);
  delay(200);
  GetIMEI();
  Serial.println(F("GSM Ready"));
  RFID.begin(9600);
  Serial.println(F("RFID Ready"));
  pinMode(verde, OUTPUT);
  pinMode(azul, OUTPUT);
  digitalWrite(verde, LOW);
  digitalWrite(azul, LOW);  
  digitalWrite(rojo, LOW);
}

void GetIMEI (){
  String result;
  result = GPRScommnad ("AT+CGSN");
  int firstindex = result.indexOf('|');
  int secondindex = result.indexOf('|', firstindex+1);
  String command = result.substring(0, firstindex);
  String imei = result.substring(firstindex+1, secondindex);
  IDModulo = imei;
  Serial.println(IDModulo);
  delay(1000);
}

String GetTime (){
  String result;
  result = GPRScommnad ("AT+CCLK?");
  int firstindex = result.indexOf('"');
  int secondindex = result.indexOf('"', firstindex + 1);
  String command = result.substring(0, firstindex);
  String Clock = result.substring(firstindex + 1, secondindex);
  return Clock;
}

void buildJson() {
  String ISO8601 = GetTime ();
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
  for (i = 0; i < 12; ++i) {
    tagId[i] = readData[i];
  }
  tagId[12] = 0;
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
  SendData = true;
}

void loop (){  
  digitalWrite(verde, HIGH);
  ParseTag();
  if (SendData == true){
     SendData = false;
     buildJson();
     sendMQTTMessage();
  }        
}

void ParseTag(){
  RFID.listen();
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
  delay(1000);  
}

void SendMqttConnectMesage (){
  String sent;
  GSMSrl.println(F("AT+CIPSEND"));
  Serial.println(F("AT+CIPSEND"));
  delay(1000);
  mqttMessageLength = 16 + strlen(clientId);
  mqtt_connect_message(mqttMessage, clientId);
  for (int j = 0; j < mqttMessageLength; j++) {
    GSMSrl.write(mqttMessage[j]); // Message contents
    //Serial.write(mqttMessage[j]); // Message contents
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

String GPRScommnad (String comm){
  GSMSrl.listen();
  String ATComRsp,response;
  Serial.println("command:" + comm);
  GSMSrl.println(comm);
  delay(500);
  while(GSMSrl.available()>0){
    char c = GSMSrl.read();
    if (c == '\n'){
      response += ATComRsp + "|";
      ATComRsp = "";
    }else{
      ATComRsp += c;
    }
  }
  return response;
  ATComRsp ="";
  GSMSrl.flush();
  Serial.flush();
}

