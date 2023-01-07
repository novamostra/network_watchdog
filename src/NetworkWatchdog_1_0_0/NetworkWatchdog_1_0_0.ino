// set it to 1 if you want to view the debug messages in the Serial Console
#define DEBUG 0

#ifdef DEBUG
  #define DEBUG_PRINT(x)  Serial.print (x)
  #define DEBUG_PRINT_LN(x) Serial.println (x)
  #define DEBUG_PRINT_F(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINT_LN(x)
  #define DEBUG_PRINT_F(...)
#endif

#include <ESP8266WiFi.h>
#include <FS.h>
#include <ESPAsyncWebSrv.h>

#include <DNSServer.h>

#include <WiFiClient.h>
#include <EEPROM.h>

#include <ESPping.h>

//timeserver
#include <NTPClient.h>
#include <WiFiUdp.h>

#define OUTPUT_PIN 2
#define RST_BTN 0
#define RST_DELAY 3000  //ms to hold the reset button for execute the reset EEPROM function

#define OFF_TIME 15000  //ms to hold the device powered off when ping fails

#ifdef DEBUG
  #define SHORT_DELAY 30000   //30 seconds
  #define LONG_DELAY 60000    //60 seconds
#else
  #define SHORT_DELAY 180000  //3 minutes
  #define LONG_DELAY 1200000  //20 minutes
#endif

#define CONNECTION_RETRIES  3 //After three times then long delay will be used

#define HOSTNAME "nm-wd.local"

WiFiUDP ntp_UDP;
//NTPClient time_client(ntp_UDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);
NTPClient time_client(ntp_UDP);

long epoch_time = 0; //first connection to the time server

boolean is_ap = false; //is access point?
boolean is_btn_pressed = false;
long pressed_at = 0;

long last_con_check = 0;
int failed_con_count = 0;
boolean sec_ping=false;

const IPAddress ap_IP(192, 168, 1, 1);
const char* ap_SSID = "nm_watchdog";

String ssid = "";
String pass = "";

String ssid_list;

String local_ip; //when is station

DNSServer dns_server;

AsyncWebServer web_server(80);

// example.com resolves to 93.184.216.34
IPAddress def_ping_ip (93, 184, 216, 34); // The ip to ping (can be local or remote)

// custom IPs
IPAddress pri_ping_ip;
IPAddress sec_ping_ip;

String pri_ping_url;
String sec_ping_url;

boolean scan_req = false;
boolean ping_req = false;

String req = "";

char* req_url;
boolean ping_ip_req = false;
boolean ping_req_completed = true;

boolean ping_req_result = false;

boolean pinging = false;

boolean saving_config = false;

int reset_count=0;


WiFiClient espClient;

void reset_EEPROM(){
  DEBUG_PRINT_LN("EEPROM init reset");
  DEBUG_PRINT_LN(EEPROM.length());
  for (int i = 0; i < EEPROM.length() ; ++i) { 
    EEPROM.write(i, 0); 
  }
  EEPROM.commit();
  DEBUG_PRINT_LN("Finish");
}

void check_btn_state(){
    if(!digitalRead(RST_BTN)){
      if(!is_btn_pressed){
        is_btn_pressed = true;
        pressed_at = millis();
      }
     }else{
        if(is_btn_pressed){
          is_btn_pressed = false;
          //configuration reset 
          if(millis()-pressed_at >=  RST_DELAY){
            reset_EEPROM();
            ESP.reset();
          }else{
            //restart the connected device
            restart_device(false);
          }
        }
     }
}

