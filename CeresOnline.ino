/*
  Modbus RS485 Soil Moisture Sensor with temp
   - read all the holding and register values for ID 1,11-16
   - method to change ID 1 to a new ID
   - Sends stream and status to (formerly)carriots IOT interface
        NOW https://www.altairsmartcore.com
  Circuit:
   - MKR 1000 board
   - MKR 485 shield
        pins A5(RE)and A6(DE), 13 and 14 and voltage
   - Modbus RS485 soil moisture sensor
        by Catnip electronics
        https://www.tindie.com/products/miceuz/modbus-rs485-soil-moisture-sensor-2/ 
   - SparkFun 16x2 SerLCD - RGB on Black 3.3V   
        https://www.sparkfun.com/products/14073
        https://github.com/sparkfun/SparkFun_SerLCD_Arduino_Library
   - 6 position relay board - generic
   - AM2302 (wired DHT22) temperature-humidity sensor
        https://www.adafruit.com/product/393
        
        
*/
//******************************************************
//******         VERSION 5.7
//       added 1-6 sensors to the break readings
//    2.4 
//       changed time to easier ntp server
//    2.5 
//       fixed time for carriots
//       adjusted the sensor output
//    2.6
//       added relays for low moisture level
//    2.7a
//       changed delays to timer function to speed up operation
//       not all delays were removed
//       fixed timing issues - cleaned up some code
//    2.8
//       moved WIFI SSID/Password to the arduino secrets.h file
//       moved Carriots Device ID and API Key to the secrets.h file 
//     b)
//       added sensor 12-16, amd fixed serial output messages to reflect the true sensor ID
//       changed output to carriots for Data Stream to send sensors 11-16 
//    3.0 
//       added WebServer for access to pump Control
//       added moisture and temperature for 6 sensors
//    3.1 
//       added MDNS name http://ceres.local/ to connect to web server
//    3.1a  
//       added moisture is greater than 0 meaning if sensors are offline, dont turn pump onn
//    4.0 
//       added lcd display  Sparkfun I2C 
//       SparkFun 16x2 SerLCD - RGB on Black 3.3V
//       https://www.sparkfun.com/products/14073
//    4.1  
//       added watchdogtimer for auto reboot on code hang
//    4.2  
//       added checkPump function, removed delay while pump running
//    4.3b  
//       added boolean wifiUp - works with or without wifi
//       added LCD changes to ORANGE when wifi connection fails
//    4.4a 
//       cleaned up code
//    4.4b
//       fixed carriots address and rem errors
//    4.4c
//       fixed carriots info in arduino_secrets.h file
//    4.4e 
//       changed NTP time lookup to time.update
//       removed the 4 pings to google and replaced with one every ten minutes
//    4.4f 
//       pump timer 20 seconds
//    4.4g 
//       fixed epoch time
//    4.5  
//       added new timer options with a cool down timer
//       running timer runs and coolDown timer run at the same time,
//       the coolDown timer finishes first then the pump runs for the remainder of the
//       running timer
//       const int coolDown = 30000; 
//       long runningTimer = 60000;
//    4.5a
//       sensor 16 has updated time settings 30 seconds on, 30 seconds off
//    4.5b
//       updated all sensors with new time delay
//    4.5c
//       added one hour restart on No Wifi condition
//    4.5d
//       added high water threshold for watering shutdown level
//    4.5e 
//       added 3 second timer to lcd display
//    4.6
//      removed adafruits wdt timer and replaced with WDTZero.h library 
//    4.6a 
//      added ip address to lcd scrolling
//    5.0 
//      added download properties from the cloud
//    5.1 
//      changed all sennsor timer settings for watering
//    5.2
//      added low and high moisture level in config
//      pulls values from carriots every 3 minutes
//      uses those values for the moisture watering values
//    5.3
//      changed soilTemp to a float, to return Soiltemp with a decimal value
//      changed Serial println for moisture thresholds to reflect new variable values
//      changed pump timers to 45 seconds on and 10 minutes off
//    5.4 
//      added individual pump running timer and pump cool down timer 
//        to the cloud config
//    5.5 
//       fixed temp checked only on boot up
//    5.6
//       changed modbus speed to 9600
//       changed reading of Moisture sensor to pull 2 registers with one call
//     5.7
//       removed reading sensors all the time, now they read every 3 seconds as does the lcd display reading
//     need to add //  https://www.arduino.cc/en/Tutorial/FirmwareUpdater
//******************************************************
String softwareFirmware = "5.7";
//#######################################################
//########### Ceres Helper ##############################

// watchdog timer
#include <WDTZero.h>
//https://github.com/javos65/WDTZero.git
#include <ArduinoHttpClient.h>
//https://github.com/amcewen/HttpClient.git
// dns responder
#include <WiFiMDNSResponder.h>
#include <NTPClient.h>
#include <WiFi101.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ArduinoRS485.h> 
#include <ArduinoModbus.h>
#include "arduino_secrets.h" 
// Sparkpost library for the lcd display
//https://github.com/sparkfun/SparkFun_SerLCD_Arduino_Library
#include <SerLCD.h>
// Initialize the lcd library with default I2C address 0x72
SerLCD lcd; 
char mdnsName[] = "ceres"; // the MDNS name that the board will respond to
// Create a MDNS responder to listen and respond to MDNS name requests.
WiFiMDNSResponder mdnsResponder;
byte bssid[6];
byte encryption =0;
//////////////////////////////////////////////////////////////////////////////////////////
/////// Wifi Settings ////////////////////////////////////////////////////////////////////
///////please enter your sensitive data in the Secret tab/arduino_secrets.////////////////
char ssid[] = SECRET_SSID;        // your network SSID (wifi name)
char pass[] = SECRET_PASS;    // your network password (wifi password)
// change to your server's port
// the AP server port number
WiFiServer server(80);
int serverPort = 80;
//int serverPort = 443;
WiFiClient wifiClient;
//WiFiSSLClient wifiClient;
char serverAddress[] = "api.altairsmartcore.com";  // server address
HttpClient client = HttpClient(wifiClient, "api.carriots.com", serverPort);
// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;
// wifi working value
boolean wifiUp = false;

// ping google variables
float pingAverage = 0;
int ping1 = 0;
int ping2 = 0;
int ping3 = 0;
int pingResult;
int totalCount = 0; 
// wifi status
int status = WL_IDLE_STATUS; 
// temp and humidity sensor values
uint16_t soilMoisture = 0;
uint16_t soilMoisture11 = 0,soilMoisture12 = 0,soilMoisture13 = 0,soilMoisture14 = 0,soilMoisture15 = 0,soilMoisture16 = 0;
int16_t soilTempC = 0;
float soilTemp11F = 0,soilTemp12F= 0,soilTemp13F= 0,soilTemp14F= 0,soilTemp15F= 0,soilTemp16F= 0;
//DHT22 temp/humidity sensor
float temp_F=0;
int val = 0;
float temp = 0;


//pin names for relays
const int relay_1 = 0; 
const int relay_2 = 3;
const int relay_3 = 4;
const int relay_4 = 5;
const int relay_5 = 6;
const int relay_6 = 7;
String hostName = "www.google.com";
int loopsensor = 11;
//pump timer
int pumpTimer_11=30,pumpTimer_12=30,pumpTimer_13=30,pumpTimer_14=30,pumpTimer_15=30,pumpTimer_16=30;
boolean pump_11_On = false,pump_12_On = false,pump_13_On = false,pump_14_On = false,pump_15_On = false,pump_16_On = false;
int pump_11_cycle_limit = 6,pump_12_cycle_limit = 6,pump_13_cycle_limit = 6,pump_14_cycle_limit = 6,pump_15_cycle_limit = 6,pump_16_cycle_limit = 6;
int pump_11_cycle_count = 0,pump_12_cycle_count = 0,pump_13_cycle_count = 0,pump_14_cycle_count = 0,pump_15_cycle_count = 0,pump_16_cycle_count = 0;
boolean pump_11_coolDown = false,pump_12_coolDown = false,pump_13_coolDown = false,pump_14_coolDown = false,pump_15_coolDown = false,pump_16_coolDown = false; 
unsigned long coolDownTimer = 0;
boolean Watering_11 = false,Watering_12 = false,Watering_13 = false,Watering_14 = false,Watering_15 = false,Watering_16 = false; 

//************shrekware carriot api key********************************************************************************************************************************************
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
const String  SECRET_APIKEY = CARRIOTSAPIKEY; // Replace with your Carriots apikey
const String  SECRET_DEVICE = CARRIOTSDEVICE; // Replace with the id_developer of your device
String wifiFirmware = "*.*";
IPAddress ip; 
long rssi = 0;
long epoch=0;
unsigned long currentMillis = 0;
// used instead of the delay
const long oneHourInterval = 3599000;
unsigned long previousMillis = 0; 
unsigned long previousMillisGoogle = 0; 
unsigned long tenMinutePreviousMillis = 0; 
unsigned long threeMinutePreviousMillis = 0;
unsigned long lcdDisplayMillis = 0;
//one hour is 3600000 millis
const long pingGoogleTime = 3599000;
const long tenMinutes = 600000;
// the loop count for displaying info o the lcd display
int lcdLoop = 11;
// unsigned long previousMillisLoop = 0;    // couldnt find
boolean startUP = false;

//DEFAULT TIMER IF NOT DOWNLOADED 
// cool Down and pump running times 
const int coolDown =600000; 
const int runningTimer = 45000;

