/*
* @Author : Gaurav Barwalia
* @Version : 1.0
* @Date : 14th March'2020

* Application summary : Make a POST API call to HTTPs target server and show returned msg
* Main points : -
* This software is desined for running in ESP32 boards and tested on V1.0.4 arduino core for ESP32.
* It uses two physical push buttons to trigger API call function, you have press these buttons for more than 1 second.
* It uses one physical push buttons to trigger WiFimanager based configuration portal, where SSID, wifi_Password, HTTPs POST details can be captured using a security key.
* It uses one physical push buttons to trigger HTTP OTA firmware upgrade, updated software will be available on gbinfosystems.store
* LCD_I2C display shows necessary information.
* Internet connection disconnection getting handled automatically, it pings google.com every 30 seconds.
* It uses a root ssl certificate from the target server, it is hardcoded. please change it accordingly when connecting to different target.
* http://maker.ifttt.com/trigger/google_doc_update/with/key/REDACTED_IFTTT_KEY : This will update google doc named [api_call_update] associated with gaurav.gbaba@gmail.com with timestamp of API call 
* 26-04-2020 : added this project to gihub and started tracking file version using git
// Sketch uses 1076362 bytes (82%) of program storage space. Maximum is 1310720 bytes. // 1.2 Mb for OTA and 1.2 Mb for application program and 1.5 Mb for SPIFFS
// Global variables use 42256 bytes (12%) of dynamic memory, leaving 285424 bytes for local variables. Maximum is 327680 bytes.
// Working :  ESP32 core for arduino v1.0.4
*/

#include <Arduino.h>           // For coding in platformIO using arduino framework, we should include Arduino.h
#include <WiFiManager.h>       // https://github.com/tzapu/WiFiManager  // using development branch code, it supports ESP32
#include <WiFiClientSecure.h>  // for SSL wifi secure client //https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFiClientSecure/src
#include <ArduinoJson.h>       // For JSON data serialization or deserialization // V6.14.0 working //https://github.com/bblanchon/ArduinoJson/tree/6.x/src
#include <SPIFFS.h>            // file system // mount fails for new ESP32 board, first run SPIFFS_TEST program, it will format the board first, then it will work //https://github.com/espressif/arduino-esp32/blob/master/libraries/SPIFFS/src/SPIFFS.h
#include <SimpleTimer.h>       // for scheduling the functions // https://github.com/jfturcot/SimpleTimer
#include <HTTPUpdate.h>        // HTTP OTA upgrade // https://github.com/espressif/arduino-esp32/tree/master/libraries/HTTPUpdate/src
#include <LiquidCrystal_I2C.h> // I2C LCD library, it works for all arduino supported board // https://github.com/johnrickman/LiquidCrystal_I2C
#include <Wire.h>              // for I2C // https://github.com/espressif/arduino-esp32/blob/master/libraries/Wire/src/Wire.h
//#include <ArduinoOTA.h>      // basic arduino OTA library, no need
//#include <HTTPClient.h>      // included in <HTTPUpdate.h>, for get request to google.com and OTA
//#include <FS.h>              // already included in <SPIFFS.h>, no need to add
//#include <WiFiMulti.h>       // no need
//#include <WiFi.h>            // already included in <wifimanager.h>, main library for wifi.begin() connection

//WiFiMulti WiFiMulti;
LiquidCrystal_I2C lcd(0x27, 16, 2); // 0x27 is standard address for most of the 16x2 LCD, use this by default

SimpleTimer timer; // initialize a SimpleTimer object

const size_t capacity = JSON_OBJECT_SIZE(6) + 2000; //  + bytes suggested by ArduinoJson assistant
DynamicJsonDocument doc(capacity);                  // standard


// SPIFFS auto format
#define FORMAT_SPIFFS_IF_FAILED false // keep it false, we should not FORMAT SPIFFS on failure, it will erase the data

