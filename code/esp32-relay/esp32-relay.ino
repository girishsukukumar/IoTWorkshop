#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFiMulti.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "RemoteDebug.h"  //https://github.com/JoaoLopesF/RemoteDebug
#define USE_ARDUINO_INTERRUPTS true
#include <NTPClient.h>
#include "SPIFFS.h"
#include <PubSubClient.h>
#include "EmonLib.h"

#include <ESP8266FtpServer.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTPIN 25    // Digital pin connected to the DHT sensor 
#define DHTTYPE    DHT11     // DHT 11

#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
#define   DEBUG 1 
//#define  NETWORK_DEBUG 1

#ifdef NETWORK_DEBUG
    #define    DEBUG_PRINTF(f,...)  Debug.printf(f,##__VA_ARGS__)
#endif
    
#ifdef DEBUG
#define DEBUG_PRINTLN(x)      Serial.println (x)
#define DEBUG_PRINT(x)        Serial.print (x)
#define DEBUG_PRINTF(f,...)   Serial.printf(f,##__VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(f,...)
#endif


#define ONBOARD_LED 2
#define SAMPLE_SIZE 10 
#define SD_LIMIT 2.0 
#define WEBSERVER_PORT 80
#define JSON_CONFIG_FILE_NAME "/config.json" 
#define JSON_PERSISTANT_FILE_NAME "/persistant.json"
#define SSID_NAME_LEN 20 
#define SSID_PASSWD_LEN 20
#define NAME_LEN 20
#define FTP_USER_NAME "apollo11"
#define FTP_PASSWORD  "eagle"
#define MAX_LIMIT_OF_CONNETION_FAILURE 5


#define CORE_ONE 1
#define CORE_ZERO 0 

#define MQTT_FINGER_PRINT_LENGTH    150
#define JSON_STRING_LENGTH          350
#define PASSWORD_NAME_LENGTH         20
#define USERNAME_NAME_LENGTH         20
#define MQTT_MAX_CONNECTION_ATTEMPTS 10
#define GEN_NAME_LEN                 20
#define HOST_NAME_LENGTH             40

FtpServer     ftpSrv; 
WebServer     webServer(WEBSERVER_PORT);
RemoteDebug   Debug;
WiFiUDP       ntpUDP;
NTPClient     timeClient(ntpUDP);

int        idx =0 ;

TaskHandle_t ReadTempAndHumidityHandle ;
TaskHandle_t ReadAmpsHandle ;
TaskHandle_t ReadRainAndMoistureHandle ;
TaskHandle_t ReadFlowRateHandle ;

typedef struct configData 
{
  char     ssid1[SSID_NAME_LEN]   ;
  char     password1[SSID_PASSWD_LEN]  ;
  char     ssid2[SSID_NAME_LEN] ;
  char     password2[SSID_PASSWD_LEN] ;
  char     ssid3[SSID_NAME_LEN] ;
  char     password3[SSID_PASSWD_LEN] ;

  char     wifiDeviceName[NAME_LEN] ;  
  char     userName[NAME_LEN];
  bool     sendDataToinFlux ;
  char     mqttServer[HOST_NAME_LENGTH] ;
  int      mqttPort ;
  char     mqttUser[USERNAME_NAME_LENGTH] ;
  char     mqttPassword[PASSWORD_NAME_LENGTH] ;
  uint8_t  mqttCertFingerprint[MQTT_FINGER_PRINT_LENGTH] ;
  uint8_t  mqttJsonStatus ;
  char     dataRequestTopic[HOST_NAME_LENGTH+USERNAME_NAME_LENGTH];
};

struct         configData ConfigData ;
EnergyMonitor  energyMonitor1 ;
WiFiMulti      wifiMulti;          //  Create  an  instance  of  the ESP32WiFiMulti 
bool  gMotorRunning = false ;
double gAmps         = 0.1   ; 
float gAirTemp      = 23.1  ;
float gHumidity     = 57.1  ;
bool  gRainStatus   = false ;
float gSoilMoisture = 63.3  ;
float gFlowRate     = 44.1  ;
float gWindSpeed    = 20.7  ;
const int relayPin  = 26    ;
int   gMqttLastPublishedTime = 0   ;
 
WiFiClientSecure WifiSecureClientForMQTT;
WiFiClient       WifiClientForMQTT  ;
PubSubClient     MQTTPubSubClient(WifiClientForMQTT);