// config variables
int from;
int to;
String strPumpRunningTimer_11;
unsigned long intPumpRunningTimer_11;
String strPumpRunningTimer_12;
unsigned long intPumpRunningTimer_12;
String strPumpRunningTimer_13;
unsigned long intPumpRunningTimer_13;
String strPumpRunningTimer_14;
unsigned long intPumpRunningTimer_14;
String strPumpRunningTimer_15;
unsigned long intPumpRunningTimer_15;
String strPumpRunningTimer_16;
unsigned long intPumpRunningTimer_16;

String strPumpCoolDownTimer_11;
unsigned long intPumpCoolDownTimer_11;
String strPumpCoolDownTimer_12;
unsigned long intPumpCoolDownTimer_12;
String strPumpCoolDownTimer_13;
unsigned long intPumpCoolDownTimer_13;
String strPumpCoolDownTimer_14;
unsigned long intPumpCoolDownTimer_14;
String strPumpCoolDownTimer_15;
unsigned long intPumpCoolDownTimer_15;
String strPumpCoolDownTimer_16;
unsigned long intPumpCoolDownTimer_16;


// watchdog timer
WDTZero MyWatchDoggy; // Define WDT  
//    time stuff
/////// time stuff //////////////////////////////////////////////////////////////
// time.nist.gov NTP server
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
// Defines pin number to which the sensor is connected
#define DHTPIN A1        // pin for DHT22 humidity sensor
#define DHTTYPE DHT22   // DHT22 type
#define delayMillis 30000UL
// temp and humidity sensor
DHT dht(DHTPIN, DHTTYPE);



//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//  config parameters pulled from api.altairsmartcore.com
int statusCode;
String response;
String myString;
//String tempCheckMoisture;
String soilMoisture_11,soilMoisture_12,soilMoisture_13,soilMoisture_14,soilMoisture_15,soilMoisture_16;
int lowMoistureThreshold_11,lowMoistureThreshold_12,lowMoistureThreshold_13,lowMoistureThreshold_14,lowMoistureThreshold_15,lowMoistureThreshold_16;
int highMoistureThreshold_11,highMoistureThreshold_12,highMoistureThreshold_13,highMoistureThreshold_14,highMoistureThreshold_15,highMoistureThreshold_16;


void setup() 
  {
    Wire.begin();
    lcd.begin(Wire); //Set up the LCD for I2C communication
    Wire.setClock(400000); //Optional - set I2C SCL to High Speed Mode of 400kHz
    lcd.setBacklight(255, 255, 255); //Set lcd backlight to bright white
    lcd.setContrast(4); //Set contrast. Lower to 0 for higher contrast.
    lcd.clear(); //Clear the display - this moves the cursor to home position as well
    lcd.print("The Ceres Helper");
    lcd.setCursor(0, 1);
    lcd.print("starting ...");
    
    // First a normal example of using the watchdog timer.
    // Enable the watchdog by calling Watchdog.enable() as below.
    // This will turn on the watchdog timer with a ~32 second timeout
    // before reseting the Arduino. The estimated actual milliseconds
    // before reset (in milliseconds) is returned.
    // Make sure to reset the watchdog before the countdown expires or
    // the Arduino will reset!

 //  Serial.print("\nWDTZero-Demo : Setup Soft Watchdog at 32S interval"); 
 //MyWatchDoggy.attachShutdown(myshutdown); //  a slow shutdown function, not provided
 MyWatchDoggy.setup(WDT_SOFTCYCLE1M);  // initialize WDT-softcounter refesh cycle on 1 Minute interval

  dht.begin();
    
    pinMode(relay_1, OUTPUT);
    digitalWrite(relay_1, HIGH);
    pinMode(relay_2, OUTPUT);
    digitalWrite(relay_2, HIGH);
    pinMode(relay_3, OUTPUT);
    digitalWrite(relay_3, HIGH);
    pinMode(relay_4, OUTPUT);
    digitalWrite(relay_4, HIGH);
    pinMode(relay_5, OUTPUT);
    digitalWrite(relay_5, HIGH);
    pinMode(relay_6, OUTPUT);
    digitalWrite(relay_6, HIGH);
    
//Serial.begin(9600);
      
    Serial.println("Starting Modbus Client Moisture Sensor");
   // start the Modbus client, DEFAULT the moisture sensor runs at 19200 with a 500ms interval
   // changed sensors to run at 9600, see sensor sketch on github to change sensor ID and baud rates
    if (!ModbusRTUClient.begin(9600)) 
     {
       Serial.println("Error, the modbus client did not start");
       while (1);
     } 
   int m = 0;    
   while(status != WL_CONNECTED)
     {   
         m++;
         wifiClient.stop();
         if(m>5)
           {
             wifiUp=false;
             lcd.setBacklight(0xFF8C00);
             break;
           }
         wifiUp=true;
         Serial.print("Attempting to connect to WPA SSID: ");
         Serial.println(ssid);
        // unsuccessful, retry in 10 seconds
         status = WiFi.begin(ssid, pass);
         delay(3000);
      } 
    if(wifiUp)
      {
         Serial.print("wifiUp is :");
         Serial.println(wifiUp);
         delay(3000);
         lcd.clear(); 
         lcd.print("You're connected to the network"); 
         //Serial.print("You're connected to the network");
       
         delay(3000);
     
         // start time server
         timeClient.begin();
         // set offset for not daylight savings
         // cant set offset here, it interferes with the epoch time sent to carriots
         // timeClient.setTimeOffset(-14400);
      
         printCurrentNet();
         printWiFiData();
         server.begin();
         wifiFirmware = WiFi.firmwareVersion();
         //Serial.println();
         currentMillis = millis();
      
         // Setup the MDNS responder to listen to the configured name.
         // NOTE: You _must_ call this _after_ connecting to the WiFi network and
         // being assigned an IP address.
         if (!mdnsResponder.begin(mdnsName)) 
           {
              //Serial.println("Failed to start MDNS responder!");
              while(1);
           }
         //Serial.print("Server listening at http://");
         //Serial.print(mdnsName);
         //Serial.println(".local/");
         lcd.clear(); 
         lcd.print("Server at       " );
         lcd.print(mdnsName);
         lcd.print(".local/");
         delay(5000);
      }
          //no WIFI
      else
        {
           Serial.println("no WIFI detected");
           wifiFirmware = WiFi.firmwareVersion();
           //Serial.println();
           currentMillis = millis();
           previousMillis = currentMillis;
        }
  }
//______________________________________________________________________________
//##############################################################################
//******************************************************************************     
//**************** Main Program Loop  ******************************************
//______________________________________________________________________________
     