// Pin declarations
#define Wifimanager_trigger 18 // will trigger wifimanager portal on GPIO18 LOW
#define API_Call_Button_1 22   // Will trigger API call when GPIO22 and GPIO23 are LOW together
#define API_Call_Button_2 23   // Will trigger API call when GPIO22 and GPIO23 are LOW together
#define HTTP_OTA_UPDATE_PIN 19 // will trigger HTTP OTA upgrade on GPIO19 LOW
#define indicator_LED 21       // Indicator LED on GPIO21 for internet connection, it will blink when no internet is available
#define SDA 16                 // I2C data pin for LCD display
#define SCL 17                 // I2C clock pin for LCD display

// Client details
//char Client_name[20] = "Millennium Tiles";
char Client_name[20] = "GB Info Systems!";
//char Client_location[20] = " Morbi, Gujarat "; // for LCD , 16x2
char Client_location[20] = "Pune,Maharashtra"; // for LCD , 16x2
char Client_Software[20] = "ERPNext";

// WiFi credentials
char ssid[200];
char wifi_password[200];

// Internet and indicator handling
bool WifiOK = false;
bool InternetOK = false;
int indicator_LED_state = LOW;

// HTTP OTA upgrade
int file_version = 1;
const char *version_url = "http://gbinfosystems.store/HTTP_Update_Production/API_Call_Device/Millennium_Tiles/version_file.version"; // change URL here
const char *code_url = "http://gbinfosystems.store/HTTP_Update_Production/API_Call_Device/Millennium_Tiles/code_file.bin";           // change URL here
const char *Google_docs_url = "http://maker.ifttt.com/trigger/google_doc_update/with/key/REDACTED_IFTTT_KEY";

// WIfimanager custom parameters
char HTTP_host[100] = "tileexporter.in";
char HTTP_url[300] = "/api/method/ceramic.api.restrict_access";
char HTTP_body[300] = ".then(response => response.text()) .then(result => console.log(result)) .catch(error => console.log('error', error));";
char Auth_token[100] = "token REDACTED_API_TOKEN";
char Security_key[20] = "qwertyuiop"; // for saving above parameters, this security key need to to verified

// Captive portal timeout
int timeout = 120; // seconds to run for

//flag for saving data from captive portal
bool shouldSaveConfig = false;

//Config portal name and password declaration
const char *Config_Portal_name = "Configuration Portal";
const char *Config_Portal_Password = "qwertyuiop";

//MAC address
String MACvalue;

// providing this root certificate for HTTPs handshake between this device and the end website
// change is root code for end website, this is for "tileexporter.in"
const char *root_ca =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDSjCCAjKgAwIBAgIQRK+wgNajJ7qJMDmGLvhAazANBgkqhkiG9w0BAQUFADA/\n"
    "MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n"
    "DkRTVCBSb290IENBIFgzMB4XDTAwMDkzMDIxMTIxOVoXDTIxMDkzMDE0MDExNVow\n"
    "PzEkMCIGA1UEChMbRGlnaXRhbCBTaWduYXR1cmUgVHJ1c3QgQ28uMRcwFQYDVQQD\n"
    "Ew5EU1QgUm9vdCBDQSBYMzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n"
    "AN+v6ZdQCINXtMxiZfaQguzH0yxrMMpb7NnDfcdAwRgUi+DoM3ZJKuM/IUmTrE4O\n"
    "rz5Iy2Xu/NMhD2XSKtkyj4zl93ewEnu1lcCJo6m67XMuegwGMoOifooUMM0RoOEq\n"
    "OLl5CjH9UL2AZd+3UWODyOKIYepLYYHsUmu5ouJLGiifSKOeDNoJjj4XLh7dIN9b\n"
    "xiqKqy69cK3FCxolkHRyxXtqqzTWMIn/5WgTe1QLyNau7Fqckh49ZLOMxt+/yUFw\n"
    "7BZy1SbsOFU5Q9D8/RhcQPGX69Wam40dutolucbY38EVAjqr2m7xPi71XAicPNaD\n"
    "aeQQmxkqtilX4+U9m5/wAl0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNV\n"
    "HQ8BAf8EBAMCAQYwHQYDVR0OBBYEFMSnsaR7LHH62+FLkHX/xBVghYkQMA0GCSqG\n"
    "SIb3DQEBBQUAA4IBAQCjGiybFwBcqR7uKGY3Or+Dxz9LwwmglSBd49lZRNI+DT69\n"
    "ikugdB/OEIKcdBodfpga3csTS7MgROSR6cz8faXbauX+5v3gTt23ADq1cEmv8uXr\n"
    "AvHRAosZy5Q6XkjEGB5YGV8eAlrwDPGxrancWYaLbumR9YbK+rlmM6pZW87ipxZz\n"
    "R8srzJmwN0jP41ZL9c8PDHIyh8bwRLtTcm1D9SZImlJnt1ir/md2cXjbDaJWFBM5\n"
    "JDGFoqgCWjBH4d1QB7wCCZAA62RjYJsWvIjJEubSfZGL+T0yjWW06XyxV3bqxbYo\n"
    "Ob8VZRzI9neWagqNdwvYkQsEjgfbKbYK7p2CNTUQ\n"
    "-----END CERTIFICATE-----\n";