boolean save_config(String ssid, String password, String pri_addr, String sec_addr) {
  reset_EEPROM();
  DEBUG_PRINT_LN("Saving new configuration");
    //write ssid
    for (int i = 0; i < ssid.length(); ++i) {
      EEPROM.write(i, ssid[i]);
    }

    //write password
    for (int i = 0; i < password.length(); ++i) {
      EEPROM.write(32+i, password[i]);
    }
    
    // write primary address bit to 1 as it can not be null
    EEPROM.write(96,1);
    
    //it is an IP
    if(pri_ping_ip.fromString(pri_addr)){
      //write ip bit
      EEPROM.write(97,1);
    }
    for (int i = 0; i < pri_addr.length(); ++i) {
      EEPROM.write(98+i, pri_addr[i]);
    }

    //if not null
    if(sec_addr!=""){                 
      EEPROM.write(196,1);
      //it is an IP
      if(sec_ping_ip.fromString(sec_addr)){
        //write IP bit
        EEPROM.write(197,1);
      }
      for (int i = 0; i < sec_addr.length(); ++i) {
        EEPROM.write(198+i, sec_addr[i]);
      }
    }
  DEBUG_PRINT_LN("Saving EEPROM...");  
  EEPROM.commit();
  DEBUG_PRINT_LN("Completed");
  return true;
}

boolean restore_config() {
  DEBUG_PRINT_LN("Reading EEPROM Configuration...");
  int val;
  if (EEPROM.read(0) !=  0) {
    for (int i = 0; i < 32; ++i) {
      ssid +=  char(EEPROM.read(i));
    }
    for (int i = 32; i < 96; ++i) {
      pass +=  char(EEPROM.read(i));
    }
    WiFi.begin(ssid.c_str(), pass.c_str());

    DEBUG_PRINT_LN("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
      DEBUG_PRINT(".");
      delay(100);
      check_btn_state();
    }

    WiFi.hostname(HOSTNAME);
    
    //stored ping configuration 1
    if(EEPROM.read(96) == 1){
      DEBUG_PRINT_LN(char(EEPROM.read(96)));
      DEBUG_PRINT_LN("Found primary configuration");
      
      //ping configuration is an IP
      if(EEPROM.read(97) ==  1){
         DEBUG_PRINT_LN("Found Primary IP");
         String ip;
           for (int i = 98; i < 196; ++i) {
             ip +=  char(EEPROM.read(i));
           }
           pri_ping_ip.fromString(ip);
           DEBUG_PRINT_LN(pri_ping_ip);
      //ping configuration is a URL
      }else{
         DEBUG_PRINT_LN("Found Primary URL");
         for (int i = 98; i < 196; ++i) {
            val = EEPROM.read(i);
            if(val){
              pri_ping_url +=  char(val);
            }
         }
      }
    }

    //stored ping configuration 2
    if(EEPROM.read(196) ==  1){
      DEBUG_PRINT_LN("Found secondary configuration");
      //ping configuration is an IP
      if(EEPROM.read(197) ==  1){
          DEBUG_PRINT_LN("Found Secondary IP");
          String ip;
           for (int i = 198; i < 296; ++i) {
              ip +=  char(EEPROM.read(i));
           }
           sec_ping_ip.fromString(ip);
           DEBUG_PRINT_LN(sec_ping_ip);
      //ping configuration is a URL
      }else{
         DEBUG_PRINT_LN("Found Secondary URL");
         for (int i = 198; i < 296; ++i) {
          val = EEPROM.read(i);
            if(val){
              sec_ping_url +=  char(val);
            }
         }
      }
    }

    // if no configuration is found then use the default ping IP
    if(!pri_ping_ip && pri_ping_url=="" && !sec_ping_ip && sec_ping_url==""){
      pri_ping_ip = def_ping_ip;
    }
    
    return true;
  }else {
    DEBUG_PRINT_LN("Config not found.");
    return false;
  }
  
}

boolean keep_alive_ping(boolean primary=true){
  if(primary){
    if(pri_ping_ip){
      return exec_ping_ip(pri_ping_ip);
    }else{
      return exec_ping_url(pri_ping_url);
    }
  }else{
    if(has_secondary_config()){
      if(sec_ping_ip){
        return exec_ping_ip(sec_ping_ip);      
      }else{
        return exec_ping_url(sec_ping_url);            
      }
    }else{
      return false;
    }
  }
}

