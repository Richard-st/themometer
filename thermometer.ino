#include <OneWire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
//#include <Base64.h> 
#include <DallasTemperature.h>

#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <EEPROM.h>

//-------------------------------------------
// Program Definitions
//-------------------------------------------

  // Serial Messages
  #define DEBUG  
  #ifdef DEBUG
    #define DEBUG_PRINT(x)     Serial.print (x)
    #define DEBUG_PRINTLN(x)   Serial.println (x)
  #else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x) 
  #endif

  // Debug Flasher
  #define DEBUG_LED_PIN 5
  #define DEBUG_FLASH_MAIN_LOOP 1
  #define DEBUG_FLASH_ENTER_SETUP 2
  #define DEBUG_FLASH_WIFI_CONNECT 3
  #define DEBUG_FLASH_MQTT_CONNECT 4
  #define DEBUG_FLASH_SETUP 5
  #define DEBUG_FLASH_SLEEP 10      

  #define MODE_SWITCH_PIN 13 
  #define MODE_WORK 0
  #define MODE_SETUP 1  
  
//-------------------------------------------
// Global Variables
//-------------------------------------------
 
  String            ssid;
  String            password;

  const char*       filler  = "                            ";  
  const char*       mqtt_server  = "stanners.co.nz";

// MQTT Variables  
  WiFiClient        espClient;
  PubSubClient      client(espClient);
  int               iMQTTConnectAttempts;

// DS10b20 definitions
  #define           ONE_WIRE_BUS 2  // DS18B20 pin
  OneWire           oneWire(ONE_WIRE_BUS);
  DallasTemperature DS18B20(&oneWire);

// Setup Webserver variable

const char *SetupSsid = "IOT_TERMOMETER_SETUP";

ESP8266WebServer server(80);
String sWIFINetworks;

//EEPROM Vars
  int iVarsSet = 0;


// function declarations
void work_mode();

//-------------------------------------------



//-------------------------------------------
void debug_flash(int noOfFlashes) {
//-------------------------------------------
  int i;
  
  pinMode(DEBUG_LED_PIN, OUTPUT);
  
  for (i=1;i<=noOfFlashes;i++){

    digitalWrite(DEBUG_LED_PIN, HIGH);
    delay(300);  
    digitalWrite(DEBUG_LED_PIN, LOW); 
    delay(300);      
  } 
  delay(500);       
}
//-------------------------------------------

//-------------------------------------------
int checkSetupMode(){
//-------------------------------------------
  //
  // detect if mode switch is requested
  //
  // if MODE_SWITCH_PIN high - continue working mode
  // if MODE_SWITCH_PIN low - go into setup mode after 3 seconds of remaining low
  //
  // returns
  //
  // MODE_WORK 0
  // MODE_SETUP 1
  //
  
  pinMode(MODE_SWITCH_PIN, INPUT);
  
  if (digitalRead(MODE_SWITCH_PIN)==HIGH) return MODE_WORK;

  // pin 13 most be low
  //
  // wait 3 seconds and try again
  //
  
  delay (3000);
  if (digitalRead(MODE_SWITCH_PIN)==HIGH) return MODE_WORK;

  //is MODE_SWITCH_PIN remain low for 3 seconds, go into setup mode
  
  debug_flash(DEBUG_FLASH_ENTER_SETUP);

  return MODE_SETUP;
}

//-------------------------------------------
// Connect to MQTT Server
//-------------------------------------------
void mqtt_connect() {

  iMQTTConnectAttempts = 1;
  
  client.setServer(mqtt_server, 1883);

  // Loop until we're reconnected
    while (!client.connected()) {
      //debug_flash(DEBUG_FLASH_MQTT_CONNECT);

      DEBUG_PRINT("Attempting MQTT connection...");
    
      // Attempt to connect
      if (client.connect("local")) {
        delay(10);
        DEBUG_PRINTLN("connected");
        
    } else {
      DEBUG_PRINT("failed, rc=");
      DEBUG_PRINT(client.state());
      DEBUG_PRINTLN(" try again in .5 seconds");
      // Wait 1 seconds before retrying
      iMQTTConnectAttempts++;
      delay(100);
    }
  }

  client.publish("CA",String(iMQTTConnectAttempts).c_str() );
}