// function for reading saved json data from SPIFFS memory
void ReadConfigJson()
{
  if (SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json"))
    {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");
        delay(100);
        //const size_t capacity = JSON_OBJECT_SIZE(8) + 400; //  + bytes suggested by ArduinoJson assistant
        //DynamicJsonDocument doc(capacity);
        DeserializationError error = deserializeJson(doc, configFile);
        // ESP.wdtFeed();
        if (error)
        {
          Serial.print(F("deserializeJson() failed: "));
        }
        Serial.print("deserializeJson() without error. ");

        // Copy json values to respective variables from the file system
        strlcpy(ssid, doc["Wifi_SSID_Json"] | "default", sizeof(ssid));
        strlcpy(wifi_password, doc["Wifi_Pass_Json"] | "default", sizeof(wifi_password));
        strlcpy(HTTP_host, doc["Wifi_HTTPs_HOST_Json"] | "tileexporter.in", sizeof(HTTP_host));
        strlcpy(HTTP_url, doc["Wifi_HTTPs_URL_Json"] | "/api/method/ceramic.api.restrict_access", sizeof(HTTP_url));
        strlcpy(HTTP_body, doc["Wifi_HTTPs_BODY_Json"] | ".then(response => response.text()) .then(result => console.log(result)) .catch(error => console.log('error', error));", sizeof(HTTP_body));
        strlcpy(Auth_token, doc["Wifi_HTTPs_AUTH_TOKEN_Json"] | "token REDACTED_API_TOKEN", sizeof(Auth_token));

        //Serial.println(ssid);
        //Serial.println(wifi_password);

        // serializeJson(doc, Serial);
      }
      else
      {
        Serial.println("failed to load json config");
      }

      configFile.close();
    }
  }
  else
  {
    Serial.println("failed to mount FS");
  }
}

//function for updating saved json data in SPIFFS
void UpdateJsonString(String x1, String x2, String x3, String x4, String x5, String x6)
{

  if (SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json"))
    {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");
        //const size_t capacity = JSON_OBJECT_SIZE(8) + 400; //  + bytes suggested by ArduinoJson assistant
        //DynamicJsonDocument doc(capacity);
        deserializeJson(doc, configFile);
        //ESP.wdtFeed();
      }
      else
      {
        Serial.println("failed to load json config");
      }
      configFile.close();
    }
  }
  else
  {
    Serial.println("failed to mount FS");
  }

  doc["Wifi_SSID_Json"] = x1;
  doc["Wifi_Pass_Json"] = x2;
  doc["Wifi_HTTPs_HOST_Json"] = x3;
  doc["Wifi_HTTPs_URL_Json"] = x4;
  doc["Wifi_HTTPs_BODY_Json"] = x5;
  doc["Wifi_HTTPs_AUTH_TOKEN_Json"] = x6;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    //Serial.println("failed to open config file !!");
  }

  serializeJson(doc, configFile);
  //delay(100);
  //serializeJson(doc, Serial);
  configFile.close();
}