boolean has_secondary_config(){
  if(!sec_ping_ip && sec_ping_url==""){
    return false;
  }else{
    return true;
  }
}

void restart_device(boolean count){
    //only count device restarts due to connectivity issues
    if(count){
      reset_count++;    
    }
    DEBUG_PRINT_LN("Restarting device");
    digitalWrite(OUTPUT_PIN,LOW);
    delay(OFF_TIME);
    digitalWrite(OUTPUT_PIN,HIGH);
}

boolean exec_ping_url(String url){
    DEBUG_PRINT("Ping url:");
    DEBUG_PRINT_LN(url);
    last_con_check = millis();
    return Ping.ping(url.c_str());
}

boolean exec_ping_ip(IPAddress ip){
    DEBUG_PRINT("Ping IP:");
    DEBUG_PRINT_LN(ip);
    last_con_check = millis();
    return Ping.ping(ip);
}
void check_wifi_connectivity(){
      if (WiFi.status() ==  WL_CONNECTED) {
        return;
      }else{
        DEBUG_PRINT_LN("No WIFI");
        //try to reconnect
      }
}
void check_connectivity() {
    //if for 3 consecutive times connectivity check fails then the LONG DELAY will be used instead
    boolean ping_result;
    if(failed_con_count<CONNECTION_RETRIES){
      if(millis()-last_con_check>SHORT_DELAY){
        if(sec_ping){
          DEBUG_PRINT_LN("Secondary Ping");
          ping_result = keep_alive_ping(false);
          sec_ping=false;
          if(!ping_result){
            DEBUG_PRINT_LN("Secondary Ping failed... restarting device");
            restart_device(true);
            failed_con_count++;
          }else{
            DEBUG_PRINT_LN("Secondary Ping success");            
            failed_con_count=0;
          }
        }else{
          DEBUG_PRINT_LN("Primary Ping");
          ping_result = keep_alive_ping(true);
          
          if(!ping_result){
            DEBUG_PRINT_LN("Primary Ping failed...");            
            if(has_secondary_config){
              DEBUG_PRINT_LN("Will try secondary ping next time");            
              sec_ping=true;
            }else{
              DEBUG_PRINT_LN("Secondary configuration does not exist... Restarting device");                          
              restart_device(true); 
              failed_con_count++;
            }
          }else{
            DEBUG_PRINT_LN("Primary Ping success");
            failed_con_count=0;
          }
        
        }
      }
    }else{
      if(millis()-last_con_check>LONG_DELAY){
        if(sec_ping){
            ping_result = keep_alive_ping(false);
            sec_ping=false;
            if(!ping_result){
              restart_device(true);
              failed_con_count++;
            }else{
              failed_con_count=0;
            }
          }else{
            ping_result = keep_alive_ping(true);
            // next time ping secondary ip
            if(has_secondary_config){
              sec_ping=true;
            }else{
              if(!ping_result){
                restart_device(true);
                failed_con_count++;
              }else{
                failed_con_count=0;
              }
            }
          }
      }
    }
}

