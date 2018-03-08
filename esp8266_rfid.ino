#include <everytime.h>
#include<ESP8266WiFi.h>
#include<ESP8266WebServer.h>
#include<ESP8266HTTPClient.h>
#include<ArduinoJson.h>
#include <MillisTimer.h>
#include <SPI.h>
#include <MFRC522.h>
#define SS_PIN 4  //D2
#define RST_PIN 5 //D1

ESP8266WebServer server;
StaticJsonBuffer<2000> jsonBuffer;

const char* ssid = "ssid";
const char* password = "password";
const String gatewayHost="host_addres";


boolean authenticated = false;
const char* access_token;
String authorization="";
char MAC_char[18];
String  registerRequestBody;


MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.
int out = 0;

MillisTimer registerTimer = MillisTimer(1000);
MillisTimer readStatesTimer = MillisTimer(1000);


void info() {
  String htmlInfoMsg = "<h1> MINI WEB SERVER INFO  </h1>";

  authenticated ? htmlInfoMsg = htmlInfoMsg + "<h2>Succesfull authenticated</h2>" :  htmlInfoMsg =  htmlInfoMsg +  "<h2>Not authenticated</h2>";
  registerTimer.isRunning() ? htmlInfoMsg = htmlInfoMsg + "<h2>Period register enabled</h2>" :  htmlInfoMsg =  htmlInfoMsg +  "<h2>Period register disabled</h2>";

  server.send(200, "text/html", htmlInfoMsg);
  htmlInfoMsg = "";
}

void authenticate() {
  String  authUri = gatewayHost+"/auth/auth/oauth/token";
  
  HTTPClient http;
  Serial.println("");
  Serial.println("Authenticate ...");
  Serial.println(authUri);
  http.begin(authUri);
  http.addHeader("Authorization", "Basic c3ByaW5nY2xvdWR0ZW1wOnNjdF9zZWNyZXQ=");
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int httpCode = http.POST("grant_type=password&username=device&password=test&scope=deviceclient");
  String payload = http.getString();
  http.end();
 // Serial.println("");
  //Serial.print("RESPONSE: ");
  //Serial.print(payload);

  JsonObject& root = jsonBuffer.parseObject(payload);
  if (!root.success()) {
    Serial.println("Authentication problem.Canot get token object from authentication resposne.");
    return;
  }

  access_token = root["access_token"];
  Serial.println("");
  Serial.println("access_token: ");
  Serial.println(access_token);
  authorization="Bearer";
  authorization+=access_token;

  authenticated = true;
}




void registerDeviceActivity() {

  String registerUri = gatewayHost + "/api/device-reg/v1/devices/register-activity/"+ MAC_char;
  HTTPClient http;
  Serial.println("");
  Serial.println("RegisterDeviceActivity ...");
  Serial.println(registerUri);

  http.begin(registerUri);
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization",  authorization);

  int httpCode = http.PUT(registerRequestBody);
  String payload = http.getString();

  Serial.println("");
  Serial.print("RESPONSE: ");
  Serial.print(payload);
  http.end();

}

void registerDevice() {

   String registerUri = gatewayHost+ "/api/device-reg/v1/devices/register";
  HTTPClient http;
  Serial.println("");
  Serial.println("RegisterDevice ...");
  Serial.println(registerUri);

  http.begin(registerUri);
   
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  
  http.addHeader("Authorization",authorization);

  int httpCode = http.POST(registerRequestBody);
  String payload = http.getString();

  Serial.println("");
  Serial.print("RESPONSE: ");
  Serial.print(payload);
  http.end();

}

void registerDeviceExpiredHanlder(MillisTimer &mt) {
  registerDeviceActivity();
}




String getDeviceOutputStates() {
  String deviceStatesUri = gatewayHost + "/api/device-state/v1/device-state/" + MAC_char;

  HTTPClient http;
  Serial.println("");
  Serial.println("Get device states...");
  Serial.println(deviceStatesUri);

  http.begin(deviceStatesUri);
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", authorization);

  int httpCode = http.GET();
  String payload = http.getString();
  http.end();
  return payload;
}