/*
void GoogleDocsAPIcallLogging()
{
  if (InternetOK == true) {
    Serial.println("wifi ok !!");
   HTTPClient HTTPClient; 
    HTTPClient.begin(Google_docs_url);
    int http_response = HTTPClient.GET(); // check response
            if (http_response == 200) {
              Serial.println("Google docs updated.. !!");
            }
            else
            {
              Serial.println("Google docs could not get updated.. !!");
            }
            HTTPClient.end(); // end
  }
}
*/

// Main function to handle the API call
void POSTapiCall()
{
  if (InternetOK == true)
  {
    lcd.clear();

    lcd.print("-- Connecting --");
    lcd.setCursor(0, 1);
    //lcd.print("ERPNext server.."); // it should be a variable, as each client uses a different software for API call
    //lcd.print(Client_Software); // it should be a variable, as each client uses a different software for API call
    //lcd.setCursor(0, 1);
    lcd.print("....");
    delay(1000);

    digitalWrite(indicator_LED, HIGH);

    ReadConfigJson(); // read saved data

    Serial.print("Connecting to ");
    Serial.println(HTTP_host);
    Serial.println(HTTP_url);

    WiFiClientSecure client; // initialize a secure client

    if (!client.connect(HTTP_host, 443))
    { // try to connect with the HOST server on 443 port, that is HTTPs port

      lcd.setCursor(0, 1);
      lcd.print("Connection fail.");
      Serial.println("Could not connect to the client..!!");
      //Serial.println("Waiting 5 seconds before retrying...");
      Serial.println("Please retry...");
      //delay(5000);
      return;
    }
    else
    { // if connected..

      // this is the standard way to create a POST request
      String postRequest =
          String("POST ") + HTTP_url + " HTTP/1.1\r\n" +
          "Host: " + HTTP_host + "\r\n" +
          "Accept: application/json\r\n" +
          "Content-Type: application/x-www-form-urlencoded\r\n" +
          "Authorization: " + Auth_token + "\r\n" +
          "redirect: follow\r\n" +
          "Content-Length: " + String(HTTP_body).length() + "\r\n" +
          "\r\n" + HTTP_body;

      Serial.println(postRequest);
      client.print(postRequest); // Request to the server

      lcd.setCursor(4, 1);
      lcd.print("....");
      delay(1000);

      // read incoming bytes from the server
      while (client.connected())
      {
        String line = client.readStringUntil('\n');
        if (line == "\r")
        {
          Serial.println("headers received");
          break;
        }
      }

      // if there are incoming bytes available
      // from the server, read them and print them:

      String w;
      char Success_Message[100];
      char Error_Message[100];

      //char c;
      while (client.available())
      {
        char c = client.read();
        w = w + String(c); // add all received characters
                           //Serial.write(c);

        //DeserializationError error = deserializeJson(doc, c);
      }
      Serial.println(w);

      lcd.setCursor(8, 1);
      lcd.print("....");
      delay(1000);

      DeserializationError error = deserializeJson(doc, w); // as the received data in json, deserialize it
      if (error)
      {
        Serial.println(F("deserializeJson() failed: "));
        lcd.setCursor(0, 1);
        lcd.print("Error occured...");
        delay(2000);
      }
      else
      {
        strlcpy(Success_Message, doc["message"] | "default", 8); // length should be +1
        strlcpy(Error_Message, doc["exc_type"] | "default", 20); // size +1

        String Success_Message_string = String(Success_Message);
        Success_Message_string.toLowerCase(); // convert to lower case, in case we receive uppercase data, it happend
        //Serial.println(Success_Message_string);

        String Error_Message_string = String(Error_Message);
        Error_Message_string.toLowerCase();
        //Serial.println(Error_Message_string);

        lcd.setCursor(12, 1);
        lcd.print("....");
        delay(1000);

        Serial.println(Success_Message);
        // if (strcmp(Success_Message, "success") == 0) {
        if (Success_Message_string == "success")
        {
          //GoogleDocsAPIcallLogging(); // update google docs sheet named [api_call_update] with API call timestamp
          Serial.println("Yo, you did it GB.. Success !!");
          lcd.clear();
          lcd.print("  Successfully  ");
          lcd.setCursor(0, 1);
          lcd.print("   Processed..  ");
          delay(3000);
        }
        else
        {
          Serial.println(Error_Message);
          //if(strcmp(Error_Message, "authenticationerror") == 0)
          if (Error_Message_string == "authenticationerror")
          {
            //GoogleDocsAPIcallLogging(); // update google docs sheet named [api_call_update] with API call timestamp
            //Serial.println(Error_Message);
            Serial.println("AuthenticationError.. !!");
            lcd.clear();
            lcd.print(" Authentication ");
            lcd.setCursor(0, 1);
            lcd.print("error occured...");
            delay(3000);
          }
          else
          {
            // GoogleDocsAPIcallLogging(); // update google docs sheet named [api_call_update] with API call timestamp
            Serial.println("Some error occured..");
            lcd.clear();
            lcd.print("      some      ");
            lcd.setCursor(0, 1);
            lcd.print("error occured...");
            delay(3000);
          }
        }
      }

      client.stop(); // stop the client once done
    }
  }
  else
  {
    Serial.println("No internet.. !!");
    lcd.setCursor(0, 1);
    lcd.print(" No internet... ");
  }
}