bool ConnectToWifi()
{
  int count ;
  wifiMulti.addAP(ConfigData.ssid1, ConfigData.password1);   
  wifiMulti.addAP(ConfigData.ssid2, ConfigData.password2);    
  wifiMulti.addAP(ConfigData.ssid3, ConfigData.password3);    
  count  = 0 ;
  while  (wifiMulti.run()  !=  WL_CONNECTED) 
  { 
    //  Wait  for the Wi-Fi to  connect:  scan  for Wi-Fi networks, and connect to  the strongest of  the networks  above       
    delay(1000);        
    DEBUG_PRINTF("*");    
    count++ ;
    if (count > 40)
    {
       return false ;  
    }
  }   
  delay(5000);
  WiFi.setHostname(ConfigData.wifiDeviceName);
  DEBUG_PRINTF("\n");   
  DEBUG_PRINTF("Connected to  ");   
  DEBUG_PRINTF("%s \n",WiFi.SSID().c_str());         
  DEBUG_PRINTF("IP  address: ");   
  DEBUG_PRINTF("%s\n\n",WiFi.localIP().toString().c_str()); 
  WiFi.softAPdisconnect (true);   //Disable the Access point mode.
  return true ;
}


void DisplayserverIndex()
{
     File  file ;
     size_t  sent;
 
     if (SPIFFS.exists("/serveridx.html"))  
     { 
        file =SPIFFS.open("/serveridx.html",  "r");
        sent =webServer.streamFile(file, "text/html");  
        file.close();
     }
     else
     {
         webServer.sendHeader("Connection", "close");
         webServer.send(200, "text/html", "<HTML> <H1> File Sensor.html not found </H1> </HTML>");
     }
}
void ChangeDetails()
{
     File  file ;
     size_t  sent;
 
     if (SPIFFS.exists("/config.html"))  
     { 
        file =SPIFFS.open("/config.html",  "r");
        sent =webServer.streamFile(file, "text/html");  
        file.close();
     }
     else
     {
         webServer.sendHeader("Connection", "close");
         webServer.send(200, "text/html", "<HTML> <H1> File loginIndex.html not found </H1> </HTML>");
     }
}
void FileUpload()
{
     File  file ;
     size_t  sent;
 
     if (SPIFFS.exists("/upload.html"))  
     { 
        file =SPIFFS.open("/upload.html",  "r");
        sent =webServer.streamFile(file, "text/html");  
        file.close();
     }
     else
     {
         webServer.sendHeader("Connection", "close");
         webServer.send(200, "text/html", "<HTML> <H1> File loginIndex.html not found </H1> </HTML>");
     }
}

void RebootDevice()
{
    webServer.sendHeader("Connection", "close");
    webServer.send(200, "text/html", "<HTML> <H1> Rebooting </H1> </HTML>");
    ESP.restart();
}
void DisplayLoginIndex()
{
     File  file ;
     size_t  sent;
 
     if (SPIFFS.exists("/loginIndex.html"))  
     { 
        file =SPIFFS.open("/loginIndex.html",  "r");
        sent =webServer.streamFile(file, "text/html");  
        file.close();
     }
     else
     {
         webServer.sendHeader("Connection", "close");
         webServer.send(200, "text/html", "<HTML> <H1> File loginIndex.html not found </H1> </HTML>");
     }

}
void DisplayDashboard()
{
     File  file ;
     size_t  sent;
 
     if (SPIFFS.exists("/sensor.html"))  
     { 
        file =SPIFFS.open("/sensor.html",  "r");
        sent =webServer.streamFile(file, "text/html");  
        file.close();
     }
     else
     {
         webServer.sendHeader("Connection", "close");
         webServer.send(200, "text/html", "<HTML> <H1> File Sensor.html not found </H1> </HTML>");
     }

}

void PostDetails()
{
  const size_t capacity = JSON_OBJECT_SIZE(15);
  DynamicJsonDocument doc(capacity);

  char jsonString[250];

  if (gMotorRunning == true)
  {
    doc["MotorStatus"]  = "Running";
  }
  else
  {
    doc["MotorStatus"]  = "Stopped";
  }


  doc["Amps"]         = gAmps; 
  doc["Temperature"]  = gAirTemp ;
  doc["Humidity"]     = gHumidity ;
  doc["Raining"]      = gRainStatus ;
  doc["SoilMoisture"] = gSoilMoisture;
  doc["WaterFlow"]    = gFlowRate ;
  doc["WindSpeed"]    = gWindSpeed ;

  serializeJson(doc, jsonString);
  webServer.sendHeader("Connection", "close");
  webServer.send(200, "json", jsonString);


}
void UpdateConfigJson()
{
  // To update config.json
}

void StartMotor()
{
  gMotorRunning = true ;
  digitalWrite(relayPin, HIGH);
  DisplayserverIndex();
}