boolean init_wifi_connection() {
  delay(100);
  int count = 0;
  DEBUG_PRINT("Waiting for Wi-Fi connection");
  
  while ( count < 30 ) {
    if (WiFi.status() ==  WL_CONNECTED) {
      DEBUG_PRINT_LN();
      DEBUG_PRINT_LN("Connected!");
      DEBUG_PRINT_LN(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer
      return true;
    }
    delay(500);
    DEBUG_PRINT(".");
    count++;
  }
  DEBUG_PRINT_LN("Timed out.");
  return false;
}

String processor(const String& var){
//  DEBUG_PRINT_LN(var); 
  if(var ==  "SSID_LIST"){
    return ssid_list;  
  }else if(var == "SSID"){
    return ssid.c_str();
  }else if(var == "SSID_PASSWORD"){
    return pass.c_str();
  }else if(var ==  "IP"){
    return WiFi.localIP().toString();
  }else if(var == "GATEWAY"){
    return WiFi.gatewayIP().toString();
  }else if(var == "SUBNET_MASK"){
    return WiFi.subnetMask().toString();
  }else if(var ==  "MAC"){
    return WiFi.macAddress();
  }else if(var == "RSSI"){
    return String(WiFi.RSSI());
  }else if(var == "HOSTNAME"){
    return WiFi.hostname();
  }else if(var ==  "BOOT_TIME"){
    return String(epoch_time);
  }else if(var == "PRIMARY_PING"){
    if (pri_ping_url.length()>0){
      return pri_ping_url;
    }else{
      if(pri_ping_ip){
        return pri_ping_ip.toString();
      }else{
        return def_ping_ip.toString();
      }
    }
  }else if(var == "SECONDARY_PING"){
    if (sec_ping_url.length()>0){
      return sec_ping_url;
    }else{
      return sec_ping_ip.toString();
    }
  }else if(var == "RESET_COUNT"){
    return String(reset_count);
  }
}

void start_web_server(boolean configured) {
  
   web_server.serveStatic("/styles/custom.css",SPIFFS,"/styles/custom.css").setCacheControl("max-age=600");    
   web_server.serveStatic("/styles/pure-min.css",SPIFFS,"/styles/pure-min.css").setCacheControl("max-age=600");
   web_server.serveStatic("/styles/modal.css",SPIFFS,"/styles/modal.css").setCacheControl("max-age=600");
   web_server.serveStatic("/js/jquery.min.js",SPIFFS,"/js/jquery.min.js");
   web_server.serveStatic("/js/ui.js",SPIFFS,"/js/ui.js").setCacheControl("max-age=600");;
   web_server.serveStatic("/js/validators.js",SPIFFS,"/js/validators.js");
   web_server.serveStatic("/js/functions.js",SPIFFS,"/js/functions.js");
   web_server.serveStatic("/js/main.js",SPIFFS,"/js/main.js");
   web_server.serveStatic("/js/config.js",SPIFFS,"/js/config.js");
   web_server.serveStatic("/assets/spinner.svg",SPIFFS,"/assets/spinner.svg").setCacheControl("max-age=600");
   
   web_server.serveStatic("/favicon/android-192x192.png",SPIFFS,"/favicon/android-192x192.png").setCacheControl("max-age=600");
   web_server.serveStatic("/favicon/android-512x512.png",SPIFFS,"/favicon/android-512x512.png").setCacheControl("max-age=600");
   web_server.serveStatic("/favicon/apple-touch-icon.png",SPIFFS,"/favicon/apple-touch-icon.png").setCacheControl("max-age=600");
   web_server.serveStatic("/favicon/favicon.ico",SPIFFS,"/favicon/favicon.ico").setCacheControl("max-age=600");
   web_server.serveStatic("/favicon/favicon-16x16.png",SPIFFS,"/favicon/favicon-16x16.png").setCacheControl("max-age=600");
   web_server.serveStatic("/favicon/favicon-32x32.png",SPIFFS,"/favicon/favicon-32x32.png").setCacheControl("max-age=600");
   web_server.serveStatic("/favicon/site.webmanifest",SPIFFS,"/favicon/site.webmanifest").setCacheControl("max-age=600");

   web_server.onNotFound([](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/404.html", "text/html",false);
    request->send(response);
      });



  if(configured){
   web_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
//    request->send(SPIFFS, "/index.html", "text/html",false,processor);
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.html", "text/html",false,processor);
   response->addHeader("Access-Control-Allow-Headers","*");    
   response->addHeader("Access-Control-Allow-Origin", "*");
   request->send(response);
      });
  }else{
   web_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/config.html", "text/html",false,processor);
   response->addHeader("Access-Control-Allow-Headers","*");    
   response->addHeader("Access-Control-Allow-Origin", "*");
   request->send(response);
      });
  }
      
 web_server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *request){
    int paramsNr = request->params();
    String result = "";
     if(paramsNr>0){ 
          for(int i = 0;i<paramsNr;i++){
              AsyncWebParameter* p = request->getParam(i);
              if(p->name() == "ip" || p->name() == "url"){
                if(!ping_req){
                  ping_req = true;
                  ping_req_completed = false;
                  if(p->name() == "ip"){
                      ping_ip_req = true;
                  }
                  req = p->value();
                  DEBUG_PRINT("Ping:");
                  DEBUG_PRINT_LN(req);
                  result = "wait";
                }else{
                  DEBUG_PRINT_LN("Ping in process");
                  if(ping_req_completed){
                    DEBUG_PRINT_LN("Ping completed");
                    if(ping_req_result){
                      result = "success";
                    }else{
                      result = "failed";
                    }
                    ping_req = false;
                    ping_ip_req = false;
                    pinging = false;
                  }else{
                    result = "wait";                    
                  }
                }
              }
          }
      }else{
        result="Not enough parameters!";
      }
      
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", result);
      response->addHeader("Access-Control-Allow-Headers","*");
      response->addHeader("Access-Control-Allow-Origin", "*");
      request->send(response);  
      
      });

    if(!configured){
      web_server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request){
      DEBUG_PRINT_LN("Receive request to store configuration");
      int paramsNr = request->params();
      String result = "wait";
      String ssid_param="";
      String ssid_pass_param="";
      String pri_ping_param="";
      String sec_ping_param="";
       if(!saving_config){
         if(paramsNr>0){
                for(int i = 0;i<paramsNr;i++){
                  AsyncWebParameter* p = request->getParam(i);
                  DEBUG_PRINT_LN(p->name());
                  DEBUG_PRINT_LN(p->value());
                  if(p->name() == "ssid" ){
                    ssid_param = p->value();
                  }else if(p->name() == "ssid_pass" ){ 
                    ssid_pass_param = p->value();
                  }else if(p->name() == "pri_ping" ){
                    pri_ping_param = p->value();
                  }else if(p->name() == "sec_ping" ){
                    if(p->value()!="0"){
                      DEBUG_PRINT_LN("Secondary address is not null");
                      sec_ping_param = p->value();
                    }else{
                      DEBUG_PRINT_LN("Secondary address is null");                  
                    }
                  }
                }
                  saving_config=true;
                  save_config(ssid_param,ssid_pass_param,pri_ping_param,sec_ping_param);
                  saving_config=false;
                  result="Configuration stored";                
         }
        }
        
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", result);
        response->addHeader("Access-Control-Allow-Headers","*");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
        });

       web_server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
    String result = "";

      if(!scan_req){
            scan_req = true;
            DEBUG_PRINT("Scanning");
            WiFi.scanNetworksAsync(scan_result);
            result = "wait";
      }
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", result);
      response->addHeader("Access-Control-Allow-Headers","*");
      response->addHeader("Access-Control-Allow-Origin", "*");
      request->send(response);  
      
      });
    }   


   /*
   web_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
//    request->send(SPIFFS, "/index.html", "text/html",false,processor);
   AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.html", "text/html",false,processor);
   response->addHeader("Access-Control-Allow-Headers","*");    
   response->addHeader("Access-Control-Allow-Origin", "*");
   request->send(response);
      });
          
   
   web_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
//    request->send(SPIFFS, "/index.html", "text/html",false,processor);
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.html", "text/html",false,processor);
   response->addHeader("Access-Control-Allow-Headers","*");    
   response->addHeader("Access-Control-Allow-Origin", "*");
   request->send(response);
      });
      */
  web_server.begin();  
}