void loop() 
  {  
      MyWatchDoggy.clear();  // refresh wdt - before it loops
     //wifiUp
    if(wifiUp)
     {
       // runs once on start up
       if(!startUP)
        {
          lcd.clear(); 
          lcd.print("The Ceres Helper" );
          lcd.print("reading sensors");
         
          readSensors();
         
          sendStatusStream();
          sendStream();
          startUP=true;
          getConfig();
        } 
        
      if(currentMillis - lcdDisplayMillis >= 3000)
        {
             if(lcdLoop >17){lcdLoop=11;}
             if(lcdLoop==17)
             {
                lcd.clear(); 
                lcd.print("Server at       " );
              // lcd.print(mdnsName);
              // lcd.print(".local/");
                lcd.setCursor(0,1);
                ip = WiFi.localIP();
                lcd.print(ip);
                lcdDisplayMillis=currentMillis; 
              }
              else
              {
               checkMoisture(lcdLoop); 
       
               lcdDisplayMillis=currentMillis; 
               lcd.clear(); 
               lcd.print("Moisture " );
               lcd.print(lcdLoop);
               lcd.print(":" );
               lcd.print(soilMoisture);
               lcd.setCursor(0,1);
               lcd.print("UpTime:" );
               // Print the number of seconds since reset:
               lcd.print(millis() / 1000);
             }
             lcdLoop++;
         }

       // Call the update() function on the MDNS responder every loop iteration to
       // make sure it can detect and respond to name requests.
       mdnsResponder.poll();
       // get time
       timeClient.update();
       // read DHT22 sensor
       readDHT22();
       // currently empty function, returns true   
       checkTempStatus();
       //records average ping to google over 4 pings
       pingGoogle();

       // gets config every 3 minutes
      threeMinuteCheck();
       
      
       MyWatchDoggy.clear();  // refresh wdt - before it loops
       // sends status every ten minutes
       Serial.println("10 Minute Check");
      tenMinuteCheck();
       
       // sends data every hour
       Serial.println("60 Minute Check");
       
      oneHourCheck();
  
    WiFiClient wifiClient = server.available();   // listen for incoming clients
      
       if (wifiClient) 
        {                             // if you get a client,
       //Serial.println("new client");           // print a message out the serial port
        String currentLine = "";  // make a String to hold incoming data from the client
   
        while(wifiClient.connected()){          // loop while the client's connected
        if (wifiClient.available()) {             // if there's bytes to read from the wifiClient,
        char c = wifiClient.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') // if the byte is a newline character
        {                    
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the wifiClient HTTP request, so send a response:
         if (currentLine.length() == 0) 
         {
           // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
           // and a content-type so the wifiClient knows what's coming, then a blank line:
           wifiClient.println("HTTP/1.1 200 OK");
           wifiClient.println("Content-type:text/html");
           wifiClient.println();
           wifiClient.println("<html>");
           wifiClient.println("<head>");
           wifiClient.println("<style type=\"text/css\"> body {background-color: #d3e8c8; margin:50px; padding:20px; line-height: 400% } h2 {font-size: 52px;color: blue;text-align: center;}p {font-family: verdana;font-size:xx-large;}</style>");
           wifiClient.println("<title>Manual Pump Override</title>");
           wifiClient.println("</head>");
           wifiClient.println("<body>");
           //the content of the HTTP response follows the header:
           wifiClient.print("<h2>PUMP OVERRIDE</h2>");
           wifiClient.print("<p style=\"color:Crimson\">");
           wifiClient.print("Sensor 11 moisture reading : ");
           wifiClient.print(soilMoisture11);
           wifiClient.print("<br>");
           wifiClient.print("Click <a href=\"/1\">here</a> to turn PUMP 11 on<br>");
           wifiClient.print("<hr>");
           wifiClient.print("<p style=\"color:Maroon\">");
           //wifiClient.println("<style type=\"text/css\"> body {background-color: #d3e8c8; margin:50px; padding:20px; line-height: 400% } h2 {font-size: 52px;color: blue;text-align: center;}p {font-family: verdana;font-size:xx-large;}</style>");
           wifiClient.print("Sensor 12 moisture reading : ");
           wifiClient.print(soilMoisture12);
           wifiClient.print("<br>");
           wifiClient.print("Click <a href=\"/2\">here</a> to turn PUMP 12 on<br>");
           wifiClient.print("<hr>");
           wifiClient.print("<p style=\"color:DarkGoldenRod\">");
           wifiClient.print("Sensor 13 moisture reading : ");
           wifiClient.print(soilMoisture13);
           wifiClient.print("<br>");
           wifiClient.print("Click <a href=\"/3\">here</a> to turn PUMP 13 on<br>");
           wifiClient.print("<hr>");
           wifiClient.print("<p style=\"color:SlateGray\">");
           wifiClient.print("Sensor 14 moisture reading : ");
           wifiClient.print(soilMoisture14);
           wifiClient.print("<br>");
           wifiClient.print("Click <a href=\"/4\">here</a> to turn PUMP 14 on<br>");
           wifiClient.print("<hr>");
           wifiClient.print("<p style=\"color:Green\">");
           wifiClient.print("Sensor 15 moisture reading : ");
           wifiClient.print(soilMoisture15);
           wifiClient.print("<br>");
           wifiClient.print("Click <a href=\"/5\">here</a> to turn PUMP 15 on<br>");
           wifiClient.print("<hr>");
           wifiClient.print("<p style=\"color:FireBrick\">");
           wifiClient.print("Sensor 16 moisture reading : ");
           wifiClient.print(soilMoisture16);
           wifiClient.print("<br>");
           wifiClient.print("Click <a href=\"/6\">here</a> to turn PUMP 16 on<br>");
           wifiClient.print("<hr>");
           wifiClient.print("<h2>Temperatures</h2>");
           wifiClient.print("<p style=\"color:Crimson\">");
           wifiClient.print("Sensor 11 temperature: ");
           wifiClient.print(soilTemp11F);
           wifiClient.print("<br>");
           wifiClient.print("<p style=\"color:Maroon\">");
           wifiClient.print("Sensor 12 temperature: ");
           wifiClient.print(soilTemp12F);
           wifiClient.print("<br>");
           wifiClient.print("<p style=\"color:DarkGoldenRod\">");
           wifiClient.print("Sensor 13 temperature: ");
           wifiClient.print(soilTemp13F);
           wifiClient.print("<br>");
           wifiClient.print("<p style=\"color:SlateGray\">");
           wifiClient.print("Sensor 14 temperature: ");
           wifiClient.print(soilTemp14F);
           wifiClient.print("<br>");
           wifiClient.print("<p style=\"color:Green\">");
           wifiClient.print("Sensor 15 temperature: ");
           wifiClient.print(soilTemp15F);
           wifiClient.print("<br>");
           wifiClient.print("<p style=\"color:FireBrick\">");
           wifiClient.print("Sensor 16 temperature: ");
           wifiClient.print(soilTemp16F);
           wifiClient.print("<br>");
           wifiClient.print("<hr>");
           wifiClient.print("Software Version: ");
           //wifiClient.print(F("<input type='value' name=LowSetText value="));
           wifiClient.print (softwareFirmware);
           wifiClient.println("</p>");
           // The HTTP response ends with another blank line:
           wifiClient.println();
           // break out of the while loop:
      
        break;
        
          }
          else 
          {      // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        }
        else if (c != '\r') 
           {    // if you got anything else but a carriage return character,
              currentLine += c;      // add it to the end of the currentLine
           }
        // Check to see if the wifiClient request was "GET /H" or "GET /L":
           if (currentLine.endsWith("GET /1")) {
          digitalWrite(relay_1, LOW);               // GET /1 turns Pump 1 on
     //      Serial.println("Pin 1 Low");

        }
        if (currentLine.endsWith("GET /2")) {
          digitalWrite(relay_2, LOW);                // GET /2 turns Pump 2 on
     //      Serial.println("relay_2  LOW");

        }
         if (currentLine.endsWith("GET /3")) {
          digitalWrite(relay_3, LOW);        // GET /3 turns Pump 3 on
     //      Serial.println("relay_3 LOW");

        }
        if (currentLine.endsWith("GET /4")) {
          digitalWrite(relay_4, LOW);                // GET /4 turns Pump 4 on
     //     Serial.println("relay_4 LOW");

           }
              if (currentLine.endsWith("GET /5")) {
          digitalWrite(relay_5, LOW);               // GET /5 turns Pump 5 on
     //     Serial.println("Pin 5 LOW");
  
           }
              if (currentLine.endsWith("GET /6")) {
          digitalWrite(relay_6, LOW);               // GET /6 turns Pump 6 on
    //      Serial.println("Pin 6 LOW");
            }
      }
    }
       delay(1);
    // close the connection:
   wifiClient.stop();
 
 //   Serial.println("wifiClient disonnected");
  }
}
//****************************************************
//****************************************************
//   when there is NO WIFI
//****************************************************
//****************************************************
  else
    {
      // sets lcd to ORANGE no wifi
      // runs once on start up
     if(!startUP)
      {
        lcd.clear(); 
        lcd.print("The Ceres Helper" );
        lcd.print("reading sensors");
        readSensors();
        startUP=true;
      } 
     if(loopsensor >16){loopsensor=11;}
        //  checks moisture level
         checkMoisture(loopsensor); 
         delay(500);
         // check temp of soil
        //   convertTemp(loopsensor);
      if(currentMillis - lcdDisplayMillis >= 3000)
        {
         if(lcdLoop >16){lcdLoop=11;}
           checkMoisture(lcdLoop); 
           lcdDisplayMillis=currentMillis; 
           lcd.clear(); 
           lcd.print("Moisture " );
           lcd.print(lcdLoop);
           lcd.print(":" );
           lcd.print(soilMoisture);
           lcd.setCursor(0,1);
           lcd.print("UpTime:" );
           // Print the number of seconds since reset:
           lcd.print(millis() / 1000);
           lcdLoop++;
           }
         
         loopsensor++;
       
         readDHT22();
  
      // currently empty function, returns true   
       checkTempStatus();
    
  // every hour reset and check wifi again
        currentMillis = millis();

      if(currentMillis - previousMillis >= oneHourInterval)
       {
         //Serial.print("this is currentMillis: ");
         //Serial.print(currentMillis); 
         //Serial.print("this is previousMillis: ");
         //Serial.println(previousMillis);      
         previousMillis = currentMillis;
         // WAIT 80 SECONDS FOR A RESTART
        delay(80000);
       }
   }
 //end Void LOOP      
}

// controller temp and humidity
void readDHT22()
   {
      for (int x=1;x<10; x++) 
      { 
        temp = dht.readTemperature();
        val = (int)dht.readHumidity();  
        delay(50);
      }
      //DHT22 sensor
       temp_F = (temp* 9 +2)/5+32;  
   }
   
void readSensors()
   {
     //*******************UNCOMMENNT THE FUNCTIONS YOU WOULD LIKE TO RUN ***********************************
     // read ID of sensor 1
     readHoldingRegisterValues();
     delay(500);

     // read Moisture value of sensor 11                             
     readInputRegisterValues(11);
     delay(500);
     
     // read Moisture value of sensor 12
     readInputRegisterValues(12);
     delay(500);
     
     readInputRegisterValues(13);
     delay(500);

     readInputRegisterValues(14);
     delay(500);

     readInputRegisterValues(15);
     delay(500);

     readInputRegisterValues(16);
     delay(500);
    }

 void getConfig()
 {
  
      String devices = String("/devices/"+DEVICE+"/");
      String strAPI = String("ApiKey:"+APIKEY);
    Serial.println("*******************************************************************************************");  
    Serial.println("*******************************************************************************************"); 
    Serial.println("*************************    GET CONFIG               *************************************");  
    Serial.println("*******************************************************************************************");   
    Serial.println("making GET request");
     Serial.println(devices);
    Serial.println(devices);
    Serial.println(strAPI);
     client.beginRequest();
     client.get(devices);
     client.sendHeader(strAPI);
     client.endRequest();
     statusCode = client.responseStatusCode();
     Serial.print("Status code: ");
     Serial.println(statusCode);
     if(statusCode == 200)
      { 
        response = client.responseBody();
        Serial.print("Response: ");
        Serial.println(response);
      // interprets the response and applies it to the proper variables
       parseResponse();
     }
 }

// parses the json into its respective variables 
void parseResponse()
  {
    if(statusCode == 200)
     {
       //PumpCoolDownTimer_11          
      from = response.indexOf("PumpCoolDownTimer_11");
      if(from >0){
      to = response.indexOf(",",from);
      myString=response.substring(from+23, to-1);
      strPumpCoolDownTimer_11 = myString;
      intPumpCoolDownTimer_11 = myString.toInt();
 
      }else
      {
        intPumpCoolDownTimer_11 = coolDown;
    
      }

          //PumpCoolDownTimer_12
                  
      from = response.indexOf("PumpCoolDownTimer_12");
      if(from >0){
      to = response.indexOf(",",from);
      myString=response.substring(from+23, to-1);
      strPumpCoolDownTimer_12 = myString;
      intPumpCoolDownTimer_12 = myString.toInt();

      }else
      {
        intPumpCoolDownTimer_12 = coolDown;
       
      }

          //PumpCoolDownTimer_13
                  
      from = response.indexOf("PumpCoolDownTimer_13");
      if(from >0){
      to = response.indexOf(",",from);
      myString=response.substring(from+23, to-1);
      strPumpCoolDownTimer_13 = myString;
      intPumpCoolDownTimer_13 = myString.toInt();
      }else
      {
        intPumpCoolDownTimer_13 = coolDown;

      }

          //PumpCoolDownTimer_14
                  
      from = response.indexOf("PumpCoolDownTimer_14");
      if(from >0){
      to = response.indexOf(",",from);
      myString=response.substring(from+23, to-1);
      strPumpCoolDownTimer_14 = myString;
      intPumpCoolDownTimer_14 = myString.toInt();

      }else
      {
        intPumpCoolDownTimer_14 = coolDown;
 
      }
      
          //PumpCoolDownTimer_15
                  
      from = response.indexOf("PumpCoolDownTimer_15");
      if(from >0){
      to = response.indexOf(",",from);
      myString=response.substring(from+23, to-1);
      strPumpCoolDownTimer_15 = myString;
      intPumpCoolDownTimer_15 = myString.toInt();

      }else
      {
        intPumpCoolDownTimer_15 = coolDown;

      }
     
     
     //PumpCoolDownTimer_16
                  
      from = response.indexOf("PumpCoolDownTimer_16");
      if(from >0){
      to = response.indexOf(",",from);
      myString=response.substring(from+23, to-1);
      strPumpCoolDownTimer_16 = myString;
      intPumpCoolDownTimer_16 = myString.toInt();

      }else
      {
        intPumpCoolDownTimer_16 = coolDown;
      }
      
//###############################################################
      //PumpRunningTimer_11
      from = response.indexOf("PumpRunningTimer_11");
      if(from >0)
      {
          to = response.indexOf(",",from);
          myString=response.substring(from+22, to-1);
          strPumpRunningTimer_11 = myString;
          intPumpRunningTimer_11 = myString.toInt();
          //Serial.print("PumpRunningTimer_11 is ");  
          //Serial.println(myString.toInt());
          //Serial.print("myString 11 ");  
          //Serial.println(myString);
      }
      else
        {
          intPumpRunningTimer_11 = runningTimer;
          //Serial.print("NOT FOUND !!!!! PumpRunningTimer_11 is ");  
          //Serial.println(intPumpRunningTimer_11);
        }
      
      //PumpRunningTimer_12
      from = response.indexOf("PumpRunningTimer_12");
      if(from >0)
      {
          to = response.indexOf(",",from);
          myString=response.substring(from+22, to-1);
          strPumpRunningTimer_12 = myString;
          intPumpRunningTimer_12 = myString.toInt();
          //Serial.print("PumpRunningTimer_12 is ");  
          //Serial.println(intPumpRunningTimer_12);
          //Serial.print("myString 12 ");  
          //Serial.println(myString);
      }
      else
        {
          intPumpRunningTimer_12 = runningTimer;
          //Serial.print("NOT FOUND !!!!! PumpRunningTimer_12 is ");  
          //Serial.println(intPumpRunningTimer_12);
        }

      //PumpRunningTimer_13
      from = response.indexOf("PumpRunningTimer_13");
      if(from >0)
      {
          to = response.indexOf(",",from);
          myString=response.substring(from+22, to-1);
          strPumpRunningTimer_13 = myString;
          intPumpRunningTimer_13 = myString.toInt();
          //Serial.print("PumpRunningTimer_13 is ");  
          //Serial.println(intPumpRunningTimer_13);
      }
      else
        {
          intPumpRunningTimer_13 = runningTimer;
          //Serial.print("NOT FOUND !!!!! PumpRunningTimer_13 is ");  
          //Serial.println(intPumpRunningTimer_13);
        }
      
      //PumpRunningTimer_14
      from = response.indexOf("PumpRunningTimer_14");
      if(from >0)
      {
          to = response.indexOf(",",from);
          myString=response.substring(from+22, to-1);
          strPumpRunningTimer_14 = myString;
          intPumpRunningTimer_14 = myString.toInt();
          //Serial.print("PumpRunningTimer_14 is ");  
          //Serial.println(intPumpRunningTimer_14);
      }
      else
        {
          intPumpRunningTimer_14 = runningTimer;
          //Serial.print("NOT FOUND !!!!! PumpRunningTimer_14 is ");  
          //Serial.println(intPumpRunningTimer_14);
        }

                  //PumpRunningTimer_15
                  
      from = response.indexOf("PumpRunningTimer_15");
      if(from >0){
      to = response.indexOf(",",from);
      myString=response.substring(from+22, to-1);
      strPumpRunningTimer_15 = myString;
      intPumpRunningTimer_15 = myString.toInt();
      //Serial.print("PumpRunningTimer_15 is ");  
      //Serial.println(intPumpRunningTimer_15);
      }else
      {
        intPumpRunningTimer_15 = runningTimer;
        //Serial.print("NOT FOUND !!!!! PumpRunningTimer_15 is ");  
        //Serial.println(intPumpRunningTimer_15);
      }
      
                  //PumpRunningTimer_16
                  
      from = response.indexOf("PumpRunningTimer_16");
      if(from >0){
      to = response.indexOf(",",from);
      myString=response.substring(from+22, to-1);
      strPumpRunningTimer_16 = myString;
      intPumpRunningTimer_16 = myString.toInt();
      //Serial.print("PumpRunningTimer_16 is ");  
      //Serial.println(intPumpRunningTimer_16);
      }else
      {
        intPumpRunningTimer_16 = runningTimer;
        //Serial.print("NOT FOUND !!!!! PumpRunningTimer_16 is ");  
        //Serial.println(intPumpRunningTimer_16);
      }
     // #############################################
            //highMoistureThreshold_11
      from = response.indexOf("highMoistureThreshold_11");
      if(from >0){
      to = response.indexOf(",",from);
      myString=response.substring(from+27, to-1);
      highMoistureThreshold_11 = myString.toInt();
      //Serial.print("highMoistureThreshold_11 is ");  
      //Serial.println(highMoistureThreshold_11);
      }else
      {
        //Serial.print("NOT FOUND !!!!! highMoistureThreshold_11 is ");  
        //Serial.println(highMoistureThreshold_11);
      }
            //highMoistureThreshold_12
     
      from = response.indexOf("highMoistureThreshold_12");
        if(from >0)
        {    
          to = response.indexOf(",",from);
          myString=response.substring(from+27, to-1);
          highMoistureThreshold_12 = myString.toInt();
          //Serial.print("highMoistureThreshold_12 is ");  
          //Serial.println(highMoistureThreshold_12);
       }
       else
        {
          //Serial.print("NOT FOUND !!!!! highMoistureThreshold_12 is ");  
          //Serial.println(highMoistureThreshold_12);
        }
            
            //highMoistureThreshold_13
         
      from = response.indexOf("highMoistureThreshold_13");
      if(from >0)
      {   
          to = response.indexOf(",",from);
          myString=response.substring(from+27, to-1);
          highMoistureThreshold_13 = myString.toInt();
          //Serial.print("highMoistureThreshold_13 is ");  
          //Serial.println(highMoistureThreshold_13);
      }
      else
      {
        //Serial.print("NOT FOUND !!!!! highMoistureThreshold_13 is ");  
        //Serial.println(highMoistureThreshold_13);
      }
            //highMoistureThreshold_14
            
      from = response.indexOf("highMoistureThreshold_14");
        if(from >0){  
      to = response.indexOf(",",from);
      myString=response.substring(from+27, to-1);
      highMoistureThreshold_14 = myString.toInt();
      //Serial.print("highMoistureThreshold_14 is ");  
      //Serial.println(highMoistureThreshold_14);
      }
      else
      {
        //Serial.print("NOT FOUND !!!!! highMoistureThreshold_14 is ");  
        //Serial.println(highMoistureThreshold_14);
      }
           //highMoistureThreshold_15
        
      from = response.indexOf("highMoistureThreshold_15");
        if(from >0){ 
      to = response.indexOf(",",from);
      myString=response.substring(from+27, to-1);
      highMoistureThreshold_15 = myString.toInt();
      //Serial.print("highMoistureThreshold_15 is ");  
      //Serial.println(highMoistureThreshold_15);
      }
      else
      {
        //Serial.print("NOT FOUND !!!!! highMoistureThreshold_15 is ");  
        //Serial.println(highMoistureThreshold_15);
      }
                 //highMoistureThreshold_16
             
      from = response.indexOf("highMoistureThreshold_16");
       if(from >0){   
      to = response.indexOf(",",from);
      myString=response.substring(from+27, to-1);
      //Serial.println(myString);
      highMoistureThreshold_16 = myString.toInt();
      //Serial.print("highMoistureThreshold_16 is ");  
      //Serial.println(highMoistureThreshold_16);
      }
      else
      {
        //Serial.print("NOT FOUND !!!!! highMoistureThreshold_16 is ");  
        //Serial.println(highMoistureThreshold_16);
      }

      
                  //lowMoistureThreshold_11
         
      from = response.indexOf("lowMoistureThreshold_11");
      if(from >0){     
      to = response.indexOf(",",from);
      myString=response.substring(from+26, to-1);
      soilMoisture_16 = myString;
      lowMoistureThreshold_11 = myString.toInt();
      //Serial.print("lowMoistureThreshold_11 is ");  
      //Serial.println(lowMoistureThreshold_11);
       }
      else
      {
        //Serial.print("NOT FOUND !!!!! lowMoistureThreshold_11 is ");  
        //Serial.println(lowMoistureThreshold_11);
      }

       //lowMoistureThreshold_12
      from = response.indexOf("lowMoistureThreshold_12");
        if(from >0){    
      to = response.indexOf(",",from);
      myString=response.substring(from+26, to-1);
      lowMoistureThreshold_12 = myString.toInt();
      //Serial.print("lowMoistureThreshold_12 is ");  
      //Serial.println(lowMoistureThreshold_12);
      }
      else
      {
        //Serial.print("NOT FOUND !!!!! lowMoistureThreshold_12 is ");  
        //Serial.println(lowMoistureThreshold_12);
      }
            //lowMoistureThreshold_13
      from = response.indexOf("lowMoistureThreshold_13");
         if(from >0){    
      to = response.indexOf(",",from);
      myString=response.substring(from+26, to-1);
      lowMoistureThreshold_13 = myString.toInt();
      //Serial.print("lowMoistureThreshold_13 is ");  
      //Serial.println(lowMoistureThreshold_13);
        }
      else
      {
        //Serial.print("NOT FOUND !!!!! lowMoistureThreshold_13 is ");  
        //Serial.println(lowMoistureThreshold_13);
      }
      
            //lowMoistureThreshold_14
      from = response.indexOf("lowMoistureThreshold_14");
        if(from >0){  
      to = response.indexOf(",",from);
      myString=response.substring(from+26, to-1);
      lowMoistureThreshold_14 = myString.toInt();
      //Serial.print("lowMoistureThreshold_14 is ");  
      //Serial.println(lowMoistureThreshold_14);
          }
      else
      {
        //Serial.print("NOT FOUND !!!!! lowMoistureThreshold_14 is ");  
        //Serial.println(lowMoistureThreshold_14);
      }
      
           //lowMoistureThreshold_15
      from = response.indexOf("lowMoistureThreshold_15");
        if(from >0){  
      to = response.indexOf(",",from);
      myString=response.substring(from+26, to-1);
      lowMoistureThreshold_15 = myString.toInt();
      //Serial.print("lowMoistureThreshold_15 is ");  
      //Serial.println(lowMoistureThreshold_15);
      }
      else
      {
        //Serial.print("NOT FOUND !!!!! lowMoistureThreshold_15 is ");  
        //Serial.println(lowMoistureThreshold_15);
      }
      
                 //lowMoistureThreshold_16
      from = response.indexOf("lowMoistureThreshold_16");
       if(from >0){ 
      to = response.indexOf(",",from);
      myString=response.substring(from+26, to-1);
      lowMoistureThreshold_16 = myString.toInt();
      //Serial.print("lowMoistureThreshold_16 is ");  
      //Serial.println(lowMoistureThreshold_16);
      }
      else
      {
        //Serial.print("NOT FOUND !!!!! lowMoistureThreshold_16 is ");  
        //Serial.println(lowMoistureThreshold_16);
      }
 }
      else{
           //Serial.print("didnt download config, response code: ");
                 //Serial.println(statusCode);
        }
   }

 // gets config every three minutes
void threeMinuteCheck()
   {
        if(currentMillis - threeMinutePreviousMillis >= 180000)
        {
          threeMinutePreviousMillis=currentMillis; 
          getConfig();
        }
   }
  // sends status every ten minutes  
void tenMinuteCheck()
   {
      if(currentMillis - tenMinutePreviousMillis >= tenMinutes)
        {
          tenMinutePreviousMillis=currentMillis; 
          sendStatusStream();
        }
   } 
  // sends sensor data every hour
void oneHourCheck()
   {
     currentMillis = millis();
     if(currentMillis - previousMillis >= oneHourInterval)
       {
         //Serial.print("this is currentMillis: ");
         //Serial.print(currentMillis); 
         //Serial.print("this is previousMillis: ");
         //Serial.println(previousMillis);      
         previousMillis = currentMillis;
         // sends data stream to carriots  
         sendStream(); 
       }
   }

void pingGoogle()
   {
     if(previousMillisGoogle>0)
     {
        currentMillis = millis();
      
        if (currentMillis - previousMillisGoogle >= pingGoogleTime) 
        {
          previousMillisGoogle = currentMillis;
          pingTimedGoogle();
        }
    }
     else    // pings google on start up
     {
          pingTimedGoogle();
          previousMillisGoogle = currentMillis;
     }
   }
   
void pingTimedGoogle()
   {
    
     //Serial.println("ping average");
     pingG();
     pingAverage = pingResult; 
     //Serial.print("Average ping time to Google: ");
     //Serial.println(pingAverage);
    }

boolean checkPumpRunning(int x)
{
      // creates a variable to determine if the pump is running
      boolean pumpRunning = false;
      // sets current millis to the system time 
      currentMillis = millis();
   switch(x)
   { 
         case 11:
             //Serial.print("CurrenntMillis: ");  
             //Serial.println(currentMillis); 
             //Serial.print("pumpTimer_11: ");  
             //Serial.println(pumpTimer_11); 
             //Serial.print("PumpRunningTimer_11: ");  
             //Serial.println(intPumpRunningTimer_11); 
             currentMillis = millis();
             if(!pump_11_On)
             {    
                pumpTimer_11= currentMillis;
                coolDownTimer = currentMillis;
                pump_11_On = true;
                pump_11_coolDown=false;
             }
              if(currentMillis - pumpTimer_11< intPumpRunningTimer_11 and !pump_11_coolDown)
                {  
                    pumpRunning = true;
                    //Serial.print("CurrenntMillis: ");  
                    //Serial.println(currentMillis); 
                    //Serial.print("Case 11: pumpTimer: ");  
                    //Serial.println(pumpTimer_11);
                    //Serial.print("Current - pumpTimer: ");  
                    //Serial.print(currentMillis-pumpTimer_11);
                    //Serial.print(" < ");  
                    //Serial.println(intPumpRunningTimer_11);
                    //Serial.print("coolDownTimer: ");  
                    //Serial.println(intPumpCoolDownTimer_11);
                    //Serial.println("Case 11: pump running:  true");  
                    // turns on pump   coolDownTimer = 30000
                    // if the cool down has been met the pump can be turned back on
                    coolDownTimer=currentMillis;
                }
                else
                  {   
                      if(currentMillis - coolDownTimer< intPumpCoolDownTimer_11)  //  coolDown
                      {  
                        pumpRunning = false;
                        pump_11_coolDown=true;
                        //Serial.print("coolDownTimer: ");  
                        //Serial.print(currentMillis - coolDownTimer);
                        //Serial.print(" < "); 
                        //Serial.print(intPumpCoolDownTimer_11); 
                        //Serial.println("Case 11: pump running:  false");
                       }else
                       {  
                        pumpRunning = false;
                        pump_11_coolDown = false;
                        pump_11_On = false;
                       //Serial.println("ELSE  Case 11: pump running:  false");
                       }
                    }
             break;
             
         case 12: 
             //Serial.print("CurrenntMillis: ");  
             //Serial.println(currentMillis); 
             //Serial.print("pumpTimer_12: ");  
             //Serial.println(pumpTimer_12); 
             //Serial.print("PumpRunningTimer_12: ");  
             //Serial.println(intPumpRunningTimer_12); 
             currentMillis = millis();
             if(!pump_12_On)
             {    
                pumpTimer_12= currentMillis;
                coolDownTimer = currentMillis;
                pump_12_On = true;
                pump_12_coolDown=false;
             }
              if(currentMillis - pumpTimer_12< intPumpRunningTimer_12 and !pump_12_coolDown)
                {  
                    pumpRunning = true;
                    //Serial.print("CurrenntMillis: ");  
                    //Serial.println(currentMillis); 
                    //Serial.print("Case 12: pumpTimer: ");  
                    //Serial.println(pumpTimer_12);
                    //Serial.print("Current - pumpTimer: ");  
                    //Serial.print(currentMillis-pumpTimer_12);
                    //Serial.print(" < ");  
                    //Serial.println(intPumpRunningTimer_12);
                    //Serial.print("coolDownTimer: ");  
                    //Serial.println(intPumpCoolDownTimer_12);
                    //Serial.println("Case 12: pump running:  true");  
                    // turns on pump   coolDownTimer = 30000
                    // if the cool down has been met the pump can be turned back on
                    coolDownTimer=currentMillis;
                }
                else
                  {   
                      if(currentMillis - coolDownTimer< intPumpCoolDownTimer_12)  //  coolDown
                      {  
                        pumpRunning = false;
                        pump_12_coolDown=true;
                        //Serial.print("coolDownTimer: ");  
                        //Serial.print(currentMillis - coolDownTimer);
                        //Serial.print(" < "); 
                        //Serial.print(intPumpCoolDownTimer_12); 
                        //Serial.println("Case 12: pump running:  false");
                       }else
                       {  
                        pumpRunning = false;
                        pump_12_coolDown = false;
                        pump_12_On = false;
                       //Serial.println("ELSE  Case 12: pump running:  false");
                       }
                    }
             break;
        case 13:
             //Serial.print("CurrenntMillis: ");  
             //Serial.println(currentMillis); 
             //Serial.print("pumpTimer_13: ");  
             //Serial.println(pumpTimer_13); 
             //Serial.print("PumpRunningTimer_13: ");  
             //Serial.println(intPumpRunningTimer_13); 
             currentMillis = millis();
             if(!pump_13_On)
             {    
                pumpTimer_13= currentMillis;
                coolDownTimer = currentMillis;
                pump_13_On = true;
                pump_13_coolDown=false;
             }
              if(currentMillis - pumpTimer_13< intPumpRunningTimer_13 and !pump_13_coolDown)
                {  
                    pumpRunning = true;
                    //Serial.print("CurrenntMillis: ");  
                    //Serial.println(currentMillis); 
                    //Serial.print("Case 13: pumpTimer: ");  
                    //Serial.println(pumpTimer_13);
                    //Serial.print("Current - pumpTimer: ");  
                    //Serial.print(currentMillis-pumpTimer_13);
                    //Serial.print(" < ");  
                    //Serial.println(intPumpRunningTimer_13);
                    //Serial.print("coolDownTimer: ");  
                    //Serial.println(intPumpCoolDownTimer_13);
                    //Serial.println("Case 13: pump running:  true");  
                    // turns on pump   coolDownTimer = 30000
                    // if the cool down has been met the pump can be turned back on
                    coolDownTimer=currentMillis;
                }
                else
                  {   
                      if(currentMillis - coolDownTimer< intPumpCoolDownTimer_13)  //  coolDown
                      {  
                        pumpRunning = false;
                        pump_13_coolDown=true;
                        //Serial.print("coolDownTimer: ");  
                        //Serial.print(currentMillis - coolDownTimer);
                        //Serial.print(" < "); 
                        //Serial.print(intPumpCoolDownTimer_13); 
                        //Serial.println("Case 13: pump running:  false");
                       }else
                       {  
                          pumpRunning = false;
                          pump_13_coolDown = false;
                          pump_13_On = false;
                          //Serial.println("ELSE  Case 13: pump running:  false");
                       }
                    }
             break;   
       case 14:
             //Serial.print("CurrenntMillis: ");  
             //Serial.println(currentMillis); 
             //Serial.print("pumpTimer_14: ");  
             //Serial.println(pumpTimer_14); 
             //Serial.print("PumpRunningTimer_14: ");  
             //Serial.println(intPumpRunningTimer_14); 
             currentMillis = millis();
             if(!pump_14_On)
             {    
                pumpTimer_14= currentMillis;
                coolDownTimer = currentMillis;
                pump_14_On = true;
                pump_14_coolDown=false;
             }
              if(currentMillis - pumpTimer_14< intPumpRunningTimer_14 and !pump_14_coolDown)
                {  
                    pumpRunning = true;
                    //Serial.print("CurrenntMillis: ");  
                    //Serial.println(currentMillis); 
                    //Serial.print("Case 14: pumpTimer: ");  
                    //Serial.println(pumpTimer_14);
                    //Serial.print("Current - pumpTimer: ");  
                    //Serial.print(currentMillis-pumpTimer_14);
                    //Serial.print(" < ");  
                    //Serial.println(intPumpRunningTimer_14);
                    //Serial.print("coolDownTimer: ");  
                    //Serial.println(intPumpCoolDownTimer_14);
                    //Serial.println("Case 14: pump running:  true");  
                    // turns on pump   coolDownTimer = 30000
                    // if the cool down has been met the pump can be turned back on
                    coolDownTimer=currentMillis;
                }
                else
                  {   
                      if(currentMillis - coolDownTimer< intPumpCoolDownTimer_14)  //  coolDown
                      {  
                        pumpRunning = false;
                        pump_14_coolDown=true;
                        //Serial.print("coolDownTimer: ");  
                        //Serial.print(currentMillis - coolDownTimer);
                        //Serial.print(" < "); 
                        //Serial.print(intPumpCoolDownTimer_14); 
                        //Serial.println("Case 14: pump running:  false");
                       }else
                       {  
                          pumpRunning = false;
                          pump_14_coolDown = false;
                          pump_14_On = false;
                          //Serial.println("ELSE  Case 14: pump running:  false");
                       }
                    }
             break;
                          
        case 15: 
             //Serial.print("CurrenntMillis: ");  
             //Serial.println(currentMillis); 
             //Serial.print("pumpTimer_15: ");  
             //Serial.println(pumpTimer_15); 
             //Serial.print("PumpRunningTimer_15: ");  
             //Serial.println(intPumpRunningTimer_15); 
             currentMillis = millis();
             if(!pump_15_On)
             {    
                pumpTimer_15= currentMillis;
                coolDownTimer = currentMillis;
                pump_15_On = true;
                pump_15_coolDown=false;
             }
              if(currentMillis - pumpTimer_15< intPumpRunningTimer_15 and !pump_15_coolDown)
                {  
                    pumpRunning = true;
                    //Serial.print("CurrenntMillis: ");  
                    //Serial.println(currentMillis); 
                    //Serial.print("Case 15: pumpTimer: ");  
                    //Serial.println(pumpTimer_15);
                    //Serial.print("Current - pumpTimer: ");  
                    //Serial.print(currentMillis-pumpTimer_15);
                    //Serial.print(" < ");  
                    //Serial.println(intPumpRunningTimer_15);
                    //Serial.print("coolDownTimer: ");  
                    //Serial.println(intPumpCoolDownTimer_15);
                    //Serial.println("Case 15: pump running:  true");  
                    // turns on pump   coolDownTimer = 30000
                    // if the cool down has been met the pump can be turned back on
                    coolDownTimer=currentMillis;
                }
                else
                 {   
                    if(currentMillis - coolDownTimer< intPumpCoolDownTimer_15)  //  coolDown
                    {  
                      pumpRunning = false;
                      pump_15_coolDown=true;
                      //Serial.print("coolDownTimer: ");  
                      //Serial.print(currentMillis - coolDownTimer);
                      //Serial.print(" < "); 
                      //Serial.print(intPumpCoolDownTimer_15); 
                      //Serial.println("Case 15: pump running:  false");
                     }
                       else
                       {  
                          pumpRunning = false;
                          pump_15_coolDown = false;
                          pump_15_On = false;
                          //Serial.println("ELSE  Case 15: pump running:  false");
                       }
                   }
             break;
        case 16:  
             //Serial.print("CurrenntMillis: ");  
             //Serial.println(currentMillis); 
             //Serial.print("pumpTimer_16: ");  
             //Serial.println(pumpTimer_16); 
             //Serial.print("intPumpRunningTimer_16: ");  
             //Serial.println(intPumpRunningTimer_16); 
             currentMillis = millis();
             if(!pump_16_On)
             {    
                pumpTimer_16= currentMillis;
                coolDownTimer = currentMillis;
                pump_16_On = true;
                pump_16_coolDown=false;
             }
              if(currentMillis - pumpTimer_16< intPumpRunningTimer_16 and !pump_16_coolDown)
                {  
                    pumpRunning = true;
                    //Serial.print("CurrenntMillis: ");  
                    //Serial.println(currentMillis); 
                    //Serial.print("Case 16: pumpTimer: ");  
                    //Serial.println(pumpTimer_16);
                    //Serial.print("Current - pumpTimer: ");  
                    //Serial.print(currentMillis-pumpTimer_16);
                    //Serial.print(" < ");  
                    //Serial.println(intPumpRunningTimer_16);
                    //Serial.print("coolDownTimer: ");  
                    //Serial.println(intPumpCoolDownTimer_16);
                    //Serial.println("Case 16: pump running:  true");  
                    // turns on pump   coolDownTimer = 30000
                    // if the cool down has been met the pump can be turned back on
                    coolDownTimer=currentMillis;
                }
                else
                  {   
                      if(currentMillis - coolDownTimer< intPumpCoolDownTimer_16)  //  coolDown
                      {  
                        pumpRunning = false;
                        pump_16_coolDown=true;
                        //Serial.print("coolDownTimer: ");  
                        //Serial.print(currentMillis - coolDownTimer);
                        //Serial.print(" < "); 
                        //Serial.print(intPumpCoolDownTimer_16); 
                        //Serial.println("Case 16: pump running:  false");
                       }else
                       {  
                        pumpRunning = false;
                        pump_16_coolDown = false;
                        pump_16_On = false;
                       //Serial.println("ELSE  Case 16: pump running:  false");
                       }
                    }
             break;
        default:
             break; 
      }
      return pumpRunning;
}
    

void checkMoisture(int x)
   {
     boolean pumpRun;
     //read Moisture value of sensor x
     readInputRegisterValues(x);
     switch (x) 
          {
           case 11:
                  pumpRun = checkPumpRunning(x);
                   if(soilMoisture11<lowMoistureThreshold_11 and soilMoisture11>0)
                   {
                     
                       if(pumpRun)
                        { 
                          Watering_11=true;
                          digitalWrite(relay_1, LOW);   
                          //Serial.print("less than "); 
                          //Serial.println(lowMoistureThreshold_11); 
                        }
                        else
                        {     
                          digitalWrite(relay_1, HIGH);   }
                        }
                    else
                    {
                      if(pumpRun)
                        { 
                  
                          if(soilMoisture11<highMoistureThreshold_11 and Watering_11)
                            {
                               digitalWrite(relay_1, LOW);   
                               //Serial.print("watering, still less than "); 
                               //Serial.println(highMoistureThreshold_11);  
                            }
                            else
                             { 
                               digitalWrite(relay_1, HIGH);   
                               //Serial.println("watered 11 "); 
                               Watering_11 = false; 
                             }
                        } 
                        else
                           {    
                              digitalWrite(relay_1, HIGH);  
                           }
                   }
                   break; 
             case 12:
                    pumpRun = checkPumpRunning(x);
                   if(soilMoisture12<lowMoistureThreshold_12 and soilMoisture12>0)
                   {
                     
                       if(pumpRun)
                        { 
                          Watering_12=true;
                          digitalWrite(relay_2, LOW);   
                           //Serial.print("less than "); 
                          //Serial.println(lowMoistureThreshold_12); 
                        }else{     digitalWrite(relay_2, HIGH);   }
                    }
                    else{
                    
                       if(pumpRun)
                        { 
                  
                          if(soilMoisture12<highMoistureThreshold_12 and Watering_12)
                            {
                                digitalWrite(relay_2, LOW);   
                                //Serial.print("watering, still less than "); 
                                //Serial.println(highMoistureThreshold_12); 
                            }
                            else
                             { 
                               digitalWrite(relay_2, HIGH);   
                               //Serial.println("watered 12 "); 
                               Watering_12 = false; 
                             }
                        } 
                        else
                           {     digitalWrite(relay_2, HIGH);   }
                          
                       }
                   break; 
              case 13:
                  pumpRun = checkPumpRunning(x);
                   if(soilMoisture13<lowMoistureThreshold_13 and soilMoisture13>0)
                   {
                     
                       if(pumpRun)
                        { 
                          Watering_13=true;
                          digitalWrite(relay_3, LOW);   
                             //Serial.print("less than "); 
                          //Serial.println(lowMoistureThreshold_13);   
                        }else{     digitalWrite(relay_3, HIGH);   }
                    }
                    else{
                    
                       if(pumpRun)
                        { 
                  
                          if(soilMoisture13<highMoistureThreshold_13 and Watering_13)
                            {
                                digitalWrite(relay_3, LOW);   
                                //Serial.print("watering, still less than "); 
                                //Serial.println(highMoistureThreshold_13); 
                            }
                            else
                             { 
                               digitalWrite(relay_3, HIGH);   
                               //Serial.println("watered 13 "); 
                               Watering_13 = false; 
                             }
                        } 
                        else
                           {     digitalWrite(relay_3, HIGH);   }
                          
                       }
                 
                   break; 
              case 14:
                  pumpRun = checkPumpRunning(x);
                   if(soilMoisture14<lowMoistureThreshold_14 and soilMoisture14>0)
                   {
                     
                       if(pumpRun)
                        { 
                          Watering_14=true;
                          digitalWrite(relay_4, LOW);   
                             //Serial.print("less than "); 
                          //Serial.println(lowMoistureThreshold_14);  
                        }else{     digitalWrite(relay_4, HIGH);   }
                    }
                    else{
                    
                       if(pumpRun)
                        { 
                  
                          if(soilMoisture14<highMoistureThreshold_14 and Watering_14)
                            {
                                digitalWrite(relay_4, LOW);   
                                //Serial.print("watering, still less than "); 
                                //Serial.println(highMoistureThreshold_14); 
                            }
                            else
                             { 
                               digitalWrite(relay_4, HIGH);   
                               //Serial.println("watered 14 "); 
                               Watering_14 = false; 
                             }
                        } 
                        else
                           {     digitalWrite(relay_4, HIGH);   }
                          
                       }
                
                   break; 
              case 15:
                   pumpRun = checkPumpRunning(x);
                   if(soilMoisture15<lowMoistureThreshold_15 and soilMoisture15>0)
                   {
                     
                       if(pumpRun)
                        { 
                          Watering_15=true;
                          digitalWrite(relay_5, LOW);   
                             //Serial.print("less than "); 
                          //Serial.println(lowMoistureThreshold_15); 
                        }else{     digitalWrite(relay_5, HIGH);   }
                    }
                    else{
                    
                       if(pumpRun)
                        { 
                  
                          if(soilMoisture15<highMoistureThreshold_15 and Watering_15)
                            {
                                digitalWrite(relay_5, LOW);   
                                //Serial.print("watering, still less than "); 
                                //Serial.println(highMoistureThreshold_15); 
                            }
                            else
                             { 
                               digitalWrite(relay_5, HIGH);   
                               //Serial.println("watered 15 "); 
                               Watering_15 = false; 
                             }
                        } 
                        else
                           {     digitalWrite(relay_5, HIGH);   }
                          
                       }
                   break; 
              case 16:
                    pumpRun = checkPumpRunning(x);
                   if(soilMoisture16<lowMoistureThreshold_16 and soilMoisture16>0)
                   {
                     
                       if(pumpRun)
                        { 
                          Watering_16=true;
                          digitalWrite(relay_6, LOW);   
                         //Serial.print("less than "); 
                         //Serial.println(lowMoistureThreshold_16); 
                        }else{     digitalWrite(relay_6, HIGH);   }
                    }
                    else{
                    
                       if(pumpRun)
                        { 
                  
                          if(soilMoisture16<highMoistureThreshold_16 and Watering_16)
                            {
                                digitalWrite(relay_6, LOW);   
                                //Serial.print("watering, still less than "); 
                                //Serial.println(highMoistureThreshold_16); 
                            }
                            else
                             { 
                               digitalWrite(relay_6, HIGH);   
                               //Serial.println("watered 16 "); 
                               Watering_16 = false; 
                             }
                        } 
                        else
                           {     digitalWrite(relay_6, HIGH);   }
                          
                       }
                   break; 
                default:
                  //Serial.print("default switch statement for moisture reached");
                  break;   
            }    
}

boolean checkTempStatus()
    {
       return true;
    }
    
    // call to change the ID of Sensor
    
void writeHoldingRegisterValues() 
   {
 //    Serial.println("Write to Holding Register 1 to change ID ... ");
     //  the values are id number, holding register number 0(ID), and last is the new ID value
     ModbusRTUClient.holdingRegisterWrite(1, 0x00, 1);
     if (!ModbusRTUClient.endTransmission()) 
     {
      //Serial.print("failed to connect ");
       // prints error of failure
      //Serial.println(ModbusRTUClient.lastError());
     } 
     else 
        {
        //Serial.println("changed to ID");
        }
}

    //*********************************************************************
    //  HOLDING REGISTER VALUES FOR THE MOISTURE SENSOR
    // Register number  Size (bytes)  Valid values   Default value Description
    //    0                 2             [1 - 247]     1         ID or Modbus address
    //    1                 2             [0 - 7]       4         Baud rate
    //    2                 2             [0 - 2]       0         Parity Note: most cheap ebay USB to RS485 dongles don't support parity properly!
    //    3                 2             [1 - 65535]   500       Measurement interval in milliseconds
    //    4                 2             [1 - 65535]   0         Time to sleep in seconds. Write to this register to put the sensor to sleep.
    //*********************************************************************
    // reads the ID of the moiture sensor
void readHoldingRegisterValues() 
   {
       //Serial.println("Reading ID value 1 ... ");
       // read 1 Input Register value from (slave) ID 1, address 0x00
       if (!ModbusRTUClient.requestFrom(1, HOLDING_REGISTERS, 0x00, 1)) 
        {
        // Serial.print("failed to connect ");
       // Serial.println(ModbusRTUClient.lastError());
        }
        else 
           {
          //Serial.println("the ID is ");
              while (ModbusRTUClient.available()) 
                {
                   Serial.print(ModbusRTUClient.read());
                Serial.print(' ');
                }
            //Serial.println();
          }
   }
   
   // reads the serial speed/baud rate to the sensor, default is 4 which is 19200
void readHoldingRegisterValues2()
   {
      //Serial.println("Reading Holding 2 Input Register values for Baud Rate ... ");
       // read 1 Input Register value from (slave) id 1, address 0x01
      if (!ModbusRTUClient.requestFrom(1, HOLDING_REGISTERS, 0x01, 1))
      {
         //Serial.print("failed to connect ");
         //Serial.println(ModbusRTUClient.lastError());
      } 
      else 
         {
          //Serial.println("the baud rate is ");
           while (ModbusRTUClient.available())  
             {
               Serial.print(ModbusRTUClient.read());
               Serial.print(' ');
             }
           //Serial.println();
         }
   }
   
// READ HOLDING REGISTER value for Parity
void readHoldingRegisterValues3() 
   {
    //Serial.println("Reading Holding 3 Input Register values for Parity ... ");
    //  delay(500);
      // read 1 Input Register value from (slave) id 1, address 0x02
    if (!ModbusRTUClient.requestFrom(1, HOLDING_REGISTERS, 0x02, 1)) 
      {
       //Serial.print("failed to connect ");
       //Serial.println(ModbusRTUClient.lastError());
      } 
      else 
        {
        //Serial.println("the parity is ");
          while (ModbusRTUClient.available()) 
            {
              Serial.print(ModbusRTUClient.read());
              Serial.print(' ');
            }
          //Serial.println();
        }
   }
   
// READ HOLDING REGISTER value for Interval default is 500
void readHoldingRegisterValues4() 
  {
     //Serial.println("Reading Holding 4 Input Register values for Interval(500) ... ");
     //delay(500);
     // read 1 Input Register value from (slave) id 1, address 0x03
    if (!ModbusRTUClient.requestFrom(1, HOLDING_REGISTERS, 0x03, 1))
      {
        //Serial.print("failed to connect ");
        //Serial.println(ModbusRTUClient.lastError());  
      } 
      else 
        {
         //Serial.println("the interval delay is ");
          while (ModbusRTUClient.available()) 
          {
            
            Serial.print(ModbusRTUClient.read());
            Serial.print(' ');
          }
          //Serial.println();
        }
  }
//###################################################################################################################
//###################################################################################################################
//  READ  Moisture and Temp for ID (x)
//###################################################################################################################
//   format is (id, type of read/write, register, how many registers)
//             (x,INPUT_REGISTERS, 0x00,2)
//
void readInputRegisterValues(int x) 
  {
    soilMoisture=0;
    // read 2 input values from (slave)ID (x) for moisture and temperature
    if (!ModbusRTUClient.requestFrom(x, INPUT_REGISTERS, 0x00,2)) 
    {
      Serial.print("failed to connect to sensor ");
      Serial.print(x);
      Serial.print(", error ");
   Serial.println(ModbusRTUClient.lastError());
    } 
    else 
       {
            Serial.print("temp for ");
            Serial.print(x);
            Serial.print(" is: ");

            while (ModbusRTUClient.available()) 
          {
              soilMoisture = ModbusRTUClient.read();
              soilTempC= ModbusRTUClient.read();
              Serial.println(soilTempC);
             // Serial.print(soilMoisture);
          }
          //Serial.println();
          convertTemp(x);
          
          switch (x) {
            case 1:
              //Serial.println("!!! the ID wasnt changed from ID #1 !!!!");
           //   soilMoisture11= soilMoisture;
              break;
            case 11:
              soilMoisture11= soilMoisture;
              break;
            case 12:
              soilMoisture12= soilMoisture;
              break;
            case 13:
              soilMoisture13= soilMoisture;
              break;
            case 14:
              soilMoisture14= soilMoisture;
                  Serial.print(" soilMoisture14 = ");
                   Serial.println(soilMoisture14);
              break;
            case 15:
              soilMoisture15= soilMoisture;
              break;
            case 16:
              soilMoisture16= soilMoisture;
                 Serial.print(" soilMoisture16 = ");
                 Serial.println(soilMoisture16);
              break; 
            default:
              //Serial.print("default switch statement for moisture reached");
              break;
          }   
       }
  }
  
// converts the temp from raw temp C to temp F
void convertTemp(int x) 
   {
           switch (x) 
              {
                case 1:       
                   soilTemp12F = setDegrees(soilTempC);
                   break;
                case 11:
                  soilTemp11F = setDegrees(soilTempC);
                  break;
                case 12:
                  soilTemp12F = setDegrees(soilTempC);
                  break;
                case 13:
                   soilTemp13F = setDegrees(soilTempC);
                   break;
                case 14:
                   soilTemp14F = setDegrees(soilTempC);
                   Serial.print(" soilTemp14F is ");
                   Serial.println(soilTemp14F);
                   break;
                case 15:
                   soilTemp15F = setDegrees(soilTempC);
                   break;
                case 16:
                   soilTemp16F = setDegrees(soilTempC);
                   Serial.print(" soilTemp16F is ");
                   Serial.println(soilTemp16F);
                   break; 
                default:
                  //Serial.print("default switch statement for moisture reached");
                  break;     
              }
     }
   
   
float setDegrees(int16_t Celcius)
   {
      float Fcelcius = Celcius/10;
      return (Fcelcius* 9.0 +2)/5.0+32;
   }

void printCurrentNet() 
   {
          // print the SSID of the network you're attached to:
          Serial.print("SSID: ");
          Serial.println(WiFi.SSID());
          // print the MAC address of the router you're attached to:
          WiFi.BSSID(bssid);
          Serial.print("BSSID: ");
          Serial.print(bssid[5], HEX);
          Serial.print(":");
          Serial.print(bssid[4], HEX);
          Serial.print(":");
          Serial.print(bssid[3], HEX);
          Serial.print(":");
          Serial.print(bssid[2], HEX);
          Serial.print(":");
          Serial.print(bssid[1], HEX);
          Serial.print(":");
          Serial.println(bssid[0], HEX);
          // print the received signal strength:
          rssi = WiFi.RSSI();
          Serial.print("signal strength (RSSI):");
          Serial.println(rssi);
          // print the encryption type:
          encryption = WiFi.encryptionType();
          Serial.print("Encryption Type:");
          Serial.println(encryption, HEX);
          Serial.println();
   }

void printWiFiData() 
   {
          // print your WiFi shield's IP address:
           ip = WiFi.localIP();
         
          IPAddress dns(8, 8, 8, 8);  //Google dns  
          // the IP address for the shield:
          Serial.print("IP Address: ");
          Serial.println(ip); 
          Serial.println(ip);
   }
//////////////////////  
/////Ping Google//////
//////////////////////
void pingG()
   {
  
      //pinging
//      Serial.print("Pinging ");
//      Serial.print(hostName);
//      Serial.print(": ");
      pingResult = WiFi.ping(hostName);
 //     if (pingResult >= 0) 
 //       {
         //   Serial.print("SUCCESS! RTT = ");
         //   Serial.print(pingResult);
         //   Serial.println(" ms");
 //        } 
 //        else 
//          {
        //     Serial.print("FAILED! Error code: ");
       //      Serial.println(pingResult);
  //         }
  
   }

      ///////////////////////////////
      ////Send stream to Carriots///
      ///////////////////////////////
    
void sendStream() 
   {      
       //get time
       epoch = timeClient.getEpochTime(); 
       if (wifiClient.connect("api.carriots.com",serverPort)) 
          {  
             // If there's a successful connection
             Serial.println("");
             Serial.println("connected to api.carriots");
             // Build the data field
             rssi = WiFi.RSSI();
             rssi = 100-(-1*rssi);
             
             String json = "{\"protocol\":\"v2\",\"device\":\""+DEVICE+
             "\",\"at\":"+epoch+
             ",\"data\":{\"WIFI_RSSI\":\""+rssi+
             "\",\"Average_ping_time_to_Google\":\""+pingAverage+
             "\",\"soilMoisture_11\":\""+soilMoisture11+
             "\",\"soilMoisture_12\":\""+soilMoisture12+
             "\",\"soilMoisture_13\":\""+soilMoisture13+
             "\",\"soilMoisture_14\":\""+soilMoisture14+
             "\",\"soilMoisture_15\":\""+soilMoisture15+
             "\",\"soilMoisture_16\":\""+soilMoisture16+
             "\",\"soilTemp_11F\":\""+soilTemp11F+
             "\",\"soilTemp_12F\":\""+soilTemp12F+
             "\",\"soilTemp_13F\":\""+soilTemp13F+
             "\",\"soilTemp_14F\":\""+soilTemp14F+
             "\",\"soilTemp_15F\":\""+soilTemp15F+
             "\",\"soilTemp_16F\":\""+soilTemp16F+
             "\",\"Controller_Temp\":\""+temp_F+
             "\",\"Controller_Humidity\":\""+val+
             "\"}}";
            
             Serial.println(json);
             // Make a HTTP request
             wifiClient.println("POST /streams HTTP/1.1");
             wifiClient.println("Host: api.carriots.com");
             wifiClient.println("Accept: application/json");
             wifiClient.println("User-Agent: Arduino-Carriots");
             wifiClient.println("Content-Type: application/json"); 
             wifiClient.print("carriots.apikey: ");
             wifiClient.println(APIKEY);
             wifiClient.print("Content-Length: ");
             int thisLength = json.length();
             wifiClient.println(thisLength);
             wifiClient.println("Connection: close");
             wifiClient.println();
             wifiClient.println(json);
             wifiClient.stop();
        }
             else 
               {
                  // If you didn't get a connection to the server:
                  //Serial.println("sendStream connection failed");                                                                        
                  while (WL_CONNECTION_LOST) 
                  {
                      // wifiClient.stop();
                      //Serial.print("Attempting to connect to WPA SSID: ");
                      //Serial.println(ssid);
                      // unsuccessful, retry in 4 seconds
                      status = WiFi.begin(ssid, pass);
                      delay(10000);
                  }
               }
   }
void sendStatusStream() 
   {        
        //get time
       epoch =  timeClient.getEpochTime();
       if (wifiClient.connect("api.carriots.com",serverPort)) 
         {   // If there's a successful connection
             //Serial.println("");
             //Serial.println("connected to api.carriots");
             // Build the data field
             rssi = WiFi.RSSI();
             rssi = 100-(-1*rssi);
             String json = "{\"protocol\":\"v2\",\"device\":\""+DEVICE+"\",\"at\":"+epoch+",\"data\":{\"WIFI_RSSI\":\""+rssi+"\",\"Wifi_Firmware_Version\":\""+wifiFirmware+"\",\"Config_Version\":\""+softwareFirmware+"\"}}";
            Serial.println(json);
             // Make a HTTP request
             wifiClient.println("POST /status HTTP/1.1");
                   // Serial.println(1);
             wifiClient.println("Host: api.carriots.com");
                   // Serial.println(2);
             wifiClient.println("Accept: application/json");
                   //Serial.println(3);
             wifiClient.println("User-Agent: Arduino-Carriots");
                   //  Serial.println(4);
             wifiClient.println("Content-Type: application/json"); 
                   // wifiClient.println("Content-Type: text/html");                     
             wifiClient.print("carriots.apikey: ");
                   // Serial.println(6);
             wifiClient.println(APIKEY);
                   // Serial.println(7);
             wifiClient.print("Content-Length: ");
             int thisLength = json.length();
             wifiClient.println(thisLength);
             wifiClient.println("Connection: close");
             wifiClient.println();
             wifiClient.println(json);
             wifiClient.stop();
        }
        else 
           {
                // If you didn't get a connection to the server:
                //Serial.println("sendStream connection failed");                                                                        
                while (status != WL_CONNECTED)
                {
                  wifiClient.stop();
                  //Serial.print("Attempting to connect to WPA SSID: ");
                  //Serial.println(ssid);
                  // unsuccessful, retry in 10 seconds
                  status = WiFi.begin(ssid, pass);
                  delay(10000);
                }
            }
   }
