#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>

const char* fn_password = "/password.txt";
const char* fn_ssid = "/ssid.txt";
const char* fn_wifiMode = "/boot_mode.txt";
const char* fn_deviceName = "/device_name.txt";

const int buttonPin = 13;     // the number of the pushbutton pin
const int ledPin =  4;      // the number of the LED pin
const int relayPin =  5;      // the number of the Relay pin

IPAddress local_IP(192,168,4,22);
IPAddress gateway(192,168,4,9);
IPAddress subnet(255,255,255,0);

String realSize = String(ESP.getFlashChipRealSize());
String ideSize = String(ESP.getFlashChipSize());
bool flashCorrectlyConfigured = realSize.equals(ideSize);
int relayState = LOW;
int buttonState = HIGH;

// Create an instance of the server
// specify the port to listen on as an argument
ESP8266WebServer server(80);

void gpio_setup() {
    // prepare GPIO2
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);
  
  pinMode(buttonPin, INPUT_PULLUP);
//  digitalWrite(ledPin,HIGH);
  
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
}


void spiffs_setup() {
  if (flashCorrectlyConfigured)
  {
    Serial.println("Flash correctly configured, DE Size: " + ideSize + ", real size: " + realSize);
  }
  else
  {
    Serial.println("Flash in-correctly configured, DE Size: " + ideSize + ", real size: " + realSize);
  }
  
  if(SPIFFS.begin())
  {
    Serial.println("SPIFFS initialised.... ok");
  }
  else
  {
    Serial.println("SPIFFS initialised.... failed");
  }

}

void wifi_setup() {
  
  File f_ssid = SPIFFS.open(fn_ssid, "r");
  File f_pwd = SPIFFS.open(fn_password, "r");
  File f_wifiMode = SPIFFS.open(fn_wifiMode, "r");
//  const char* password = "";
//  const char* ssid = "";

// Check status of boot_mode.txt and whether credentials exist and set wifi mode accodingly
  bool creds_exist = true;
  String wifiMode = f_wifiMode.readStringUntil('\n');
  Serial.print("WIFI Mode: ");
  Serial.println(wifiMode); 
  String str_ssid = f_ssid.readStringUntil('\n');
  String str_pwd = f_pwd.readStringUntil('\n');
//  Serial.print("SSID: ");
//  Serial.println(str_ssid);
//  Serial.print("Password: ");
//  Serial.println(str_pwd);
  if (wifiMode == "STA")
  {
    if (!f_ssid or !f_pwd)
    {
      creds_exist = false;
    }
    else if (str_ssid.length() == 0 or str_pwd.length() == 0)
    {
      creds_exist = false;
    }
    
    if (!creds_exist)
    {
      Serial.println("SSID and Password not set - restarting in AP mode...");
      Serial.println("==== Write to boot_mode.txt file in SPIFF =====");
      f_wifiMode = SPIFFS.open(fn_wifiMode, "w");
      f_wifiMode.print("AP");
      f_wifiMode.close();
      
      Serial.println("==== Restarting ESP8266====");
      ESP.restart();
    }
    else
    {
      Serial.print("Reading password...");
//      String str_password = f_pwd.readStringUntil('\n');
      const char* password = str_pwd.c_str();
//      Serial.println(password);
      Serial.print("Reading ssid...");
//      String str_ssid = f_ssid.readStringUntil('\n');
      const char* ssid = str_ssid.c_str();
//      Serial.println(String(ssid));
      
      WiFi.mode(WIFI_AP);
      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid, password);
      Serial.print("Connecting");
      int timeout = 60000;
      int counter = 0;
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        counter++;
        if (counter * 500 >= timeout)
        {
          Serial.println("SSID and Password not set - restarting in AP mode...");
          Serial.println("==== Write to boot_mode.txt file in SPIFF =====");
          f_wifiMode = SPIFFS.open(fn_wifiMode, "w");
          f_wifiMode.print("AP");
          f_wifiMode.close();
          
          Serial.println("==== Restarting ESP8266====");
          ESP.restart();
          break;
        }
      }
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("Connect using IP: ");
    Serial.println(WiFi.localIP());
  }
//    if (wifiMode == "AP")
  else
  {
    WiFi.mode(WIFI_AP);
//    delay(100);
    Serial.print("Setting soft-AP configuration... ");
    Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");
//    delay(10);
    Serial.print("Setting soft-AP... ");
    Serial.println(WiFi.softAP("ABCD_Smart_Device","Password123!") ? "Ready" : "Failed!");
//    delay(10);
    Serial.print("Soft-AP IP address = ");
    Serial.println(WiFi.softAPIP());
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
}

//void handleRoot() {
//  server.send(200, "text/html","
//}

void startServer() {
  // Start the server
  // server.on ("/", handleRoot);
  server.serveStatic("/img", SPIFFS, "/img");
  server.serveStatic("/src", SPIFFS, "/src");
  server.serveStatic("/css", SPIFFS, "/css");
  server.serveStatic("/", SPIFFS, "/index_3.html");
  server.serveStatic("/data", SPIFFS, "/");
  server.on("/gpio", updateGPIO);
  server.on("/getDeviceName", getDeviceName);
  server.on("/newConfig", newConfig);
  server.on("/setMode", setMode);
  server.on("/getMode", getMode);
  server.on("/getConfig", getConfig);
  server.begin();
  Serial.println ( "HTTP server started" );
}