void detect_networks(){
  int n = WiFi.scanNetworks();
  ssid_list="";
  for (int i = 0; i < n; ++i) {
    ssid_list +=  "<option value = \"";
    ssid_list +=  WiFi.SSID(i);
    ssid_list +=  "\">";
    ssid_list +=  WiFi.SSID(i);
    ssid_list +=  "</option>";
    DEBUG_PRINT_LN(WiFi.SSID(i));
  }
  WiFi.scanDelete();
}

void scan_result(int networksFound)
{
  DEBUG_PRINT_F("%d network(s) found\n", networksFound);
  ssid_list="";
  for (int i = 0; i < networksFound; i++)
  {
    ssid_list +=  "<option value = \"";
    ssid_list +=  WiFi.SSID(i);
    ssid_list +=  "\">";
    ssid_list +=  WiFi.SSID(i);
    ssid_list +=  "</option>";
    DEBUG_PRINT_LN(WiFi.SSID(i));
  }
   scan_req=false; 
  WiFi.scanDelete();
}

void setup_ap() {
  DEBUG_PRINT("Starting Access Point at \"");
  DEBUG_PRINT(ap_SSID);
  DEBUG_PRINT_LN("\"");
  detect_networks();
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ap_IP, ap_IP, IPAddress(255, 255, 255, 0));
  //set ap ssid, null password, channel 1, not hidden and limit max connections to 1
  WiFi.softAP(ap_SSID,"",1,false,1);
  WiFi.hostname(HOSTNAME);
  start_web_server(false);
  dns_server.start(53, "*", ap_IP);
  is_ap = true;
}