void LcdMonitor()
{
  Serial.println("");
  Serial.println("LcdMonitor function called..!!");
  if (InternetOK == true)
  {
    lcd.clear();
    lcd.println(Client_name);
    lcd.setCursor(0, 1);
    lcd.print("    Ready!!!    ");
  }
  else
  {
    lcd.clear();
    lcd.println(Client_name);
    lcd.setCursor(0, 1);
    lcd.print(" No internet... ");
  }
}

//callback notifying us of the need to save config received from wifimanager captive portal
void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

//Main function to handle configuration change using wifimanger coptive portal
void HandleWifimanager()
{
  if (digitalRead(Wifimanager_trigger) == LOW)
  {

    lcd.clear();
    lcd.print(" Configuration  ");
    lcd.setCursor(0, 1);
    lcd.print("  initiated..  ");
    delay(3000);

    digitalWrite(indicator_LED, LOW);
    WiFiManager wm; // initialize wifimanager

    wm.setSaveConfigCallback(saveConfigCallback); // for saving the configuration
    wm.setConfigPortalTimeout(timeout);           // set captive portal timeout
    wm.setClass("invert");                        // dark theme
    wm.setScanDispPerc(true);                     // display percentages instead of graphs for RSSI

    //reset settings - for testing
    //wifiManager.resetSettings();

    // declare custom parameters
    //(ID, label, default value, lenght)
    WiFiManagerParameter custom_HTTP_host("HOST", "HTTPs HOST", "", 100);
    WiFiManagerParameter custom_HTTP_url("URL", "HTTPs URL", "", 300);
    WiFiManagerParameter custom_HTTP_body("BODY", "HTTPs BODY", "", 300);
    WiFiManagerParameter custom_Auth_token("Token", "HTTPs Authentication Token", "", 100);
    WiFiManagerParameter custom_Security_key("KEY", "Security KEY", "", 20);

    // add parameteres
    wm.addParameter(&custom_HTTP_host);
    wm.addParameter(&custom_HTTP_url);
    wm.addParameter(&custom_Auth_token);
    wm.addParameter(&custom_HTTP_body);
    wm.addParameter(&custom_Security_key);

    // create wifi with provided name and password
    //if (!wm.startConfigPortal("TaxSaving Device", "qwertyuiop")) {
    if (!wm.startConfigPortal(Config_Portal_name, Config_Portal_Password))
    {
      Serial.println("failed to connect and hit timeout");
      lcd.clear();
      lcd.print("     Portal     ");
      lcd.setCursor(0, 1);
      lcd.print("  time out..    ");
      delay(3000);
    }
    delay(6000);
    if (WiFi.status() == WL_CONNECTED)
    {
      strcpy(ssid, wm.getWiFiSSID().c_str()); // store saved wifi SSID to ssid variable
      strcpy(wifi_password, wm.getWiFiPass().c_str());
      Serial.println("Yes, connected, saving data to ssid and wifi_password");
      Serial.println(ssid);
      Serial.println(wifi_password);
    }
    else
    {
      Serial.println("Not connected..");
      Serial.println(ssid);
      Serial.println(wifi_password);
      // strcpy(ssid, wm.getWiFiSSID().c_str()); // store saved wifi SSID to ssid variable
      // strcpy(wifi_password, wm.getWiFiPass().c_str());
      // Serial.println(ssid);
      //Serial.println(wifi_password);
    }

    //if you get here you have connected to the WiFi
    //Serial.println("connected...yeey :)");

    strcpy(Security_key, custom_Security_key.getValue());

    // copy the configured data if the security matches
    if (strcmp(Security_key, "qwertyuiop") == 0)
    {
      Serial.println("Security key is correct");
      strcpy(HTTP_host, custom_HTTP_host.getValue());
      strcpy(HTTP_url, custom_HTTP_url.getValue());
      strcpy(HTTP_body, custom_HTTP_body.getValue());
      strcpy(Auth_token, custom_Auth_token.getValue());
    }

    if (shouldSaveConfig)
    {
      Serial.println("saving config");
      //UpdateJsonString(String(ssid), String(wifi_password)); // save these data in SPIFFS
      UpdateJsonString(String(ssid), String(wifi_password), String(HTTP_host), String(HTTP_url), String(HTTP_body), String(Auth_token));

      lcd.clear();
      lcd.print("     Saving     ");
      lcd.setCursor(0, 1);
      lcd.print(" Configuration..");
      delay(3000);
    }
    LcdMonitor();
  }
}

