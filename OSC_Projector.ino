#include <FS.h>  
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> 
#include <ArduinoJson.h>
/**OSC Requirments**/ 
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <OSCData.h>
#include <SoftwareSerial.h>
/**OTA Requirments**/
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

//default custom static IP
char static_ip[16] = "192.168.1.78";
char static_gw[16] = "192.168.1.1";
char static_sn[16] = "255.255.255.0";

//flag for saving data
bool shouldSaveConfig = false;

//OSC setup
WiFiUDP Udp;
IPAddress qLabIP(192,168,1,200); //qLab IP
unsigned int qLabPort = 53000; //qLab port
unsigned int localPort = 8888; // OSC port
OSCErrorCode error;
byte lock = 0; // only execute 1 OSC comand at a time

//projector setup
SoftwareSerial projSerial(14, 12, false, 256); //setup projector connection

//webserver setup
ESP8266WebServer server(80);

//Local intialization. Once its business is done, there is no need to keep it around
WiFiManager wifiManager;

char INDEX_HTML[640] =
"<!DOCTYPE HTML>"
"<html>"
"<head>"
"<meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\">"
"<title>OSC Projector Config</title>"
"<style>"
"\"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }\""
"</style>"
"</head>"
"<body>"
"<h1>QLab Configuration</h1>"
"<FORM action=\"/\" method=\"post\">"
"<p>QLab IP:<INPUT type=\"text\" name=\"qLabIP\" value=\"%s\"></p>"
"<p>QLab Port:<INPUT type=\"text\" name=\"qLabPort\" value=\"%i\"></p>"
"<INPUT type=\"submit\" value=\"Send\"> <INPUT type=\"reset\">"
"</FORM>"
"</body>"
"</html>";


//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void displayIndex(){
   byte oct1 = qLabIP[0];
   byte oct2 = qLabIP[1];
   byte oct3 = qLabIP[2];
   byte oct4 = qLabIP[3];

   char s[16];  
   sprintf(s, "%d.%d.%d.%d", oct1, oct2, oct3, oct4);
   char html[640];
   sprintf(html, INDEX_HTML, s, qLabPort);
   server.send(200, "text/html", html);
}

void handleRoot(){
  if (server.hasArg("qLabPort")) {
    handleSubmit();
  } else {
     displayIndex();
  }
}

void handleSubmit(){
  if (!server.hasArg("qLabPort")) return returnFail("BAD ARGS");
  qLabPort = server.arg("qLabPort").toInt();
  String ipTemp = server.arg("qLabIP");
  qLabIP.fromString(ipTemp);
  updateConfig();
  ESP.reset();
}

void returnFail(String msg){
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(500, "text/plain", msg + "\r\n");
}

void handleNotFound(){
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

void updateConfig(){
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();

    json["ip"] = WiFi.localIP().toString();
    json["gateway"] = WiFi.gatewayIP().toString();
    json["subnet"] = WiFi.subnetMask().toString();
    json["qlabport"] = String(qLabPort);

    byte oct1 = qLabIP[0];
    byte oct2 = qLabIP[1];
    byte oct3 = qLabIP[2];
    byte oct4 = qLabIP[3];

    char s[16];  
    sprintf(s, "%d.%d.%d.%d", oct1, oct2, oct3, oct4);     
    json["qlabip"] = String(s);

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
}

void setup() {
  Serial.begin(115200);  
  projSerial.begin(9600);
  Serial.println();

  pinMode(LED_BUILTIN, OUTPUT);
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          if(json["ip"]) {
            char tmp_port[5];
            char tmp_ip[19];
            Serial.println("setting custom ip from config");
            strcpy(static_ip, json["ip"]);
            strcpy(static_gw, json["gateway"]);
            strcpy(static_sn, json["subnet"]);
            strcpy(tmp_port, json["qlabport"]);
            strcpy(tmp_ip, json["qlabip"]);
            qLabPort = atoi(tmp_port);
            qLabIP.fromString(tmp_ip);
          } else {
            Serial.println("no custom ip in config");
          }
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
  


  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set static ip 
  IPAddress _ip,_gw,_sn;
  _ip.fromString(static_ip);
  _gw.fromString(static_gw);
  _sn.fromString(static_sn);
  
  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);


  //tries to connect to last known settings
  //if it does not connect it starts an access point with the specified name
  //here  "OSC_Projector_Setup" with password "projector"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("OSC_Projector_Setup", "projector76")) {
    Serial.println("failed to connect, we should reset as see if it connects");
    delay(3000);
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    updateConfig();
  }

  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("projectorOSC");

  // No authentication by default
   ArduinoOTA.setPassword((const char *)"projector23");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
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

  server.on("/", handleRoot);  
  server.onNotFound(handleNotFound);
  server.begin();
  
}