void StopMotor()
{
    gMotorRunning = false ;
    digitalWrite(relayPin, LOW);   
    DisplayserverIndex();
}

void setupWebHandler()
{
   /*return index page which is stored in serverIndex */

  webServer.on("/",                 HTTP_GET,   DisplayLoginIndex);
  webServer.on("/serverIndex",      HTTP_GET,   DisplayserverIndex);
  webServer.on("/dashboard",        HTTP_POST,  DisplayDashboard);
  webServer.on("/getUpdate",        HTTP_POST,  PostDetails);
  webServer.on("/ConfigPage",       HTTP_POST,  ChangeDetails);
  webServer.on("/Fileupload",       HTTP_POST,  FileUpload);
  webServer.on("/rebootDevice",     HTTP_POST,  RebootDevice);
  webServer.on("/StartMotor",       HTTP_POST,  StartMotor);
  webServer.on("/StopMotor",        HTTP_POST,  StopMotor);
  webServer.on("/updateConfigJson", HTTP_POST,  UpdateConfigJson);
 
  /*handling uploading firmware file */
  webServer.on("/update", HTTP_POST, []() {
  webServer.sendHeader("Connection", "close");
  webServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = webServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      DEBUG_PRINTF("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        DEBUG_PRINTF("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  webServer.begin();
}

void ReadConfigValuesFromSPIFFS()
{
  File jsonFile ;
  const size_t capacity = JSON_OBJECT_SIZE(8) + 240;
  DynamicJsonDocument doc(capacity);

  //const char* json = "{\"ssid1\":\"xxxxxxxxxxxxxxxxxxxx\",\"password1\":\"xxxxxxxxxxxxxxxxxxxx\",\"ssid2\":\"xxxxxxxxxxxxxxxxxxxx\",\"password2\":\"xxxxxxxxxxxxxxxxxxxx\",\"ssid3\":\"xxxxxxxxxxxxxxxxxxxx\",\"password3\":\"xxxxxxxxxxxxxxx\",\"wheelDiameter\":85.99,\"devicename\":\"xxxxxxxxxxxxxx\"}";
  
  jsonFile = SPIFFS.open(JSON_CONFIG_FILE_NAME, FILE_READ);
  
  if (jsonFile == NULL)
  {
     DEBUG_PRINTF("Unable to open %s",JSON_CONFIG_FILE_NAME);
     return ;
  }
  
  deserializeJson(doc, jsonFile);

  const char* ssid1 = doc["ssid1"]; // "xxxxxxxxxxxxxxxxxxxx"
  const char* password1 = doc["password1"]; // "xxxxxxxxxxxxxxxxxxxx"
  const char* ssid2 = doc["ssid2"]; // "xxxxxxxxxxxxxxxxxxxx"
  const char* password2 = doc["password2"]; // "xxxxxxxxxxxxxxxxxxxx"
  const char* ssid3 = doc["ssid3"]; // "xxxxxxxxxxxxxxxxxxxx"
  const char* password3 = doc["password3"]; // "xxxxxxxxxxxxxxx"
  const char* devicename = doc["devicename"]; // "xxxxxxxxxxxxxx"
  jsonFile.close();
  
  strcpy(ConfigData.ssid1,ssid1);
  strcpy(ConfigData.password1,password1);
  
  strcpy(ConfigData.ssid2,ssid2);
  strcpy(ConfigData.password2,password2);

  strcpy(ConfigData.ssid3,ssid3);
  strcpy(ConfigData.password3,password3);


//TODO Items for config file 
  strcpy(ConfigData.mqttServer, "platform.i2otlabs.com");
                                
  ConfigData.mqttPort = 1883 ;
  
  strcpy(ConfigData.userName,"girish_kumar"); // TODO Read from JSON file
  sprintf(ConfigData.dataRequestTopic,"MASTER\/WORKOUTDATA\/%s",ConfigData.userName);
  strcpy(ConfigData.mqttUser,"dataone");
  strcpy(ConfigData.mqttPassword,"onedata");

}
void DisplayConfigValues()
{
   DEBUG_PRINTF("ssid1 %s \n",ConfigData.ssid1);
   DEBUG_PRINTF("Password %s \n", ConfigData.password1);

   DEBUG_PRINTF("ssid2 %s \n",ConfigData.ssid2);
   DEBUG_PRINTF("Password2 %s \n", ConfigData.password2);

   DEBUG_PRINTF("ssid3 %s \n",ConfigData.ssid3);
   DEBUG_PRINTF("Password3 %s \n", ConfigData.password3);

   DEBUG_PRINTF("Device name = %s ", ConfigData.wifiDeviceName);
}



void ReadTempAndHumidity(void *params)
{
  
    DHT_Unified   dht(DHTPIN, DHTTYPE);

    sensors_event_t event;
    dht.begin();

  while(true)
  {
    
       dht.temperature().getEvent(&event);
       DEBUG_PRINTF("Temp = %f \n",event.temperature);
       if (isnan(event.temperature) == false) 
       {
         gAirTemp = event.temperature ;
       }
       else
       {
          DEBUG_PRINTF("Error in reading temp \n");
       }      
       dht.humidity().getEvent(&event);
       
       DEBUG_PRINTF("Humudity = %f \n",event.relative_humidity);

       if (isnan(event.relative_humidity) == false) 
       {
         gHumidity = event.relative_humidity ;
       }
       else
       {
          DEBUG_PRINTF("Error in reading humudity \n");
       }
       vTaskDelay(10000); 
  } 
  
}
void ReadAmps(void *params)
{
  
  while(true)
  {
    gAmps = energyMonitor1.calcIrms(1480);  // Calculate Irms only
    DEBUG_PRINTF("%f\n",gAmps*230.0);           // Apparent power
    vTaskDelay(1000); 
  } 

}

void ReadRainAndMoisture(void *params)
{
  while(true)
  {
    vTaskDelay(10000); 
  } 
  
}

void ReadFlowRate(void *params)
{
  while(true)
  {
    vTaskDelay(10000); 
  } 
  
}
void ConfigureAsAccessPoint()
{
   IPAddress local_IP(192,168,4,4);
   IPAddress gateway(192,168,5,5);
   IPAddress subnet(255,255,255,0);
   String gHotSpotIP ;

   WiFi.softAP(ConfigData.wifiDeviceName, "12345689");  //Start HOTspot removing password will disable security
   DEBUG_PRINTF("Wifi.softAP completed ");
   WiFi.softAPConfig(local_IP, gateway, subnet);
   IPAddress myIP = WiFi.softAPIP(); //Get IP address
   gHotSpotIP = myIP.toString() ; 
   DEBUG_PRINTF("%s\n",gHotSpotIP.c_str());
   vTaskDelay(5000);

}
void setup() 
{
  
  int     GMTOffset = 19800;
  Serial.begin(115200);
  DEBUG_PRINTF("Motor Control");

  SPIFFS.begin(true) ;
  pinMode(relayPin, OUTPUT);
  ReadConfigValuesFromSPIFFS();
  DisplayConfigValues();
  DEBUG_PRINTF("Configuratio file reading : Success \n");
  if (ConnectToWifi() == false)
  {
      DEBUG_PRINTF("WifiSetup: failed \n");
      ConfigureAsAccessPoint(); 
  }
  else
  {
     DEBUG_PRINTF("WiFiSetup:Success\n");
  }

  timeClient.begin();
  timeClient.setTimeOffset(GMTOffset); /* GMT + 5:30 hours */
  timeClient.update(); // Keep the device time up to date
  delay(5000);
   
  setupWebHandler();
  //display.printf("Web Server:Ready\n");
  Debug.begin(ConfigData.wifiDeviceName); // Initialize the WiFi server
  Debug.setResetCmdEnabled(true); // Enable the reset command
  Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
  Debug.showColors(true); // Colors
  ftpSrv.begin(FTP_USER_NAME,FTP_PASSWORD);
  DEBUG_PRINTF("WeSuccessb Server configuration: Success \n");
  delay(2000);


  xTaskCreatePinnedToCore(
                    ReadTempAndHumidity,   /* Task function. */
                    "ReadTempAndHumidity",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &ReadTempAndHumidityHandle,      /* Task handle to keep track of created task */
                    CORE_ZERO);          /* pin task to core 1 */  


  xTaskCreatePinnedToCore(
                    ReadAmps,   /* Task function. */
                    "ReadAmps",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &ReadAmpsHandle,      /* Task handle to keep track of created task */
                    CORE_ONE);          /* pin task to core 1 */     
 
  xTaskCreatePinnedToCore(
                    ReadRainAndMoisture,   /* Task function. */
                    "ReadRainAndMoisture",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &ReadRainAndMoistureHandle,      /* Task handle to keep track of created task */
                    CORE_ONE);          /* pin task to core 1 */      

  xTaskCreatePinnedToCore(
                    ReadFlowRate,   /* Task function. */
                    "ReadFlowRate",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &ReadFlowRateHandle,      /* Task handle to keep track of created task */
                    CORE_ONE);          /* pin task to core 1 */      

}



void loop() 
{
  webServer.handleClient();
  ftpSrv.handleFTP();     
  Debug.handle();
  yield();
  // put your main code here, to run repeatedly:

}
