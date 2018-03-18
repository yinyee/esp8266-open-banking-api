#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

#define USE_SERIAL Serial

ESP8266WebServer server(80);

String ip, tokenString, url;
String fingerprint =      "FF 06 FE 77 07 29 24 76 CD F1 9F 65 4A 58 8B 42 27 6C 22 56";
String proxyFingerprint = "E0 5C 97 B1 16 70 B4 B0 CA 75 D5 E1 80 46 B4 BB FB 3F 57 31";
String content_type = "application/json";
String authorization = "DirectLogin username=\"kosmond\", password=\"wezirpa6aAwezirpa\", consumer_key=\"5jyvepcncuxqexe4algfhndws1dvlyfgzmne1qiw\"";
String baseUrl = "https://santander.openbankproject.com/obp/v3.0.0/";
String proxyUrl = "https://santandeverywhere.factern.com/";
String proxyUrlHttp = "http://santandeverywhere.factern.com/";
String ownBankIdString = "santander.01.uk.sanuk";
String ownAccountIdString = "asdf0123";
String ownId = "car";
String auth_tokenised = "DirectLogin token=\"";
String hardToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyIiOiIifQ.PJ_ORqCJKK6zbiAwu0LPm-kR1sMdU-OJnd7MIkjCHWk";
// curl -H "Content-Type: application/json" -X POST http://10.1.107.32 -d '{"bank_id":"santander.01.uk.sanuk", "account_id":"mirw-1234", "currency": "GBP", "amount": "3"}'

void setup() {

  USE_SERIAL.begin(115200);
  // USE_SERIAL.setDebugOutput(true);

  pinMode(LED_BUILTIN, OUTPUT);
  
  WiFi.begin("Huckletree", "staycurious16");
//  WiFiMulti.addAP("Huckletree Guest");
  
  while (WiFi.status() != WL_CONNECTED) { 
//    Serial.printf("Wi-Fi mode set to WIFI_STA %s\n", WiFi.mode(WIFI_STA) ? "" : "Failed!");
//    Serial.printf("Connection status: %d\n", WiFiMulti.status());
//    WiFi.printDiag(Serial);
    delay(5000);
    Serial.println("Waiting for connection...");
  }

  ip = WiFi.localIP().toString();
  Serial.println(ip);
  String ipString = "http://ec2-54-205-16-115.compute-1.amazonaws.com:81/ip/" + ownId;
  HTTPClient httpClient;
  httpClient.begin(ipString);
  int httpCode = -99;
  while(httpCode < 0) {
    httpCode = httpClient.POST(ip);
    Serial.println("POST " + ipString + ": " + httpCode + " " + httpClient.errorToString(httpCode) + " " + httpClient.getString());
  }
  
  if (!MDNS.begin("iot-device")) {
    Serial.println("Error setting up MDNS responder!");
    while(1) { 
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

//  server.on("/", GET, handleRoot);
//  server.on("/", POST, handleTransactionRequest);
  server.on("/", handleTransactionRequest);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("HTTP server started");
  
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
  
  auth_tokenised += hardToken + "\"";
  
}

void handleRoot() {
  server.send(200, "text/plain", "hello from iot-device!");
}

void handleTransactionRequest() {

  digitalWrite(LED_BUILTIN, LOW); 
  Serial.println(server.arg("plain"));
  
  JsonObject& root = parseJson(server.arg("plain"));
  const char* payeeBankId = root["bank_id"];
  const char* payeeAccountId = root["account_id"];
  const char* currency = root["currency"];
  const char* amount = root["amount"];

  String payeeBankIdString(payeeBankId);
  String payeeAccountIdString(payeeAccountId);
  String currencyString(currency);
  String amountString(amount);

  if(payeeBankIdString == "") {
    server.send(200, "text/plain", "Missing: bank_id");
    return;
  }
  if(payeeAccountIdString == "") {
    server.send(200, "text/plain", "Missing: account_id");
    return;
  }
  
  if(currencyString == "") {
    server.send(200, "text/plain", "Missing: currency");
    return;
  }

  if(amountString == "") {
    server.send(200, "text/plain", "Missing: amount");
    return;
  }

  Serial.println("Pay " + currencyString + amountString + " to " + payeeBankIdString + " " + payeeAccountIdString);
  server.send(200, "text/plain", makePayment(payeeBankIdString, payeeAccountIdString, currencyString, amountString));
  digitalWrite(LED_BUILTIN, HIGH); 
  
}

void handleNotFound() {
  String message = "File Not Found HELP ME\n\n";
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
  url = proxyUrlHttp + "my/banks/" + ownBankIdString + "/accounts/" + ownAccountIdString + "/" + ownId + "/account";
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

String makePayment(String payeeBankId, String payeeAccountId, String currency, String amount) {
  
  Serial.println("=== Make Payment ===");
  
  HTTPClient httpClient;

  url = proxyUrlHttp + "banks/" + ownBankIdString + "/accounts/" + ownAccountIdString + "/" + ownId + "/owner/transaction-request-types/SANDBOX_TAN/transaction-requests";
  httpClient.begin(url);
//  url = baseUrl + "banks/" + ownBankIdString + "/accounts/" + ownAccountIdString + "/owner/transaction-request-types/SANDBOX_TAN/transaction-requests";
//  httpClient.begin(url, fingerprint);
  Serial.println(url);
  
  httpClient.addHeader("Content-Type", content_type);
  httpClient.addHeader("Authorization", auth_tokenised);
  DynamicJsonBuffer jsonBuffer;
  JsonObject& transaction = jsonBuffer.createObject();
  JsonObject& to = transaction.createNestedObject("to");
  to["bank_id"] = payeeBankId;
  to["account_id"] = payeeAccountId;
  JsonObject& value = transaction.createNestedObject("value");
  value["currency"] = currency;
  value["amount"] = amount;
  transaction["description"] = "Payment from iot-device.local";
  String transactionString;
  transaction.printTo(transactionString);
  Serial.println(transactionString);
  int httpCode = httpClient.POST(transactionString);
  String response = httpClient.getString();
  Serial.println(httpCode);
  Serial.println(httpClient.errorToString(httpCode));
  Serial.println(response);

  JsonObject& responseRoot = parseJson(response);
  const char* responseStatus = responseRoot["status"];
  String statusString(responseStatus);
  if(statusString == "SUCCESS") {
    const char* jwt = responseRoot["jwt"];
    return String(jwt);
  } else if(statusString == "FAILED") {
    const char* error = responseRoot["error"];
    return String(error);
  } else {
    return String("DUMMY RESPONSE");
  }

  httpClient.end();
}

void loop() {
  
  server.handleClient();
  Serial.println("looping...");
  delay(10000);
  
}

JsonObject& parseJson(String input) {
  
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(input);
  if(!root.success()) {
    Serial.println("Parsing failed");
  }
  return root;
  
}