void  setDeviceOutputStates(String json) {
  Serial.println("");
  Serial.println("Start parsing json response...");
  Serial.println(json);
  
  JsonArray& rootArray = jsonBuffer.parseArray(json);
  int arraySize=rootArray.size();
  Serial.println(rootArray.size()); //0   
  rootArray.prettyPrintTo(Serial); //[]
  
  for (int i = 0; i < arraySize; i++) {  
    
      JsonObject& arrayElement = rootArray[i];
      uint8_t pin=arrayElement["pinNumber"];
      boolean activate= arrayElement["activated"];
      pinMode(pin, OUTPUT);
      digitalWrite(pin, activate ? HIGH : LOW);
  }


}

void readStatesExpiredHanlder(MillisTimer &mt){
   setDeviceOutputStates(getDeviceOutputStates());
}

void refreshStates(){

  setDeviceOutputStates(getDeviceOutputStates());
  server.send(200, "text/plain", "STATES UPDATED FROM DB" );
}


void setMacAddress() {
  uint8_t MAC_array[6];
  WiFi.macAddress(MAC_array);
  for (int i = 0; i < sizeof(MAC_array); ++i) {
    sprintf(MAC_char, "%s%02x:", MAC_char, MAC_array[i]);
  }
  Serial.println(MAC_char);
}

void stopregisterTimer() {
  registerTimer.stop();
  server.send(200, "text/plain", "STOP ACTIVITY REGISTRATION" );

}

void startregisterTimer() {
  registerTimer.start();
  server.send(200, "text/plain", "START ACTIVITY REGISTRATION" );
}

void setRegisterRequestBody() {

  JsonObject& root = jsonBuffer.createObject();
  root["deviceId"] = MAC_char;
  root["ipAddress"] = WiFi.localIP().toString();
  root["listeningPort"] = "80";
  root["comment"] = "Some additional inforation about device";
  root.printTo(registerRequestBody);
  Serial.println("");
  Serial.println("Register request body : ");
  Serial.println(registerRequestBody);
}

void sendRfidData(String uidTag){
  
  String serviceDataUri = gatewayHost + "/api/device-data/v1/rfid-data";
  HTTPClient http;
  Serial.println("");
  Serial.println("Send RFID DATA...");
  Serial.println(serviceDataUri);

  http.begin(serviceDataUri);
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", authorization);
  

  String requestBody="";

  JsonObject& root = jsonBuffer.createObject();
  root["deviceId"] = MAC_char;
  root["uidTag"] = uidTag;
  root.printTo(requestBody);
  
  int httpCode = http.POST(requestBody);
  String payload = http.getString();

  Serial.println("");
  Serial.print("RESPONSE: ");
  Serial.print(payload);
  http.end();
  
}

void readRfidData() {
  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) 
  {
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) 
  {
    return;
  }
  //Show UID on serial monitor
  Serial.println();
  Serial.print(" UID tag :");
  String content= "";
  byte letter;
  for (byte i = 0; i < mfrc522.uid.size; i++) 
  {
     content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  content.toUpperCase();
  sendRfidData(content.substring(1));
  
  delay(2000);
}


void setup() {
  Serial.begin(115200);   // Initiate a serial communication  
  
  SPI.begin();           // Initiate  SPI bus
  mfrc522.PCD_Init();   // Initiate MFRC522

  
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.print("Device IP Adress: ");
  Serial.print(WiFi.localIP());
  setMacAddress();
  setRegisterRequestBody();

  server.on("/info", info);
  server.on("/reg-start", startregisterTimer);
  server.on("/reg-stop", stopregisterTimer);
  server.on("/refresh-states",refreshStates);
  server.begin();

  registerTimer.setInterval(1800000);
  registerTimer.expiredHandler(registerDeviceExpiredHanlder);

  readStatesTimer.setInterval(60000);
  readStatesTimer.expiredHandler(readStatesExpiredHanlder);

  authenticate();
  registerDevice();
  setDeviceOutputStates(getDeviceOutputStates());
}




void loop() {
    server.handleClient();
    registerTimer.run();
    readRfidData();
}