/* Freeze the projector
 *  0 -- unfreezes the projector
 *  1 -- freezes the projector
 *  2 -- freeze status
 */
void freeze(OSCMessage &msg){
  OSCMessage qLab_msg("/cue/p0/name");
  Serial.print("Freeze");
  lock = 1;
  
  if(msg.getInt(0) == 1){
    projSerial.write("FREEZE ON\r");
    Serial.println(" On");
    qLab_msg.add("Freeze On");
  }else if(msg.getInt(0) == 2){
    projSerial.write("FREEZE?\r");
    Serial.println(" Status");
    qLab_msg.add("Freeze Status");    
  }else{
    projSerial.write("FREEZE OFF\r");
    Serial.println(" Off");
    qLab_msg.add("Freeze Off");
  }

  Udp.beginPacket(qLabIP, qLabPort);
  qLab_msg.send(Udp);
  Udp.endPacket();
  qLab_msg.empty();
  
}

/* Shutters the projector
 *  0 -- unshutters the projector
 *  1 -- shutters the projector
 *  2 -- status of the shutter
 */
void shutter(OSCMessage &msg){  
  OSCMessage qLab_msg("/cue/p0/name");
  Serial.print("Shutter");
  lock = 1;
  
  if(msg.getInt(0) == 1){
    projSerial.write("MUTE ON\r");
    Serial.println(" On");
    qLab_msg.add("Mute On");
  }else if(msg.getInt(0) == 2){
    projSerial.write("MUTE?\r");
    Serial.println(" Status");
    qLab_msg.add("Mute Status");
  }else{
    projSerial.write("MUTE OFF\r");
    Serial.println(" Off");
    qLab_msg.add("Mute Off");
  }

  Udp.beginPacket(qLabIP, qLabPort);
  qLab_msg.send(Udp);
  Udp.endPacket();
  qLab_msg.empty();
}

/* Zoom the projector
 *  0 -- Minimum Zoom
 *  1 -- Maximum Zoom
 *  2 --  Zoom status
 */
void zoom(OSCMessage &msg){
  OSCMessage qLab_msg("/cue/p0/name");
  Serial.print("Zoom");
  lock = 1;
  
  if(msg.getInt(0) == 1){
    projSerial.write("ZOOM MAX\r");
    Serial.println(" Max");
    qLab_msg.add("Zoom Max");
  }else if(msg.getInt(0) == 2){
    projSerial.write("ZOOM?\r");
    Serial.println(" Status");
    qLab_msg.add("Zoom Status");
  }else{
    projSerial.write("ZOOM MIN\r");
    Serial.println(" Min");
    qLab_msg.add("Zoom Min");
  }

  Udp.beginPacket(qLabIP, qLabPort);
  qLab_msg.send(Udp);
  Udp.endPacket();
  qLab_msg.empty();
}

/* Zoom the projector
 *  x < 0 -- zoom out x times
 *  x >= 0 -- zoom in x times
 */
void zoom_inc(OSCMessage &msg){    
  OSCMessage qLab_msg("/cue/p0/name");
  Serial.print("Zoom ");
  lock = 1;
  char cmd[10] = "ZOOM MAX\r";
  
  if(msg.getInt(0) < 0){
    cmd[6] = 'I';
    cmd[7] = 'N';
  }

  int high = (abs(msg.getInt(0)) > 9000)? 9000 : abs(msg.getInt(0));

  projSerial.write(cmd);  
  delay(high);
  projSerial.write("ZOOM OFF\r");
    

  qLab_msg.add("Zoom Incremental 2.0");    
  Udp.beginPacket(qLabIP, qLabPort);
  qLab_msg.send(Udp);
  Udp.endPacket();
  qLab_msg.empty();
}

/* Foucs the projector
 *  0 -- Minimum Focus
 *  1 -- Maximum Focus
 *  2 -- Focus status
 */
void focus(OSCMessage &msg){
  OSCMessage qLab_msg("/cue/p0/name");
  Serial.print("Focus");
  lock = 1;
  
  if(msg.getInt(0) == 1){
    projSerial.write("FOCUS MAX\r");
    Serial.println(" Max");
    qLab_msg.add("Focus Max");
  }else if(msg.getInt(0) == 2){
    projSerial.write("FOCUS?\r");
    Serial.println(" Status");
    qLab_msg.add("Focus Status");
  }else{
    projSerial.write("FOCUS MIN\r");
    Serial.println(" Min");
    qLab_msg.add("Focus Min");
  }

  Udp.beginPacket(qLabIP, qLabPort);
  qLab_msg.send(Udp);
  Udp.endPacket();
  qLab_msg.empty();
}