//Main function for HTTP OTA firmware upgrade
void HttpOtaUpdate()
{
  if (digitalRead(HTTP_OTA_UPDATE_PIN) == LOW)
  {
    if (InternetOK == true)
    {

      Serial.println("wifi ok !!");
      Serial.println("Checking for firmware update !!");
      HTTPClient HTTPClient; // initialize
      WiFiClient client;
      Serial.println("[HTTP] begin...");
      HTTPClient.begin(version_url);

      int http_response = HTTPClient.GET(); // check response
      if (http_response == 200)
      {
        Serial.println("Site accesible..");
        String firmware_version = HTTPClient.getString();
        int new_file_version = firmware_version.toInt();
        if (new_file_version > file_version)
        { // proceed if new version avaialbe

          lcd.clear();
          lcd.print("  New version   ");
          lcd.setCursor(0, 1);
          lcd.print("   available..  ");
          delay(2000);

          lcd.clear();
          lcd.print(" Update process ");
          lcd.setCursor(0, 1);
          lcd.print("  initiated...  ");
          delay(2000);

          lcd.clear();
          lcd.print(" Device restarts ");
          lcd.setCursor(0, 1);
          lcd.print(" after update.. ");
          delay(2000);

          Serial.println("New version available..");
          Serial.println("Preparing for update..");

          t_httpUpdate_return ret = httpUpdate.update(client, code_url); // upgrade
          switch (ret)
          {
          case HTTP_UPDATE_FAILED:
            Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            lcd.clear();
            lcd.print("   connection   ");
            lcd.setCursor(0, 1);
            lcd.print(" error occured..");
            delay(2000);
            break;

          case HTTP_UPDATE_NO_UPDATES:
            Serial.println("HTTP_UPDATE_NO_UPDATES");
            lcd.clear();
            lcd.print("   connection   ");
            lcd.setCursor(0, 1);
            lcd.print(" error occured..");
            delay(2000);
            break;

          case HTTP_UPDATE_OK:
            Serial.println("HTTP_UPDATE_OK");
            lcd.clear();
            lcd.print(" Update process ");
            lcd.setCursor(0, 1);
            lcd.print("  successful..  ");
            delay(2000);
            break;
          }
        }
        else
        {
          Serial.println("Running latest firmware !!");
          lcd.clear();
          lcd.print(" Running latest ");
          lcd.setCursor(0, 1);
          lcd.print(" software ");
          //lcd.setCursor(0, 13);
          lcd.print(file_version);
          lcd.print(".0 ");
          delay(2000);
        }
      }
      else
      {
        Serial.println("No file available..");
        lcd.clear();
        lcd.print("   connection   ");
        lcd.setCursor(0, 1);
        lcd.print(" error occured..");
        delay(2000);
      }
      HTTPClient.end(); // end
    }
    else
    {
      Serial.println("No internet.. !!");
      lcd.setCursor(0, 1);
      lcd.print(" No internet... ");
    }
    LcdMonitor();
  }
}