void readEEPROMVars() {

  int iStringLength;
  int iEEPROMPointer = 0;
  
  EEPROM.begin(512);


  iVarsSet = EEPROM.read(iEEPROMPointer);  //value 0f 1 indicates we have values
  DEBUG_PRINT("iVarsSet: ");
  DEBUG_PRINTLN(iVarsSet);
  if (iVarsSet != 1) return; //no varables present

  iEEPROMPointer++;
  iStringLength = EEPROM.read(iEEPROMPointer);  //get ssid String length
  DEBUG_PRINT("SSID Length: ");
  DEBUG_PRINTLN(iStringLength);

  iEEPROMPointer++;
  for (int i = 0; i < iStringLength; ++i)
  {
      ssid += char(EEPROM.read(i+iEEPROMPointer));
  }  
  iEEPROMPointer+=iStringLength;
  DEBUG_PRINT("SSID : ");
  DEBUG_PRINTLN(ssid);
  
  iStringLength = EEPROM.read(iEEPROMPointer);  //get password String length
  DEBUG_PRINT("Password Length: ");
  DEBUG_PRINTLN(iStringLength);
  
  iEEPROMPointer++;
  for (int i = 0; i < iStringLength; ++i)
  {
      password += char(EEPROM.read(i+iEEPROMPointer));
  }  
  iEEPROMPointer+=iStringLength;  
  DEBUG_PRINT("Password : ");
  DEBUG_PRINTLN(password);
  
}