/* Focus the projector
 *  x < 0 -- focus out x times
 *  x >= 0 -- focus in x times
 */
void focus_inc(OSCMessage &msg){    
  OSCMessage qLab_msg("/cue/p0/name");
  Serial.print("Focus ");
  lock = 1;
  char cmd[11] = "FOCUS MAX\r";
  
  if(msg.getInt(0) < 0){
    cmd[7] = 'I';
    cmd[8] = 'N';
  }

  int high = (abs(msg.getInt(0)) > 9000)? 9000 : abs(msg.getInt(0));
  
  projSerial.write(cmd);
  delay(high);
  projSerial.write("FOCUS OFF\r");
  
  qLab_msg.add("Focus Incremental");    
  Udp.beginPacket(qLabIP, qLabPort);
  qLab_msg.send(Udp);
  Udp.endPacket();
  qLab_msg.empty();

}
  
/* V-Lens the projector
 *  0 -- Minimum V-Lens
 *  1 -- Maximum V-Lens
 *  2 -- V-Lens status
 */
void lens(OSCMessage &msg){
  OSCMessage qLab_msg("/cue/p0/name");
  Serial.print("Vertical Lens");
  lock = 1;
  
  if(msg.getInt(0) == 1){
    projSerial.write("LENS MAX\r");
    Serial.println(" Max");
    qLab_msg.add("Vertical Lens Max");
  }else if(msg.getInt(0) == 2){
    projSerial.write("LENS?\r");
    Serial.println(" Status");
    qLab_msg.add("Vertical Lens Status");
  }else{
    projSerial.write("LENS MIN\r");
    Serial.println(" Min");
    qLab_msg.add("Vertical Lens Min");
  }

  Udp.beginPacket(qLabIP, qLabPort);
  qLab_msg.send(Udp);
  Udp.endPacket();
  qLab_msg.empty();
}

/* V-Lens the projector
 *  x < 0 -- lens out x times
 *  x >= 0 -- lens in x times
 */
void lens_inc(OSCMessage &msg){    
  OSCMessage qLab_msg("/cue/p0/name");
  Serial.print("Lens ");
  lock = 1;
  char cmd[10] = "LENS MAX\r";
  
  if(msg.getInt(0) < 0){
    cmd[6] = 'I';
    cmd[7] = 'N';
  }

  int high = (abs(msg.getInt(0)) > 9000)? 9000 : abs(msg.getInt(0));
  

  projSerial.write(cmd);
  delay(high);
  projSerial.write("LENS OFF\r");
  
  
  qLab_msg.add("Vertical Lens Incremental");    
  Udp.beginPacket(qLabIP, qLabPort);
  qLab_msg.send(Udp);
  Udp.endPacket();
  qLab_msg.empty();

}

/* H-Lens the projector
 *  0 -- Minimum H-Lens
 *  1 -- Maximum H-Lens
 *  2 -- H-Lens status
 */
void hlens(OSCMessage &msg){
  OSCMessage qLab_msg("/cue/p0/name");
  Serial.print("HLens");
  lock = 1;
  
  if(msg.getInt(0) == 1){
    projSerial.write("HLENS MAX\r");
    Serial.println(" Max");
    qLab_msg.add("HLens Max");
  }else if(msg.getInt(0) == 2){
    projSerial.write("HLENS?\r");
    Serial.println(" Status");
    qLab_msg.add("HLens Status");
  }else{
    projSerial.write("HLENS MIN\r");
    Serial.println(" Min");
    qLab_msg.add("HLens Min");
  }

  Udp.beginPacket(qLabIP, qLabPort);
  qLab_msg.send(Udp);
  Udp.endPacket();
  qLab_msg.empty();
}

/* HLens the projector
 *  x < 0 -- HLens out x times
 *  x >= 0 -- HLens in x times
 */
void hlens_inc(OSCMessage &msg){    
  OSCMessage qLab_msg("/cue/p0/name");
  Serial.print("HLens ");
  lock = 1;
  char cmd[11] = "HLENS MAX\r";
  
  if(msg.getInt(0) < 0){
    cmd[7] = 'I';
    cmd[8] = 'N';
  }

  int high = (abs(msg.getInt(0)) > 9000)? 9000 : abs(msg.getInt(0));

  projSerial.write(cmd);
  delay(high);
  projSerial.write("HLENS OFF\r");
  
  qLab_msg.add("Horizontal Lens Incremental");    
  Udp.beginPacket(qLabIP, qLabPort);
  qLab_msg.send(Udp);
  Udp.endPacket();
  qLab_msg.empty();

}
    
/* V-Pos the projector
 *  x < 0 -- vpos out x times
 *  x >= 0 -- vpos in x times
 */