// function for checking internet connectivity by pinging google.com
void CheckInternetConnectivity()
{
  if (WifiOK == true)
  {
    HTTPClient HTTPClient;
    HTTPClient.begin("http://www.google.com/");
    int http_response = HTTPClient.GET();
    if (http_response == 200)
    {
      Serial.println("Google.com ping successful, internet working..!!");
      InternetOK = true;
    }
    else
    {
      InternetOK = false;
      Serial.println("Could not ping google.com, no internet..!!");
    }
    HTTPClient.end();
  }

  LcdMonitor();
}

// function for checking wifi status, if not connected then try connection
void WiFiStatus()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WIfi not connected, trying to re-connect.. !!!");
    WiFi.begin(ssid, wifi_password);
    WifiOK = false;
    InternetOK = false;
    LcdMonitor();
  }
  else
  {
    WifiOK = true;
    CheckInternetConnectivity();
    //Serial.println("Wifi Check OK !!");
  }
}

// function for indicator LED status update as per internet availability
void IndicatorLED()
{
  if (InternetOK == false) // when not connected to the internet
  {
    indicator_LED_state = !digitalRead(indicator_LED);
    digitalWrite(indicator_LED, indicator_LED_state);
  }
  else
  {
    digitalWrite(indicator_LED, HIGH);
  }
}

// function for updating LCD monitor as per internet availability

String macAddressCustom()
{
  uint8_t mac1[6];
  char macStr1[18] = {0};
  if (WiFiGenericClass::getMode() == WIFI_MODE_NULL)
  {
    esp_read_mac(mac1, ESP_MAC_WIFI_STA);
  }
  sprintf(macStr1, "%02X%02X%02X%02X%02X%02X", mac1[0], mac1[1], mac1[2], mac1[3], mac1[4], mac1[5]);
  return String(macStr1);
}

