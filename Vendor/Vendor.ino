#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

#define USE_SERIAL Serial

ESP8266WebServer server(80);

String tokenString, url;
String fingerprint =      "FF 06 FE 77 07 29 24 76 CD F1 9F 65 4A 58 8B 42 27 6C 22 56";
String proxyFingerprint = "E0 5C 97 B1 16 70 B4 B0 CA 75 D5 E1 80 46 B4 BB FB 3F 57 31";
String content_type = "application/json";
String authorization = "DirectLogin username=\"kosmond\", password=\"wezirpa6aAwezirpa\", consumer_key=\"5jyvepcncuxqexe4algfhndws1dvlyfgzmne1qiw\"";
String baseUrl = "https://santander.openbankproject.com/obp/v3.0.0/";
String proxyUrl = "https://santandeverywhere.factern.com/";
String proxyUrlHttp = "http://santandeverywhere.factern.com/";
String ownBankIdString = "santander.01.uk.sanuk";
String ownAccountIdString = "mirw-1234";
String auth_tokenised = "DirectLogin token=\"";
String hardToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyIiOiIifQ.PJ_ORqCJKK6zbiAwu0LPm-kR1sMdU-OJnd7MIkjCHWk";

byte input = 0;
byte input_debounce = 20;
int count;

void setup() {

  count = 0;
  USE_SERIAL.begin(115200);
  // USE_SERIAL.setDebugOutput(true);
  
  pinMode(5, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  
  WiFi.begin("Huckletree", "staycurious16");
  
  while (WiFi.status() != WL_CONNECTED) { 
    delay(5000);
    Serial.println("Waiting for connection");
  }
  
  if (!MDNS.begin("vendor")) {
    Serial.println("Error setting up MDNS responder!");
    while(1) { 
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

//  server.on("/", GET, handleRoot);
//  server.on("/", POST, handleTransactionRequest);
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("HTTP server started");
  
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
  
  auth_tokenised += hardToken + "\"";
  
}

void handleRoot() {
  server.send(200, "text/plain", "hello from vendor!");
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void senseObstacle(int count) {

  if(count % 1000 == 0) {
    Serial.printf("State %d\n", input);
  }
  
  byte new_input = 1 - digitalRead(5);
  if (input != new_input) {
    if (input_debounce > 0) {
      input_debounce--;
    } else {
      input = new_input;
      digitalWrite(LED_BUILTIN, 1 - input);
      Serial.printf("State now %d\n", input);
      if(input == 1) {
        requestPayment();
      }
      input_debounce = 20;
    }
  } else {
    input_debounce = 20;
  }
}

String login() {
  
  Serial.println("=== Log in ===");
  
  HTTPClient http;
  http.begin("https://santander.openbankproject.com/my/logins/direct", fingerprint);
  http.addHeader("Content-Type", content_type);
  http.addHeader("Authorization", authorization);
  int httpCode = http.POST("");
  Serial.println(httpCode);
  Serial.println(http.errorToString(httpCode));
  Serial.println(http.getString());
  
  // Get token
  JsonObject& getLoginRoot = parseJson(http.getString());
  const char* token = getLoginRoot["token"];
  tokenString = String(token);
  Serial.println("token: " + tokenString);
  return tokenString;
}

JsonObject& checkBalance() {

  Serial.println("=== Check account balance ===");
  url = proxyUrlHttp + "my/banks/" + ownBankIdString + "/accounts/" + ownAccountIdString + "/account";
  Serial.println(url);

  HTTPClient http;
  http.begin(url, proxyFingerprint);
  http.begin(url);
  http.addHeader("Authorization", auth_tokenised);
  int httpCode = http.GET();
  String response = http.getString();
  Serial.println(httpCode);
  Serial.println(http.errorToString(httpCode));
  Serial.println(response);
  http.end();

//  // Get account details
//  JsonObject& getAccountRoot = parseJson(response);
//  const char* currency = getAccountRoot["balance"]["currency"];
//  String currencyString(currency);
//  const char* balance = getAccountRoot["balance"]["amount"];
//  String balanceString(balance);
//  Serial.println("balance: " + currencyString + " " + balanceString);

  return parseJson(response);
}

String getIpAddress() {

  Serial.println("=== Get IP Address ===");
  
  String response;
  String ipString = "http://ec2-54-205-16-115.compute-1.amazonaws.com:81/ip/car";
  HTTPClient httpClient;
  httpClient.begin(ipString);
  int httpCode = -99;
  while(httpCode < 0) {
    httpCode = httpClient.GET();
    response = httpClient.getString();
    Serial.println("GET " + ipString + ": " + httpCode + " " + httpClient.errorToString(httpCode) + " " + response);
  }
  return response;
}

void requestPayment() {
  
  Serial.println("=== Request Payment ===");
  
  HTTPClient httpClient;
  url = "http://" + getIpAddress() + "/";
  httpClient.begin(url);
//  url = baseUrl + "banks/" + ownBankIdString + "/accounts/" + ownAccountIdString + "/owner/transaction-request-types/SANDBOX_TAN/transaction-requests";
//  httpClient.begin(url, fingerprint);
  Serial.println("url:" + url);
  
  httpClient.addHeader("Content-Type", content_type);
  DynamicJsonBuffer jsonBuffer;
  JsonObject& transaction = jsonBuffer.createObject();
  transaction["bank_id"] = ownBankIdString;
  transaction["account_id"] = ownAccountIdString;
  transaction["currency"] = "GBP";
  transaction["amount"] = "2.20";
  String transactionString;
  transaction.printTo(transactionString);
  Serial.println(transactionString);
  int httpCode = httpClient.POST(transactionString);
  String response = httpClient.getString();
  Serial.println(httpCode);
  Serial.println(httpClient.errorToString(httpCode));
  Serial.println(response);
  if(response != "LIMIT REACHED" && response != "DUMMY RESPONSE") {
    // blink vigorously
  }
  httpClient.end();
}

void loop() {

  server.handleClient();
  senseObstacle(count);
  delay(10);

  count++;
  if(count % 1000 == 0) {
    Serial.println("looping...");
  }
  
}

JsonObject& parseJson(String input) {
  
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(input);
  if(!root.success()) {
    Serial.println("Parsing failed");
  }
  return root;
  
}