void vpos_inc(OSCMessage &msg){    
  OSCMessage qLab_msg("/cue/p0/name");
  Serial.print("VPos ");
  lock = 1;
  char cmd[10] = "VPOS INC\r";
  
  if(msg.getInt(0) < 0){
    cmd[5] = 'D';
    cmd[6] = 'E';
  }

  int high = (abs(msg.getInt(0)) > 255)? 255 : abs(msg.getInt(0));
  
  Serial.print(cmd);
  Serial.print(" ");
  Serial.print(high);
  Serial.println(" times");
  
  for(int i = 0; i < high; i++){
    projSerial.write(cmd);
    delay(100);
  }
  
  qLab_msg.add("Vertical Postition Incremental");    
  Udp.beginPacket(qLabIP, qLabPort);
  qLab_msg.send(Udp);
  Udp.endPacket();
  qLab_msg.empty();

}

/* H-Pos the projector
 *  x < 0 -- hpos out x times
 *  x >= 0 -- hpos in x times
 */
void hpos_inc(OSCMessage &msg){    
  OSCMessage qLab_msg("/cue/p0/name");
  Serial.print("HPos ");
  lock = 1;
  char cmd[10] = "HPOS INC\r";
  
  if(msg.getInt(0) < 0){
    cmd[5] = 'D';
    cmd[6] = 'E';
  }

  int high = (abs(msg.getInt(0)) > 255)? 255 : abs(msg.getInt(0));
  
  Serial.print(cmd);
  Serial.print(" ");
  Serial.print(high);
  Serial.println(" times");
  
  for(int i = 0; i < high; i++){
    projSerial.write(cmd);
    delay(100);
  }
  
  qLab_msg.add("Horizontal Position Incremental");    
  Udp.beginPacket(qLabIP, qLabPort);
  qLab_msg.send(Udp);
  Udp.endPacket();
  qLab_msg.empty();

}

/* Power the projector
 *  0 -- power off the projector
 *  1 -- power on the projector
 *  2 -- power status
 */
void power(OSCMessage &msg){
  OSCMessage qLab_msg("/cue/p0/name");
  Serial.print("Power");
  lock = 1;
  
  if(msg.getInt(0) == 1){
    projSerial.write("PWR ON\r");
    Serial.println(" On");
    qLab_msg.add("Power On");
  }else if(msg.getInt(0) == 2){
    projSerial.write("PWR?\r");
    Serial.println(" Status");
    qLab_msg.add("Power Status");    
  }else{
    projSerial.write("PWR OFF\r");
    Serial.println(" Off");
    qLab_msg.add("Power Off");
  }

  Udp.beginPacket(qLabIP, qLabPort);
  qLab_msg.send(Udp);
  Udp.endPacket();
  qLab_msg.empty();
  
}


/* Reset the wifi
 *  reset code is 871249
 */
void wifiReset(OSCMessage &msg){  
  if(msg.getInt(0) == 871249){
    Serial.println("Wifi Reseting");
    wifiManager.resetSettings();
    ESP.restart();
  }
}

/* Restart the esp
 *  reset code is 125771
 */
void restart(OSCMessage &msg){
  if(msg.getInt(0) == 125771){
    ESP.restart();
  }
}
void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  digitalWrite(LED_BUILTIN, HIGH);

  lock = 0;

  OSCMessage msg; //create an osc message
  int size = Udp.parsePacket(); //get the size of packet

  if (size > 0 && !lock) { //if the packet has a size and OSC isn't locked
    while (size--) {
      msg.fill(Udp.read()); //populate the message
    }
    if (!msg.hasError()) {
      //send the correct comand to the projector
      msg.dispatch("/projector/freeze", freeze);
      msg.dispatch("/projector/shutter", shutter);
      msg.dispatch("/projector/zoom", zoom);
      msg.dispatch("/projector/zoom/increment", zoom_inc);
      msg.dispatch("/projector/focus", focus);
      msg.dispatch("/projector/focus/increment", focus_inc);
      msg.dispatch("/projector/v-lens", lens);
      msg.dispatch("/projector/v-lens/increment", lens_inc);      
      msg.dispatch("/projector/h-lens", hlens);
      msg.dispatch("/projector/h-lens/increment", hlens_inc);
      msg.dispatch("/projector/h-pos/increment", hpos_inc);
      msg.dispatch("/projector/v-pos/increment", vpos_inc);
      msg.dispatch("/projector/power", power);
      msg.dispatch("/projector/reset_wifi", wifiReset);
      msg.dispatch("/projector/restart", restart);
    } else {
      error = msg.getError();
      Serial.print("error: ");
      Serial.println(error);
    }
  }

}