void reset_ping_config(){
  DEBUG_PRINT_LN("EEPROM ip config reset");
  for (int i = 96; i < 296 ; ++i) { 
    EEPROM.write(i, 0); 
  }
  EEPROM.commit();
  DEBUG_PRINT_LN("Finish");
}

void setup() {
  pinMode(RST_BTN,INPUT);
  pinMode(OUTPUT_PIN,OUTPUT);
  digitalWrite(OUTPUT_PIN,HIGH);
  
  Serial.begin(115200);  

  if(!SPIFFS.begin()){
        DEBUG_PRINT_LN("An Error has occurred while mounting SPIFFS");
        return;
  }

  // use 512 bytes of EEPROM
  EEPROM.begin(512);
  
  delay(10);
  detect_networks();
  
  if (restore_config()) {
    if (init_wifi_connection()) {
      start_web_server(true);
      time_client.begin();
      time_client.update();
      DEBUG_PRINT_LN("NTP Time:");
      DEBUG_PRINT_LN(time_client.getFormattedTime());
      epoch_time = time_client.getEpochTime();
      DEBUG_PRINT_LN(epoch_time);
      time_client.end();
    }else{      
          setup_ap();
    }
  }else{
    setup_ap();
  }  
}

void loop() {
  if (is_ap) {
    dns_server.processNextRequest();
  }else{
    //check and reset the device only if is configured
   check_connectivity();    
   check_wifi_connectivity();
  }
   check_btn_state();
 
  if(ping_req){
    if(!pinging){
    DEBUG_PRINT_LN("Ping requested");
    boolean ping_result;
      
      if(ping_ip_req){
         IPAddress req_ip;
         req_ip.fromString(req);
         pinging = true;
          if(Ping.ping(req_ip)&& Ping.averageTime()>0){
            ping_result = true;
          }else{
            ping_result = false;
          }
      }else{
        const char* host = req.c_str();
         pinging = true;
        if(Ping.ping(host) && Ping.averageTime()>0){
          ping_result = true;
        }else{
          ping_result = false;
        }
      }
      
    if(ping_result){
      DEBUG_PRINT_LN("success");
      DEBUG_PRINT_LN();
      ping_req_result = true;
    }else{
      DEBUG_PRINT_LN("failed");
      ping_req_result = false;
    }
    ping_req_completed = true;
  }    
  }
}