//-------------------------------------------
// Connect to WIFI
//-------------------------------------------
void wifi_connect() {

  DEBUG_PRINTLN("Connecting to wifi");



  WiFi.begin( ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    debug_flash(DEBUG_FLASH_WIFI_CONNECT);
    delay(100);
    DEBUG_PRINT(".");
  }
  DEBUG_PRINTLN("");
  DEBUG_PRINT("Connected to ");
  DEBUG_PRINTLN(ssid);
  DEBUG_PRINT("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());
}

//-------------------------------------------
// build Web Page for setup mode
//-------------------------------------------
void getWifiList(){

  //start up html page //
  sWIFINetworks = "<!DOCTYPE html> <html> <body> <h1>Choose Wifi Network</h1> <form method='post'><select  name='ssidlist'  >";
   
  DEBUG_PRINTLN("scan start");

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  DEBUG_PRINTLN("scan done");
  if (n == 0)
     sWIFINetworks +=("no networks found");
  else
  {
    sWIFINetworks +=(n);
    sWIFINetworks += (" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      sWIFINetworks += "<option value='" ;
      sWIFINetworks +=(WiFi.SSID(i));
      sWIFINetworks += "'>";
      sWIFINetworks +=(WiFi.SSID(i));
      sWIFINetworks += "</option>";
      
      sWIFINetworks.replace("Poppy's","Poppy&#x27s");
      
      delay(10);
    }
  }
  //Finish html page //
  sWIFINetworks += "</select><br>password <input type='text' name='password'> ";
  sWIFINetworks += "<input type='hidden' name='submitted' value='yes'>";
  sWIFINetworks += "<br><input type='submit' value='Submit'></form></body></html>";
}

//-------------------------------------------
//  process return from web page
//-------------------------------------------
void handleRoot() {

  String sSubmitted = "No";
  int iEEPROMPointer = 0;
  
  DEBUG_PRINTLN("handleRoot");
  server.send(200, "text/html", sWIFINetworks.c_str() );

  String selectedSSID = server.arg("ssidlist");
  DEBUG_PRINTLN(selectedSSID.c_str());  
  String selectedPWD = server.arg("password");
  DEBUG_PRINTLN(selectedPWD.c_str()); 
  sSubmitted = server.arg("submitted");
  DEBUG_PRINTLN(sSubmitted.c_str()); 
  
  //
  // write eeprom variables
  //
  
  EEPROM.begin(512);  
  //first byte set to i indicates we have variables
  EEPROM.write(iEEPROMPointer++, 1);
  
  
  //size of the string
  EEPROM.write(iEEPROMPointer++, selectedSSID.length());
  for (int i = 0; i < selectedSSID.length(); ++i){
      EEPROM.write(iEEPROMPointer++, selectedSSID[i]);
      DEBUG_PRINT("Wrote: ");
      DEBUG_PRINTLN(selectedSSID[i]);     
  }
  EEPROM.commit();

  //size of the string
  EEPROM.write(iEEPROMPointer++, selectedPWD.length());
  for (int i = 0; i < selectedPWD.length(); ++i){
      EEPROM.write(iEEPROMPointer++, selectedPWD[i]);
      DEBUG_PRINT("Wrote: ");
      DEBUG_PRINTLN(selectedPWD[i]);     
  }
  EEPROM.commit();

  
  EEPROM.end();  
     
  //
  // go into work mode
  //
  ssid     = selectedSSID.c_str();
  password = selectedPWD.c_str();
  
  if (sSubmitted == "yes")
    work_mode(); 
}

//-------------------------------------------
// POST Temperature
//-------------------------------------------
 void work_mode(){

    float  temp;
    char   cTemp[30]; 
    String sTemp ;
    

    debug_flash(DEBUG_FLASH_MAIN_LOOP);

    //
    //get temperature
    //
    do {
      DS18B20.requestTemperatures(); 
      temp = DS18B20.getTempCByIndex(0);
      DEBUG_PRINT("Temperature: ");
      DEBUG_PRINTLN(temp);
    } while (temp == 85.0 || temp == (-127.0));


    //
    //check wifi connection
    //
    if (WiFi.status() != WL_CONNECTED) {
      wifi_connect();
    }

    //
    // create MQTT Brocker connection
    // 
    if (!client.connected()&& WiFi.status() == WL_CONNECTED ) {
      mqtt_connect();
    }

    //
    // Publish Temp to MQTT broket
    //
    sTemp = String(temp,2);
    sTemp.toCharArray(cTemp,10);
    DEBUG_PRINT(".");
    client.publish("test",cTemp );


    //
    // Sleep
    //
    DEBUG_PRINTLN("going to sleep");
    debug_flash(DEBUG_FLASH_SLEEP);
    ESP.deepSleep(60000000, WAKE_RF_DEFAULT); // Sleep for 60 seconds   
 }

//-------------------------------------------
// Setup Webserver to get connection parameters and system settings
//-------------------------------------------
void  setup_mode(){
  DEBUG_PRINTLN("Configuring access point...");
 
  WiFi.softAP(SetupSsid);

  IPAddress myIP = WiFi.softAPIP();
  DEBUG_PRINT("AP IP address: ");
  DEBUG_PRINTLN(myIP);
  server.on("/", handleRoot);
  
  server.begin();
  Serial.println("HTTP server started");

  getWifiList();  
}

//-------------------------------------------
// One Off Setup
//-------------------------------------------
void setup() {
  delay(100); 

  #ifdef DEBUG
    Serial.begin(115200);
  #endif

  readEEPROMVars();

  if (checkSetupMode()== MODE_WORK){
    if (iVarsSet == 1)  // Only go into work mode if we have variables present
        work_mode();
  }else{
    DEBUG_PRINTLN("Entering Setup ");
    setup_mode();
  }
  
}



//-------------------------------------------
// Control Loop
//-------------------------------------------
void loop() {
  //
  // Only Used When in setup mode
  //
  server.handleClient();  
}
