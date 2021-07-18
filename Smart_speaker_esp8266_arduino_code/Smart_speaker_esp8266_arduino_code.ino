//========================================== laberarys===========================================================================
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <WebSocketsServer.h>
#include <FastLED.h>

//========================================== set up ==============================================================================
#define LED_COUNT 17 
#define LED_DT 3 

uint8_t idex = 0; 
uint8_t ihue = 0;
uint8_t saturationVal = 255; 
uint8_t ibright = 0;
uint16_t TOP_INDEX = uint8_t(LED_COUNT / 2); 
uint8_t EVENODD = LED_COUNT % 2; 
uint8_t bouncedirection = 0; 
int ledsX[LED_COUNT][3]; 
 
uint8_t ledMode = 0;
uint8_t bright = 25; 
uint8_t flag = 1; 
uint8_t delayValue = 20; 
uint8_t stepValue = 10; 
uint8_t hueValue = 0; 

const char *ssid = "leds"; // The name of the Wi-Fi network that will be created
const char *password = "123456789"; // The password required to connect to it, leave blank for an open network
const char *OTAName = "leds"; // A name and a password for the OTA service
const char *OTAPassword = "esp8266";
const char* mdnsName = "led"; // Domain name for the mDNS responder


IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

CRGBArray<LED_COUNT> leds;
ESP8266WebServer server(80); // create a web server on port 80
WebSocketsServer webSocket = WebSocketsServer(81); // create a websocket server on port 81
File fsUploadFile; // a File variable to temporarily store the received file

//========================================== setup ==============================================================================
void setup() {
  Serial.begin(115200); // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println("\r\n");
  
  start_ap_WiFi(); // Start a Wi-Fi access point, and try to connect to some given access points. Then wait for either an
  startOTA(); // Start the OTA service
  startSPIFFS(); // Start the SPIFFS and list all contents
  startWebSocket(); // Start a WebSocket server
  startMDNS(); // Start the mDNS responder
  startServer(); // Start a HTTP server with a file read handler and an upload handler
  startled();
}

//============================================== loop ====================================================================

void loop() {
  ledEffect(ledMode);
  webSocket.loop(); // constantly check for websocket events
  server.handleClient(); // run the server
  ArduinoOTA.handle();
}

//============================================== AP wifi service ====================================================================
void start_ap_WiFi() { // Start Wi-Fi access point, and try to connect to some given access points. Then wait for either an AP or STA connection
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password); // Start the access point
  WiFi.softAPConfig(local_ip, gateway, subnet);
  Serial.print("Access Point \""); Serial.print(ssid); Serial.println("\" started\r\n");
  delay(250);
  Serial.print('.');
}

//=============================================== OTA service ==================================================================
void startOTA() { // Start the OTA service
  ArduinoOTA.setHostname(OTAName);
  ArduinoOTA.setPassword(OTAPassword);
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\r\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready\r\n");
}
//=========================================== mdns service =====================================================================

void startMDNS() { // Start the mDNS responder
  MDNS.begin(mdnsName); // start the multicast domain name server
  Serial.print("mDNS responder started: http://");
  Serial.print(mdnsName);
  Serial.println(".local");
}

//=========================================== led service =====================================================================

void startled(){
  Serial.begin(9600); 
  LEDS.setBrightness(bright);
  LEDS.addLeds<WS2811, LED_DT, GRB>(leds, LED_COUNT);  
  updateColor(0,0,0);
  LEDS.show(); 
}

//=========================================== web server =======================================================================

