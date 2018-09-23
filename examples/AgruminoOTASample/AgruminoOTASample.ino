#include <Agrumino.h>
#include <AgruminoOTA.h>

#define SLEEP_TIME_SEC 2
#define PIN_BTN_S1       4
#define WI_FI_FAILED_TRIES_MAX 4
#define OTA_TYPE 1 //1 HTTP update, 2 web browser, 3 IDE update, 4 IDE update with password

Agrumino agrumino;

// Agrumino timers
unsigned long previousMillis = 0UL;
unsigned long currentMillis = 0UL;
const long interval = 10000;

// Wi-Fi configuration
const char* ssid ="........";
const char* network_password = "........";

// Web update host name
const char* host = "agrumino-webupdate";

// OTA IDE optional password
const char* ide_password = "........";

// OTA HTTP Server configuration
const char* ota_server = "........";
int ota_port = 80;
const char* ota_path = "....... .bin";
const char* ota_version_string = "1"; // optional version of file [HTTP_X_ESP8266_VERSION] http header data

// OTA mode activaction
const long pressInterval = 3000;
boolean pressionFlag = false;
long timerStart = 0;
boolean lastIsPressed = false;

void setup(void) 
{
  Serial.begin(115200);
  agrumino.setup();
}

void loop(void) 
{
  Serial.println("#########################\n");

  agrumino.turnBoardOn();
  
  boolean isAttachedToUSB =   agrumino.isAttachedToUSB();
  boolean isBatteryCharging = agrumino.isBatteryCharging();
  boolean isButtonPressed =   agrumino.isButtonPressed();
  float temperature =         agrumino.readTempC();
  unsigned int soilMoisture = agrumino.readSoil();
  float illuminance =         agrumino.readLux();
  float batteryVoltage =      agrumino.readBatteryVoltage();
  unsigned int batteryLevel = agrumino.readBatteryLevel();

  // Check if user button has been pressed for [pressInterval] ms to activate OTA mode
  if(!pressionFlag && isButtonPressed && !lastIsPressed)
  {
    timerStart = millis();
    pressionFlag = true;
  }
  if(pressionFlag)
  {
    if(isButtonPressed)
    {
      if(millis() - timerStart >= pressInterval )
      {
        updateSketch(); // Launch OTA mode
        pressionFlag = false;
      }
    }
    else
    {
      pressionFlag = false;
    }
  }
  lastIsPressed = isButtonPressed;
  
  Serial.println("");
  Serial.println("isAttachedToUSB:   " + String(isAttachedToUSB));
  Serial.println("isBatteryCharging: " + String(isBatteryCharging));
  Serial.println("isButtonPressed:   " + String(isButtonPressed));
  Serial.println("temperature:       " + String(temperature) + "°C");
  Serial.println("soilMoisture:      " + String(soilMoisture) + "%");
  Serial.println("illuminance :      " + String(illuminance) + " lux");
  Serial.println("batteryVoltage :   " + String(batteryVoltage) + " V");
  Serial.println("batteryLevel :     " + String(batteryLevel) + "%");
  Serial.println("");

  if (isButtonPressed) 
  {
    agrumino.turnWateringOn();
    delay(2000);
    agrumino.turnWateringOff();
  }

  blinkLed();

  agrumino.turnBoardOff(); // Board off before delay/sleep to save battery :)

  // Keeps the time
  if(!pressionFlag)
  {
    deepSleepSec(SLEEP_TIME_SEC); // ESP8266 enter in deepSleep and after the selected time starts back from setup() and then loop()
  }
  else
  {
    delaySec(SLEEP_TIME_SEC); // The ESP8266 stays powered, executes the loop repeatedly
  }
}

/////////////////////
// Utility methods //
/////////////////////

void blinkLed() {
  agrumino.turnLedOn();
  delay(200);
  agrumino.turnLedOff();
}

void delaySec(int sec) {
  delay (sec * 1000);
}

void deepSleepSec(int sec) {
  ESP.deepSleep(sec * 1000000); // microseconds
}


/////////////////////
// OTA methods     //
/////////////////////

void updateSketch()
{
  /*
  OTA methods are static so they can be called in two equivalent ways:
    AgruminoOTA agruminoOTA;
    agruminoOTA.isConnected(); //Using class instance
    AgruminoOTA::isConnected(); //Using class name
  */
  AgruminoOTA::wifiConnect(ssid,network_password,WI_FI_FAILED_TRIES_MAX); // Tries to connect to WiFi network
  if(!AgruminoOTA::isConnected())
  {
    Serial.println("UPDATE ABORTED [No connection]");
  }
  else
  {
    Serial.println("Connection success!");
    switch(OTA_TYPE) // Every OTA mode will require a reboot after a successful update, and enough free space to save the new sketch
    {
      case 1:
        Serial.println("OTA type: HTTP Update"); // This mode consist of a direct download of a binary image from a specified IP or domain address on the network or Internet.
        AgruminoOTA::httpUpdate(agrumino,ota_server,ota_port,ota_path,ota_version_string);
        break;
      case 2:
        Serial.println("OTA type: Web Server"); // This mode uses a web site hosted by the board to handle the upload of a binary image containing the new sketch.
        AgruminoOTA::webServer(agrumino,host,ota_port);
		    // When the mode is activated an url (decided by the parameter host) is provided by the board using the serial monitor, if the url does not work, you can replace it with the module’s IP address. 
        break;
      case 3:
        Serial.println("OTA type: IDE update"); // This mode requires python 2.7 and handles updates done using Arduino IDE, or even directly with a console python command.
        AgruminoOTA::ideUpdate(agrumino);
		    // In Arduino IDE (you may need to restart the IDE first) the board will appear  as Agrumino-[ChipID] in Tools -> Port -> Network ports
        break;
      case 4:
        Serial.println("OTA type: IDE update with password"); // ArduinoIDE update using password
        AgruminoOTA::ideUpdate(agrumino,ide_password);
        break;
      default:
        Serial.println("Unidentified OTA type");
    }
  }
}