#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#include <WiFiClientSecure.h> //HTTPS Request
#include <ArduinoJson.h>
#include <SoftwareSerial.h>

#define GSM_RX  D1  // Esp8266 pin 1 to URX
#define GSM_TX  D2 // Esp8266 pin 2 to UTX

SoftwareSerial simModule(GSM_TX, GSM_RX); 
HTTPClient http;
WiFiClientSecure client;
 
//Variables
int i = 0;
int statusCode;
const char* ssid = "text";
const char* passphrase = "text";
String st;
String content;

const char* SMS_FORSENDING_ENDPOINT = "https://nathanielssanrival.000webhostapp.com/show-sms";
const char* UPDATE_SMS_ENDPOINT = "https://nathanielssanrival.000webhostapp.com/update-sms/";
int HTTP_STATUS_CODE_OK = 0;
 
 
//Function Declaration
bool testWifi(void);
void launchWeb(void);
void setupAP(void);
 
//Establishing Local server at port 80 whenever required
ESP8266WebServer server(80);

void setup(){
 
  Serial.begin(115200); //Initialising if(DEBUG)Serial Monitor
  simModule.begin(9600,EspSoftwareSerial::SWSERIAL_8N1,GSM_RX,GSM_TX);

  Serial.println("Disconnecting previously connected WiFi");
  WiFi.disconnect();
  EEPROM.begin(512); //Initializing EEPROM
  delay(10);
  Serial.println("Startup");
 
  Serial.println("Reading EEPROM ssid");
 
  String esid;
  for (int i = 0; i < 32; ++i){
    esid += char(EEPROM.read(i));
  }
  Serial.println();
  Serial.print("SSID: ");
  Serial.println(esid);
  Serial.println("Reading EEPROM pass");
 
  String epass = "";
  for (int i = 32; i < 96; ++i){
    epass += char(EEPROM.read(i));
  }
  Serial.print("PASS: ");
  Serial.println(epass);

 
  WiFi.begin(esid.c_str(), epass.c_str());
  if (testWifi()){
  //if (false){
    Serial.println("Succesfully Connected!!!");
    return;
  }else{
    Serial.println("Turning the HotSpot On");
    launchWeb();
    setupAP();// Setup HotSpot
  }
 
  Serial.println();
  Serial.println("Waiting.");
  
  while ((WiFi.status() != WL_CONNECTED)){
    Serial.print(".");
    delay(100);
    server.handleClient();
  }
 
}
void loop() {
  if ((WiFi.status() == WL_CONNECTED)){
    handleProcess();
  }else{
    Serial.print("No Connection");
  }
}

//Handles the process for reading API,Sending SMS and Tagging record.
void handleProcess(){
  client.setInsecure();
  http.begin(client, SMS_FORSENDING_ENDPOINT);
  delay(100);
  int httpCode = http.GET();  
  String payload = http.getString();  //Get the response payload

  Serial.println("----------------------"); 
  Serial.println(httpCode);   //Print HTTP return code
  //Serial.println(payload);    //Print request response payload

  if (httpCode > HTTP_STATUS_CODE_OK){
    //const size_t bufferSize = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(8) + 370;
    const size_t bufferSize = 1024;
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonArray& nodes = jsonBuffer.parseArray(payload);
    if(payload.length() > 0){
          if (!nodes.success()) {
            Serial.println("parseObject() failed");
          }else{
            int node_length = nodes.size(); 
            Serial.print("List Size:"); 
            Serial.println(node_length); 
            for(int i=0; i<node_length;i++){

              String id = nodes[i]["id"].as<const char*>();
              String message = nodes[i]["message"].as<const char*>();
              String cus_mobile = nodes[i]["cus_mobile"].as<const char*>();
            
              Serial.print("userId:");
              Serial.println(id);
              Serial.print("message:");
              Serial.println(message);
              Serial.print("cus_mobile:");
              Serial.println(cus_mobile);
              delay(200);
              sendSms(cus_mobile, message);
             
              delay(10000);
              updateRecord(id);
              delay(10000);
            }

            http.end();
          }
    }else{
        Serial.println("Payload Empty");
    }
  }
  delay(60000);
}

int updateRecord(String id){
  String endpoint = UPDATE_SMS_ENDPOINT + id;
  
  http.begin(client, endpoint);
  delay(100);
  Serial.println("Updating recrod....");
  Serial.println("Endpoint = " + endpoint);

  delay(100);
  int httpCode = http.GET(); 
  if(httpCode == HTTP_STATUS_CODE_OK){
      Serial.println("Updated record with id: " + id);
  }
  http.end();
  delay(100);
  return httpCode;
}