void setRelayStatus(int state) {
  
  digitalWrite(relayPin, state);
  String printText = "Setting relay pin to State: ";
  relayState = state;
  if (state == 1) {
    printText += "HIGH";
  } else {
    printText += "LOW";
  }
  Serial.println(printText);

}

void updateGPIO(){
  String gpio = server.arg("id");
  String state = server.arg("state");
  String success = "1";
  int pin = ledPin;
  if ( gpio == "LED" ) {
    pin = ledPin;
  } else if ( gpio == "RELAY" ) {
    pin = relayPin;
    relayState = state.toInt();
  } else {   
    pin = ledPin;
  }
  Serial.println(pin);
  if ( state == "1" ) {
    digitalWrite(pin, HIGH);
  } else if ( state == "0" ) {
    digitalWrite(pin, LOW);
  } else {
    success = "1";
    Serial.println("Err Led Value");
  }
  
  String json = "{\"gpio\":\"" + String(gpio) + "\",";
  json += "\"state\":\"" + String(state) + "\",";
  json += "\"success\":\"" + String(success) + "\"}";
  
  server.send(200, "application/json", json);
  Serial.println("GPIO updated");
}

void getDeviceName(){
  File f_deviceName = SPIFFS.open(fn_deviceName, "r");
  String deviceName = f_deviceName.readStringUntil('\n');
  Serial.print("Device Name: ");
  Serial.println(deviceName);
  String json = "{\"deviceName\":\"" + deviceName + "\"}";
  server.send(200, "application/json", json);
  f_deviceName.close();
}

void getConfig(){
  File f_ssid = SPIFFS.open(fn_ssid, "r");
  File f_pwd = SPIFFS.open(fn_password, "r");
  File f_deviceName = SPIFFS.open(fn_deviceName, "r");
  String str_ssid = f_ssid.readStringUntil('\n');
  String str_pwd = f_pwd.readStringUntil('\n');
  String str_deviceName = f_deviceName.readStringUntil('\n');
  Serial.print("SSID: ");
  Serial.println(str_ssid);
  Serial.print("Password: ");
  Serial.println(str_pwd);
  Serial.print("Device Name: ");
  Serial.println(str_deviceName);
  String json = "{\"ssid\":\"" + str_ssid + "\"";
  json += ",\"password\":\"" + str_pwd + "\"";
  json += ",\"deviceName\":\"" + str_deviceName + "\"}";
  Serial.println(json);
  server.send(200, "application/json", json);
  f_ssid.close();
  f_pwd.close();
  f_deviceName.close();
}

// Issue here where values not being correctly recognised and saved into the relevant .txt
// Try saving values into an array and use println to write data into SPIFFS files??
void newConfig(){
  String str_ssid = server.arg("ssid");
  String str_pwd = server.arg("pwd");
  String str_device_name = server.arg("deviceName");
  const char* new_ssid = str_ssid.c_str();
  const char* new_pwd = str_pwd.c_str();
  const char* new_device_name = str_device_name.c_str();
  String success = "1";
  
  Serial.println("Changing config settings...");
  
  Serial.println("==== Write to ssid.txt file in SPIFF =====");
  Serial.print("Value: ");
  Serial.println(String(new_ssid));
  File f_ssid = SPIFFS.open(fn_ssid, "w");
  f_ssid.print(new_ssid);
  f_ssid.close();
  
  Serial.println("==== Write to password.txt file in SPIFF =====");
  Serial.print("Value: ");
  Serial.println(String(new_pwd));
  File f_pwd = SPIFFS.open(fn_password, "w");
  f_pwd.print(new_pwd);
  f_pwd.close();
    
  Serial.println("==== Write to device_name.txt file in SPIFF =====");
  Serial.print("Value: ");
  Serial.println(String(new_device_name));
  File f_device_name = SPIFFS.open(fn_deviceName, "w");
  f_device_name.print(String(new_device_name));
  f_device_name.close();
    
  String json = "{\"success\":\"" + String(success) + "\"}";
  
  server.send(200, "application/json", json);
  Serial.println("Config settings updated");
}

void getMode(){
  File f_wifiMode = SPIFFS.open(fn_wifiMode, "r");
  String str_wifiMode = f_wifiMode.readStringUntil('\n');
  String json = "{\"mode\":\"" + str_wifiMode + "\"}";
  Serial.println(json);
  server.send(200, "application/json", json);
}

void setMode(){
  String wifiMode = server.arg("mode");
  String success = "1";
  
  Serial.println("Changing to AP mode...");
  Serial.println("==== Write to boot_mode.txt file in SPIFF =====");
  
  File f_wifiMode = SPIFFS.open(fn_wifiMode, "w");
  f_wifiMode.print(wifiMode);
  f_wifiMode.close();
  
  String json = "{\"success\":\"" + String(success) + "\"}";
  
  server.send(200, "application/json", json);
  Serial.println("boot_mode.txt updated");
  
  Serial.println("==== Restarting ESP8266====");
  ESP.restart();
}

void setup() {
  // put your setup code here, to run once:
  delay(1000);
  Serial.begin(115200);
  Serial.println("Booting...");
  spiffs_setup();
  wifi_setup();
  gpio_setup();
  startServer();

}

void loop() {
  int curButtonState = digitalRead(buttonPin);
  if (curButtonState != buttonState) {
    if (curButtonState == LOW) {
      if (relayState == 1) {
        setRelayStatus(0);
      }
      else {
        setRelayStatus(1);
      }
    }
  buttonState = curButtonState;
  }
  server.handleClient();
}