void startServer() { // Start a HTTP server with a file read handler and an upload handler
  server.on("/edit.html", HTTP_POST, []() { // If a POST request is sent to the /edit.html address,
    server.send(200, "text/plain", "");
  }, handleFileUpload); // go to 'handleFileUpload'
  server.onNotFound(handleNotFound); // if someone requests any other file or page, go to function 'handleNotFound'
  // and check if the file exists
  server.begin(); // start the HTTP server
  Serial.println("HTTP server started.");
}
void handleNotFound() { // if the requested file or page doesn't exist, return a 404 not found error
  if (!handleFileRead(server.uri())) { // check if the file exists in the flash memory (SPIFFS), if so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
}

//=============================================== SPIFFS ======================================================================

void startSPIFFS() { // Start the SPIFFS and list all contents
  SPIFFS.begin(); // Start the SPI Flash File System (SPIFFS)
  Serial.println("SPIFFS started. Contents:");
}



String getContentType(String filename) {
  if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html"; // If a folder is requested, send the index file
  String contentType = getContentType(path); // Get the MIME type
  if (SPIFFS.exists(path)) { // If the file exists
    File file = SPIFFS.open(path, "r"); // Open it
    size_t sent = server.streamFile(file, contentType); // And send it to the client
    file.close(); // Then close the file again
    return true;
  }

  Serial.println("\tFile Not Found");
  return false; // If the file doesn't exist, return false
}

void handleFileUpload() { // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  String path;
  if (upload.status == UPLOAD_FILE_START) {
    path = upload.filename;
    if (!path.startsWith("/")) path = "/" + path;
    if (!path.endsWith(".gz")) { // The file server always prefers a compressed version of a file
      String pathWithGz = path + ".gz"; // So if an uploaded file is not compressed, the existing compressed
      if (SPIFFS.exists(pathWithGz)) // version of that file must be deleted (if it exists)
        SPIFFS.remove(pathWithGz);
    }
    Serial.print("handleFileUpload Name: "); Serial.println(path);
    fsUploadFile = SPIFFS.open(path, "w"); // Open the file for writing in SPIFFS (create if it doesn't exist)
    path = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) { // If the file was successfully created
      fsUploadFile.close(); // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.sendHeader("Location", "/success.html"); // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}


//============================================== WEBSOCKET  =====================================================================

void startWebSocket() { // Start a WebSocket server
  webSocket.begin(); // start the websocket server
  webSocket.onEvent(webSocketEvent); // if there's an incomming websocket message, go to function 'webSocketEvent'
  Serial.println("WebSocket server started.");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) { // When a WebSocket message is received
  switch (type) {
    case WStype_DISCONNECTED: // if the websocket is disconnected
      Serial.printf("[%u] Disconnected!\n", num);
      break;

    case WStype_CONNECTED: { // if a new websocket connection is established
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      }
      break;

    case WStype_TEXT: // if new text data is received
      Serial.printf("[%u] get Text: %s\n", num, payload);
      String msg = String((char *)payload);
      handle_msg(msg,num);
      break;
  }
}
  
//============================================== JSON  =====================================================================

void handle_msg(String &payload ,uint8_t mac) {
  DynamicJsonDocument jsonBuffer(JSON_OBJECT_SIZE(5) + 100);
  DeserializationError error = deserializeJson(jsonBuffer, payload);
  if (error) {
    Serial.print(F("parseObject() failed: "));
    Serial.println(error.c_str());
  }
  JsonObject root = jsonBuffer.as<JsonObject>();
  String action = jsonBuffer["action"];
  
  if (action=="mode" ){   
     ledMode = jsonBuffer["mode"];
     flag = 0;
     Serial.print("Led mode: "+ledMode);
     ledEffect(ledMode);
    
  }else if (action=="color") {
      if(!flag){
         Serial.print("flag : ");
         Serial.println(flag);
         ledMode=0;
         ledEffect(ledMode);
         flag = 1;
      }else{
         uint8_t r = jsonBuffer["r"];
         uint8_t g = jsonBuffer["g"];
         uint8_t b = jsonBuffer["b"];
         Serial.print("ColorPicker: "+r+g+b);
         for(int x = 0; x < LED_COUNT; x++){
           leds[x].setRGB(r,g,b);
         }
         LEDS.show();   
       }
       
  }else if (action=="bright") {
    int bright = jsonBuffer["bright"];     
    flag = 0;
    Serial.print("Bright: "+bright);
    LEDS.setBrightness(bright);
  }
}
//============================================== led effects  =====================================================================

void ledEffect(uint8_t Mode){
   switch(Mode){
      case 0: updateColor(0,0,0); break;
      case 1: rainbow_fade(); delayValue = 20; break;       
      case 2: rainbow_loop(); delayValue = 20; break;
      case 3: new_rainbow_loop(); delayValue = 5; break;
      case 4: random_march(); delayValue = 40; break;  
      case 5: rgb_propeller(); delayValue = 25; break;
      case 6: rotatingRedBlue(); delayValue = 40; hueValue = 0; break;
      case 7: Fire(55, 120, delayValue); delayValue = 15; break; 
      case 8: blueFire(55, 250, delayValue); delayValue = 15; break;  
      case 9: random_burst(); delayValue = 20; break;
      case 10: flicker(); delayValue = 20; break;
      case 11: random_color_pop(); delayValue = 35; break;                                      
      case 12: Sparkle(255, 255, 255, delayValue); delayValue = 0; break;                   
      case 13: color_bounce(); delayValue = 20; hueValue = 0; break;
      case 14: color_bounceFADE(); delayValue = 40; hueValue = 0; break;
      case 15: red_blue_bounce(); delayValue = 40; hueValue = 0; break;
      case 16: rainbow_vertical(); delayValue = 50; stepValue = 15; break;
      case 17: matrix(); delayValue = 50; hueValue = 95; break; 
      case 18: rwb_march(); delayValue = 80; break;                         
      case 19: flame(); break;
      case 20: theaterChase(255, 0, 0, delayValue); delayValue = 50; break;
      case 21: Strobe(255, 255, 255, 10, delayValue, 1000); delayValue = 100; break;
      case 22: policeBlinker(); delayValue = 25; break;
      case 23: kitt(); delayValue = 100; break;
      case 24: rule30(); delayValue = 100; break;
      case 25: fade_vertical(); delayValue = 60; hueValue = 180; break;
      case 26: fadeToCenter(); break;
      case 27: runnerChameleon(); break;
      case 28: blende(); break;
      case 29: blende_2();break;
    }
}


void updateColor(uint8_t r,uint8_t g,uint8_t b){
  for(uint8_t i = 0 ; i < LED_COUNT; i++ ){
    leds[i].setRGB(r,g,b);
  }
}


void rainbow_fade(){ 
  ihue++;
  if(ihue > 255){
    ihue = 0;
  }
  for(int idex = 0 ; idex < LED_COUNT; idex++ ){
    leds[idex] = CHSV(ihue, saturationVal, 255);
  }
  LEDS.show();
  delay(delayValue);
}


void rainbow_loop(){ 
  idex++;
  ihue = ihue + stepValue;
  if(idex >= LED_COUNT){
    idex = 0;
  }
  if(ihue > 255){
    ihue = 0;
  }
  leds[idex] = CHSV(ihue, saturationVal, 255);
  LEDS.show();
  delay(delayValue);
}


void random_burst(){
  idex = random(0, LED_COUNT);
  ihue = random(0, 255);
  leds[idex] = CHSV(ihue, saturationVal, 255);
  LEDS.show();
  delay(delayValue);
}


void color_bounce(){
  if(bouncedirection == 0){
    idex = idex + 1;
    if(idex == LED_COUNT){
      bouncedirection = 1;
      idex = idex - 1;
    }
  }
  if(bouncedirection == 1){
    idex = idex - 1;
    if(idex == 0){
      bouncedirection = 0;
    }
  }
  for(int i = 0; i < LED_COUNT; i++ ){
    if(i == idex){
      leds[i] = CHSV(hueValue, saturationVal, 255);
    }
    else{
      leds[i] = CHSV(0, 0, 0);
    }
  }
  LEDS.show();
  delay(delayValue);
}


void color_bounceFADE(){
  if(bouncedirection == 0){
    idex = idex + 1;
    if(idex == LED_COUNT){
      bouncedirection = 1;
      idex = idex - 1;
    }
  }
  if(bouncedirection == 1){
    idex = idex - 1;
    if(idex == 0){
      bouncedirection = 0;
    }
  }
  int iL1 = adjacent_cw(idex);
  int iL2 = adjacent_cw(iL1);
  int iL3 = adjacent_cw(iL2);
  int iR1 = adjacent_ccw(idex);
  int iR2 = adjacent_ccw(iR1);
  int iR3 = adjacent_ccw(iR2);

  for(int i = 0; i < LED_COUNT; i++ ){
    if(i == idex){
      leds[i] = CHSV(hueValue, saturationVal, 255);
    }
    else if(i == iL1){
      leds[i] = CHSV(hueValue, saturationVal, 150);
    }
    else if(i == iL2){
      leds[i] = CHSV(hueValue, saturationVal, 80);
    }
    else if(i == iL3){
      leds[i] = CHSV(hueValue, saturationVal, 20);
    }
    else if(i == iR1){
      leds[i] = CHSV(hueValue, saturationVal, 150);
    }
    else if(i == iR2){
      leds[i] = CHSV(hueValue, saturationVal, 80);
    }
    else if(i == iR3){
      leds[i] = CHSV(hueValue, saturationVal, 20);
    }
    else{
      leds[i] = CHSV(0, 0, 0);
    }
  }
  LEDS.show();
  delay(delayValue);
}

// вращение красного и синего
void red_blue_bounce(){
  idex++;
  if(idex >= LED_COUNT){
    idex = 0;
  }
  int idexR = idex;
  int idexB = antipodal_index(idexR);
  int thathue =(hueValue + 160) % 255;

  for(int i = 0; i < LED_COUNT; i++ ){
    if(i == idexR){
      leds[i] = CHSV(hueValue, saturationVal, 255);
    }
    else if(i == idexB){
      leds[i] = CHSV(thathue, saturationVal, 255);
    }
    else{
      leds[i] = CHSV(0, 0, 0);
    }
  }
  LEDS.show();
  delay(delayValue);
}

int antipodal_index(int i){
  int iN = i + TOP_INDEX;
  if(i >= TOP_INDEX){
    iN =( i + TOP_INDEX ) % LED_COUNT;
  }
  return iN;
}

// вращение красного/синего
void rotatingRedBlue(){
  idex++;
  if(idex >= LED_COUNT){
    idex = 0;
  }
  int idexR = idex;
  int idexB = antipodal_index(idexR);
  int thathue =(hueValue + 160) % 255;
  leds[idexR] = CHSV(hueValue, saturationVal, 255);
  leds[idexB] = CHSV(thathue, saturationVal, 255);
  LEDS.show();
  delay(delayValue);
}


void flicker(){
  int random_bright = random(0, 255);
  int random_delay = random(10, 100);
  int random_bool = random(0, random_bright);
  if(random_bool < 10){
    for(int i = 0 ; i < LED_COUNT; i++ ){
      leds[i] = CHSV(160, 50, random_bright);
    }
    LEDS.show();
    delay(random_delay);
  }
}


void fade_vertical(){
  idex++;
  if(idex > TOP_INDEX){
    idex = 0;
  }
  int idexA = idex;
  int idexB = horizontal_index(idexA);
  ibright = ibright + 10;
  if(ibright > 255){
    ibright = 0;
  }
  leds[idexA] = CHSV(hueValue, saturationVal, ibright);
  leds[idexB] = CHSV(hueValue, saturationVal, ibright);
  LEDS.show();
  delay(delayValue);
}


int horizontal_index(int i){
  if(i == 0){
    return 0;
  }
  if(i == TOP_INDEX && EVENODD == 1){
    return TOP_INDEX + 1;
  }
  if(i == TOP_INDEX && EVENODD == 0){
    return TOP_INDEX;
  }
  return LED_COUNT - i;
}


void random_red(){
  int temprand;
  for(int i = 0; i < LED_COUNT; i++ ){
    temprand = random(0, 100);
    if(temprand > 50){
      leds[i].r = 255;
    }
    if(temprand <= 50){
      leds[i].r = 0;
    }
    leds[i].b = 0; leds[i].g = 0;
  }
  LEDS.show();
}


void rule30(){
  if(bouncedirection == 0){
    random_red();
    bouncedirection = 1;
  }
  copy_led_array();
  int iCW;
  int iCCW;
  int y = 100;
  for(int i = 0; i < LED_COUNT; i++ ){
    iCW = adjacent_cw(i);
    iCCW = adjacent_ccw(i);
    if(ledsX[iCCW][0] > y && ledsX[i][0] > y && ledsX[iCW][0] > y){
      leds[i].r = 0;
    }
    if(ledsX[iCCW][0] > y && ledsX[i][0] > y && ledsX[iCW][0] <= y){
      leds[i].r = 0;
    }
    if(ledsX[iCCW][0] > y && ledsX[i][0] <= y && ledsX[iCW][0] > y){
      leds[i].r = 0;
    }
    if(ledsX[iCCW][0] > y && ledsX[i][0] <= y && ledsX[iCW][0] <= y){
      leds[i].r = 255;
    }
    if(ledsX[iCCW][0] <= y && ledsX[i][0] > y && ledsX[iCW][0] > y){
      leds[i].r = 255;
    }
    if(ledsX[iCCW][0] <= y && ledsX[i][0] > y && ledsX[iCW][0] <= y){
      leds[i].r = 255;
    }
    if(ledsX[iCCW][0] <= y && ledsX[i][0] <= y && ledsX[iCW][0] > y){
      leds[i].r = 255;
    }
    if(ledsX[iCCW][0] <= y && ledsX[i][0] <= y && ledsX[iCW][0] <= y){
      leds[i].r = 0;
    }
  }
  LEDS.show();
  delay(delayValue);
}
int adjacent_cw(int i){
  int r;
  if(i < LED_COUNT - 1){
    r = i + 1;
  }else{
    r = 0;
  }
  return r;
}
int adjacent_ccw(int i){
  int r;
  if(i > 0){
    r = i - 1;
  }
  else{
    r = LED_COUNT - 1;
  }
  return r;
}


void random_march(){
  copy_led_array();
  int iCCW;
  leds[0] = CHSV(random(0, 255), 255, 255);
  for(int idex = 1; idex < LED_COUNT ; idex++ ){
    iCCW = adjacent_ccw(idex);
    leds[idex].r = ledsX[iCCW][0];
    leds[idex].g = ledsX[iCCW][1];
    leds[idex].b = ledsX[iCCW][2];
  }
  LEDS.show();
  delay(delayValue);
}
void copy_led_array(){
  for(int i = 0; i < LED_COUNT; i++ ){
    ledsX[i][0] = leds[i].r;
    ledsX[i][1] = leds[i].g;
    ledsX[i][2] = leds[i].b;
  }
}


void rwb_march(){ 
  copy_led_array();
  int iCCW;
  idex++;
  if(idex > 2){
    idex = 0;
  }
  switch(idex){
    case 0:
      leds[0].r = 255;
      leds[0].g = 0;
      leds[0].b = 0;
      break;
    case 1:
      leds[0].r = 255;
      leds[0].g = 255;
      leds[0].b = 255;
      break;
    case 2:
      leds[0].r = 0;
      leds[0].g = 0;
      leds[0].b = 255;
      break;
  }
  for(int i = 1; i < LED_COUNT; i++ ){
    iCCW = adjacent_ccw(i);
    leds[i].r = ledsX[iCCW][0];
    leds[i].g = ledsX[iCCW][1];
    leds[i].b = ledsX[iCCW][2];
  }
  LEDS.show();
  delay(delayValue);
}


void flame(){
  int idelay = random(0, 35);
  float hmin = 0.1; float hmax = 45.0;
  float hdif = hmax - hmin;
  int randtemp = random(0, 3);
  float hinc =(hdif / float(TOP_INDEX)) + randtemp;
  int ihue = hmin;
  for(int i = 0; i <= TOP_INDEX; i++ ){
    ihue = ihue + hinc;
    leds[i] = CHSV(ihue, saturationVal, 255);
    int ih = horizontal_index(i);
    leds[ih] = CHSV(ihue, saturationVal, 255);
    leds[TOP_INDEX].r = 255; leds[TOP_INDEX].g = 255; leds[TOP_INDEX].b = 255;
    LEDS.show();
    delay(idelay);
  }
}


void rainbow_vertical(){ 
  idex++;
  if(idex > TOP_INDEX){
    idex = 0;
  }
  ihue = ihue + stepValue;
  if(ihue > 255){
    ihue = 0;
  }
  int idexA = idex;
  int idexB = horizontal_index(idexA);
  leds[idexA] = CHSV(ihue, saturationVal, 255);
  leds[idexB] = CHSV(ihue, saturationVal, 255);
  LEDS.show();
  delay(delayValue);
}


void random_color_pop(){
  idex = random(0, LED_COUNT);
  ihue = random(0, 255);
  updateColor(0, 0, 0);
  leds[idex] = CHSV(ihue, saturationVal, 255);
  LEDS.show();
  delay(delayValue);
}


void policeBlinker(){
  int hueValue = 0;
  int thathue =(hueValue + 160) % 255;
  for(int x = 0 ; x < 5; x++ ){
    for(int i = 0 ; i < TOP_INDEX; i++ ){
      leds[i] = CHSV(hueValue, saturationVal, 255);
    }
    LEDS.show(); delay(delayValue);
    updateColor(0, 0, 0);
    LEDS.show(); delay(delayValue);
  }
  for(int x = 0 ; x < 5; x++ ){
    for(int i = TOP_INDEX ; i < LED_COUNT; i++ ){
      leds[i] = CHSV(thathue, saturationVal, 255);
    }
    LEDS.show(); delay(delayValue);
    updateColor(0, 0, 0);
    LEDS.show(); delay(delayValue);
  }
}

void rgb_propeller(){ 
  idex++;
  int ghue =(hueValue + 80) % 255;
  int bhue =(hueValue + 160) % 255;
  int N3  = int(LED_COUNT / 3);
  int N6  = int(LED_COUNT / 6);
  int N12 = int(LED_COUNT / 12);

  for(int i = 0; i < N3; i++ ){
    int j0 =(idex + i + LED_COUNT - N12) % LED_COUNT;
    int j1 =(j0 + N3) % LED_COUNT;
    int j2 =(j1 + N3) % LED_COUNT;
    leds[j0] = CHSV(hueValue, saturationVal, 255);
    leds[j1] = CHSV(ghue, saturationVal, 255);
    leds[j2] = CHSV(bhue, saturationVal, 255);
  }
  LEDS.show();
  delay(delayValue);
}


void kitt(){
  int rand = random(0, TOP_INDEX);
  for(int i = 0; i < rand; i++ ){
    leds[TOP_INDEX + i] = CHSV(hueValue, saturationVal, 255);
    leds[TOP_INDEX - i] = CHSV(hueValue, saturationVal, 255);
    LEDS.show();
    delay(delayValue / rand);
  }
  for(int i = rand; i > 0; i-- ){
    leds[TOP_INDEX + i] = CHSV(hueValue, saturationVal, 0);
    leds[TOP_INDEX - i] = CHSV(hueValue, saturationVal, 0);
    LEDS.show();
    delay(delayValue / rand);
  }
}


void matrix(){
  int rand = random(0, 100);
  if(rand > 90){
    leds[0] = CHSV(hueValue, saturationVal, 255);
  }
  else{
    leds[0] = CHSV(hueValue, saturationVal, 0);
  }
  copy_led_array();
  for(int i = 1; i < LED_COUNT; i++ ){
    leds[i].r = ledsX[i - 1][0];
    leds[i].g = ledsX[i - 1][1];
    leds[i].b = ledsX[i - 1][2];
  }
  LEDS.show();
  delay(delayValue);
}


void new_rainbow_loop(){
  ihue -= 1;
  fill_rainbow( leds, LED_COUNT, ihue );
  LEDS.show();
  delay(delayValue);
}

void setPixel(int Pixel, byte red, byte green, byte blue){
  leds[Pixel].r = red;
  leds[Pixel].g = green;
  leds[Pixel].b = blue;
}
//служебная функция
void setAll(byte red, byte green, byte blue){
  for(int i = 0; i < LED_COUNT; i++ ){
    setPixel(i, red, green, blue);
  }
  FastLED.show();
}


void Fire(int Cooling, int Sparking, int SpeedDelay){
  static byte heat[LED_COUNT];
  int cooldown;

  for(int i = 0; i < LED_COUNT; i++){
    cooldown = random(0,((Cooling * 10) / LED_COUNT) + 2);

    if(cooldown > heat[i]){
      heat[i] = 0;
    } else{
      heat[i] = heat[i] - cooldown;
    }
  }

  for( int k = LED_COUNT - 1; k >= 2; k--){
    heat[k] =(heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  }

  if( random(255) < Sparking ){
    int y = random(7);
    heat[y] = heat[y] + random(160, 255);
  }

  for( int j = 0; j < LED_COUNT; j++){
    setPixelHeatColor(j, heat[j] );
  }

  FastLED.show();
  delay(SpeedDelay);
}

void setPixelHeatColor(int Pixel, byte temperature){
  byte t192 = round((temperature / 255.0) * 191);
  byte heatramp = t192 & 0x3F;
  heatramp <<= 2;

  if( t192 > 0x80){
    setPixel(Pixel, 255, 255, heatramp);
  } else if( t192 > 0x40 ){
    setPixel(Pixel, 255, heatramp, 0);
  } else{
    setPixel(Pixel, heatramp, 0, 0);
  }
}

void CenterToOutside(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay){
  for(int i =((LED_COUNT - EyeSize) / 2); i >= 0; i--){
    setAll(0, 0, 0);

    setPixel(i, red / 10, green / 10, blue / 10);
    for(int j = 1; j <= EyeSize; j++){
      setPixel(i + j, red, green, blue);
    }
    setPixel(i + EyeSize + 1, red / 10, green / 10, blue / 10);

    setPixel(LED_COUNT - i, red / 10, green / 10, blue / 10);
    for(int j = 1; j <= EyeSize; j++){
      setPixel(LED_COUNT - i - j, red, green, blue);
    }
    setPixel(LED_COUNT - i - EyeSize - 1, red / 10, green / 10, blue / 10);

    FastLED.show();
    delay(SpeedDelay);
  }
  delay(ReturnDelay);
}

void OutsideToCenter(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay){
  for(int i = 0; i <=((LED_COUNT - EyeSize) / 2); i++){
    setAll(0, 0, 0);

    setPixel(i, red / 10, green / 10, blue / 10);
    for(int j = 1; j <= EyeSize; j++){
      setPixel(i + j, red, green, blue);
    }
    setPixel(i + EyeSize + 1, red / 10, green / 10, blue / 10);

    setPixel(LED_COUNT - i, red / 10, green / 10, blue / 10);
    for(int j = 1; j <= EyeSize; j++){
      setPixel(LED_COUNT - i - j, red, green, blue);
    }
    setPixel(LED_COUNT - i - EyeSize - 1, red / 10, green / 10, blue / 10);

    FastLED.show();
    delay(SpeedDelay);
  }
  delay(ReturnDelay);
}

void LeftToRight(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay){
  for(int i = 0; i < LED_COUNT - EyeSize - 2; i++){
    setAll(0, 0, 0);
    setPixel(i, red / 10, green / 10, blue / 10);
    for(int j = 1; j <= EyeSize; j++){
      setPixel(i + j, red, green, blue);
    }
    setPixel(i + EyeSize + 1, red / 10, green / 10, blue / 10);
    FastLED.show();
    delay(SpeedDelay);
  }
  delay(ReturnDelay);
}


void Sparkle(byte red, byte green, byte blue, int SpeedDelay){
  int Pixel = random(LED_COUNT);
  setPixel(Pixel, red, green, blue);
  FastLED.show();
  delay(SpeedDelay);
  setPixel(Pixel, 0, 0, 0);
}


void theaterChase(byte red, byte green, byte blue, int SpeedDelay){
  for(int j = 0; j < 10; j++){
    for(int q = 0; q < 3; q++){
      for(int i = 0; i < LED_COUNT; i = i + 3){
        setPixel(i + q, red, green, blue);
      }
      FastLED.show();
      delay(SpeedDelay);
      for(int i = 0; i < LED_COUNT; i = i + 3){
        setPixel(i + q, 0, 0, 0);
      }
    }
  }
}


void Strobe(byte red, byte green, byte blue, int StrobeCount, int FlashDelay, int EndPause){
  for(int j = 0; j < StrobeCount; j++){
    setAll(red, green, blue);
    FastLED.show();
    delay(FlashDelay);
    setAll(0, 0, 0);
    FastLED.show();
    delay(FlashDelay);
  }

  delay(EndPause);
}


void blueFire(int Cooling, int Sparking, int SpeedDelay){
  static byte heat[LED_COUNT];
  int cooldown;

  for(int i = 0; i < LED_COUNT; i++){
    cooldown = random(0,((Cooling * 10) / LED_COUNT) + 2);

    if(cooldown > heat[i]){
      heat[i] = 0;
    } else{
      heat[i] = heat[i] - cooldown;
    }
  }
  for( int k = LED_COUNT - 1; k >= 2; k--){
    heat[k] =(heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  }
  if( random(255) < Sparking ){
    int y = random(7);
    heat[y] = heat[y] + random(160, 255);
  }
  for( int j = 0; j < LED_COUNT; j++){
    setPixelHeatColorBlue(j, heat[j] );
  }
  FastLED.show();
  delay(SpeedDelay);
}

void setPixelHeatColorBlue(int Pixel, byte temperature){
  byte t192 = round((temperature / 255.0) * 191);
  byte heatramp = t192 & 0x03;
  heatramp <<= 2;

  if( t192 > 0x03){
    setPixel(Pixel, heatramp,255, 255);
  } else if( t192 > 0x40 ){
    setPixel(Pixel, 255, heatramp, 0);
  } else{
    setPixel(Pixel, 0, 0, heatramp);
  }
}


void fadeToCenter(){
 static uint8_t hue;
  for(int i = 0; i < LED_COUNT/2; i++) {   
    leds.fadeToBlackBy(40);
    leds[i] = CHSV(hue++,255,255);
    leds(LED_COUNT/2,LED_COUNT-1) = leds(LED_COUNT/2 - 1 ,0);
    FastLED.delay(33);
  }  
}


void runnerChameleon(){
    static uint8_t hue = 0;
  for(int i = 0; i < LED_COUNT; i++) {
    leds[i] = CHSV(hue++, 255, 255);
    FastLED.show(); 
     leds[i] = CRGB::Black;
//    fadeall();
    delay(10);
  }
}


void blende(){
    static uint8_t hue = 0;
  for(int i = 0; i < LED_COUNT; i++) {
    leds[i] = CHSV(hue++, 255, 255);
    FastLED.show(); 
//     leds[i] = CRGB::Black;
    fadeall();
    delay(10);
  }

  for(int i = (LED_COUNT)-1; i >= 0; i--) {
    leds[i] = CHSV(hue++, 255, 255);
    FastLED.show();
//     leds[i] = CRGB::Black;
    fadeall();
    delay(10);
  }
}

void blende_2(){
    static uint8_t hue = 0;
  for(int i = 0; i < LED_COUNT; i++) {
    leds[i] = CHSV(hue++, 255, 255);
    FastLED.show(); 
    leds[i] = CRGB::Black;
    fadeall();
    delay(10);
  }

  for(int i = (LED_COUNT)-1; i >= 0; i--) {
    leds[i] = CHSV(hue++, 255, 255);
    FastLED.show();
//     leds[i] = CRGB::Black;
    fadeall();
    delay(10);
  }
}

void fadeall(){
  for(int i = 0; i < LED_COUNT; i++) {
    leds[i].nscale8(250); 
  } 
}