void sendSms(String mobileNumber,String message){
    Serial.println ("Sending Message");
    simModule.println("AT+CMGF=1");    //Sets the GSM Module in Text Mode
    delay(1000);
    Serial.println("Set SMS Number");
    simModule.println("AT+CMGS=\"" + mobileNumber + "\"\r"); //Mobile phone number to send message
    delay(1000);
    simModule.println(message);
    delay(1000);
    simModule.println((char)26);// ASCII code of CTRL+Z
    delay(1000);
}
 
//-------- Fuctions used for WiFi credentials saving and connecting to it which you do not need to change 
bool testWifi(void){
  int c = 0;
  Serial.println("Waiting for Wifi to connect");
  while ( c < 20 ) {
    if (WiFi.status() == WL_CONNECTED){
      return true;
    }
    delay(500);
    Serial.print("*");
    c++;
  }
  Serial.println("Connect timed out, opening AP");
  return false;
}
 
void launchWeb(){
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED){
    Serial.println("WiFi connected");
  }
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  createWebServer();
  // Start the server
  server.begin();
  Serial.println("Server started");
}
 
void setupAP(void){
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i){
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");
  st = "<ol>";
  for (int i = 0; i < n; ++i){
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);
 
    st += ")";
    st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    st += "</li>";
  }
  st += "</ol>";
  delay(100);
  WiFi.softAP("ESP8266-WIFI", "");
  Serial.println("softap");
  launchWeb();
  Serial.println("over");
}
 
void createWebServer(){
  server.on("/", []() {
 
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      content = "<!DOCTYPE HTML>\r\n<html style = \"padding: 30px;\"><b>ESP8266 | " + ipStr + "</b>";
      content += "<form action=\"/scan\" method=\"POST\"><input style=\"display: inline-block;border-radius: 4px;background-color: #f4511e;border: none;color: #FFFFFF;text-align: center;font-size: 15px;padding: 15px;width: 120px;transition: all 0.5s;cursor: pointer;margin: 5px;}\" type=\"submit\" value=\"Scan Network\"></form>";
     // content += ipStr;
      content += "<hr style='border-top: 3px dashed #f4511e;'>";
      content += "<p>List of available networks:</p>";
      content += "<p>";
      content += st;
      content += "<hr style='border-top: 3px dashed #f4511e;'>";
      content += "<p>Choose WIFI from the list above to connect</p>";
      content += "</p><form method='get' action='setting'><label style = \"font-size: 2em\">SSID: </label><input style = \"width: 50%;padding: 15px 5px;margin: 2px 0;box-sizing: border-box;\" name='ssid' length=32><br/><br/><label style = \"font-size: 2em\">PASS: </label><input style = \"width: 50%;padding: 15px 5px;margin: 2px 0;box-sizing: border-box;\" name='pass' length=64><br/><br/>";
      content += " <input type='submit' style=\"display: inline-block;border-radius: 4px;background-color: #f4511e;border: none;color: #FFFFFF;text-align: center;font-size: 15px;padding: 15px;width: 120px;transition: all 0.5s;cursor: pointer;margin: 5px;}\" value=\"Connect\"></form>";
      content += "</html>";
      server.send(200, "text/html", content);
    });
    server.on("/scan", []() {
      //setupAP();
      IPAddress ip = WiFi.softAPIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
 
      content = "<!DOCTYPE HTML>\r\n<html>go back";
      server.send(200, "text/html", content);
    });
 
    server.on("/setting", []() {
      String qsid = server.arg("ssid");
      String qpass = server.arg("pass");
      if (qsid.length() > 0) {
        Serial.println("clearing eeprom");
        for (int i = 0; i < 96; ++i) {
          EEPROM.write(i, 0);
        }
        Serial.println(qsid);
        Serial.println("");
        Serial.println(qpass);
        Serial.println("");
 
        Serial.println("writing eeprom ssid:");
        for (int i = 0; i < qsid.length(); ++i)
        {
          EEPROM.write(i, qsid[i]);
          Serial.print("Wrote: ");
          Serial.println(qsid[i]);
        }
        Serial.println("writing eeprom pass:");
        for (int i = 0; i < qpass.length(); ++i)
        {
          EEPROM.write(32 + i, qpass[i]);
          Serial.print("Wrote: ");
          Serial.println(qpass[i]);
        }
        EEPROM.commit();
 
        content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
        statusCode = 200;
        ESP.reset();
      } else {
        content = "{\"Error\":\"404 not found\"}";
        statusCode = 404;
        Serial.println("Sending 404");
      }
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(statusCode, "application/json", content);
 
    });
}