void setup()
{

  Serial.begin(115200); // intialize the serial port
  delay(100);           // some delay

  // pinMode declaration,INPUT_PULLUP means it will remain HIGH by default
  pinMode(Wifimanager_trigger, INPUT_PULLUP);
  pinMode(API_Call_Button_1, INPUT_PULLUP);
  pinMode(API_Call_Button_2, INPUT_PULLUP);
  pinMode(HTTP_OTA_UPDATE_PIN, INPUT_PULLUP);
  pinMode(indicator_LED, OUTPUT);

  // Keep Internet indicator LED off on boot up
  digitalWrite(indicator_LED, LOW);

  //MAC address with colon
  // Serial.print("ESP Board MAC Address:  ");
  // Serial.println(WiFi.macAddress());

  MACvalue = macAddressCustom(); // store MAC address without colon

  // I2C LCD initialization
  Wire.begin(SDA, SCL); //Wire.begin(int SDA, int SCL)
  lcd.begin();          // initialize the LCD
  lcd.backlight();      // Enable or Turn On the backlight
  //lcd.println("GB Info Systems!"); // Start Printing
  lcd.println(Client_name);
  //Serial.println("------GB Info Systems------");
  Serial.println(Client_name);
  lcd.setCursor(0, 1);
  lcd.print("------****------");
  delay(3000);
  lcd.clear();
  //lcd.setCursor(0,0);
  lcd.println(Client_name); // Start Printing
  Serial.println(Client_name);
  lcd.setCursor(0, 1);
  lcd.print(Client_location);
  Serial.println(Client_location);
  delay(2000);
  lcd.setCursor(0, 1);
  lcd.print("...Initiating...");
  Serial.println("...Initiating...");
  delay(2000);

  lcd.setCursor(0, 1);
  lcd.print("  MAC address:  ");
  Serial.println("ESP Board MAC Address:  ");
  delay(2000);

  lcd.setCursor(0, 1);
  lcd.print("  ");
  lcd.print(MACvalue);
  lcd.print("  ");
  Serial.println(WiFi.macAddress());
  delay(3000);

  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  Serial.print("Connecting WiFi...");
  lcd.setCursor(0, 1);
  lcd.print("Fetching data...");
  delay(2000);

  ReadConfigJson(); // read saved data on SPIFFS

  lcd.setCursor(0, 1);
  lcd.print("Connecting to...");
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print(ssid);
  lcd.print("....");

  Serial.println();

  WiFi.begin(ssid, wifi_password); // try WiFi connection
  delay(6000);                     // for connecting

  if (WiFi.status() != WL_CONNECTED)
  { // if not connected

    Serial.println("");
    Serial.println("Could not connect to WiFi.... !!!");
    Serial.println(ssid);
    Serial.println(wifi_password);

    lcd.clear();
    lcd.println("Wifi connection");
    lcd.setCursor(0, 1);
    lcd.println("    failed !!   ");

    WifiOK = false;
    InternetOK = false;
  }
  else
  {
    lcd.clear();
    lcd.println(" Wifi connected ");
    lcd.setCursor(0, 1);
    lcd.println(" successfully!! ");
    delay(2000);

    WifiOK = true;
    InternetOK = true;
    Serial.println("");
    Serial.println("Wifi connected using saved JSON password !!");
    Serial.println(ssid);
    Serial.println(wifi_password);

    Serial.println("");
    //Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }

  // Timer helps to reduce processing usage but timely running it
  timer.setInterval(30000, WiFiStatus);       // Check Wifi and internet connectivity every 30 second
  timer.setInterval(500, IndicatorLED);       // update indicator LED state every 500ms
  timer.setInterval(3000, HandleWifimanager); // check wifimanager pin state every 3 seconds
  timer.setInterval(5000, HttpOtaUpdate);     // check HTTP OTA pin state every 5 seconds

  LcdMonitor(); // call it once to update LCD monitor, it gets called with Wifistatus function every 30 seconds
}

void loop()
{ // keeping loop clean as much as possible

  timer.run(); // for SimpleTimer functionality

  if (digitalRead(API_Call_Button_1) == LOW && digitalRead(API_Call_Button_2) == LOW)
  {
    delay(1000); // keep on pressing for one second
    if (digitalRead(API_Call_Button_1) == LOW && digitalRead(API_Call_Button_2) == LOW)
    {
      POSTapiCall(); // main API call function
      delay(5000);   // for repetitive button press
      LcdMonitor();
    }
    Serial.println("Wait for 5 seconds for next button press..!!"); // not to flood POST request in the end server
  }
}

/*
void FixTimeButton()
{
if (digitalRead(API_Call_Button_1) == LOW && digitalRead(API_Call_Button_2) == LOW) {

  int Start_time = millis();
  int End_time = millis();
  int Wait_time = 2000;
  int time_lapse=0;
  while()
  {
    if (digitalRead(API_Call_Button_1) == LOW && digitalRead(API_Call_Button_2) == LOW) {
        int End_time = millis();
        time_lapse = End_time-Start_time;
        delay(500);
       if(time_lapse > Wait_time)  
       {
          POSTapiCall();
          break; 
       }
  }
  else
  {
    break;
  }
}
}
}
*/
