#include "Agrumino.h"
#include "AgruminoOTA.h"

// OTA libraries
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h> 
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#define DELAY_TIME 100

/*
AgruminoOTA library, please check readme.md file for more documentation(markdown formatted file).
Updates require enough free space (can be checked with ESP.getFreeSketchSpace()) and a board reset after.
Certain protection functionality is already built in and do not require any additional coding by developer. ArduinoOTA and espota.py use Digest-MD5 to authenticate upload. Integrity of transferred data is verified on ESP side using MD5 checksum.
onOTAModeStart() is run at the start of every mode to handle the safety of the connected equipment.
ArduinoOTA documentation used in development: https://github.com/esp8266/Arduino/blob/master/doc/ota_updates/readme.rst

OTA methods are static so they can be called in two equivalent ways:
	AgruminoOTA agruminoOTA;
	agruminoOTA.isConnected(); //Using class instance
	AgruminoOTA::isConnected(); //Using class name
*/

// Constructors
AgruminoOTA::AgruminoOTA() {
}

//////////////////////////
// Support methods 	   //
/////////////////////////

/**
 *  \brief Agrumino wifi connect
 *  
 *  \param [in] ssid 
 *  \param [in] password 
 *  \return Returns true if connected, false if not connected
 */
boolean AgruminoOTA::wifiConnect(const char* ssid, const char* password)
{
	WiFi.mode(WIFI_AP_STA);
	WiFi.begin(ssid, password);
	return(AgruminoOTA::isConnected());
}

/**
 *  \brief Agrumino wifi connect with a specified number of attempts
 *  
 *  \param [in] ssid 
 *  \param [in] password 
 *  \param [in] max_tries Maximum number of connection tries
 *  \return Returns true if connected, false if not connected
 */
boolean AgruminoOTA::wifiConnect(const char* ssid,const char* password, int max_tries)
{
	int failedAttempts=0;
	while (!AgruminoOTA::isConnected() && failedAttempts < max_tries) 
	{
		AgruminoOTA::wifiConnect(ssid,password);
		failedAttempts++;
		Serial.println("Connection attempt #" + String(failedAttempts) + "/" + String(max_tries));
	}
	if(!AgruminoOTA::isConnected()) // If the board is still not connected after the maximum amount of attempts
	{
		Serial.println("Max number of attempts reached");
	}
	return(AgruminoOTA::isConnected());
}

 /**
 *  \brief Checks if the board is connected to the network
 *  
 *  \return Returns result of check
 */
 boolean AgruminoOTA::isConnected()
{
	return(WiFi.waitForConnectResult() == WL_CONNECTED);
}

/**
 *  \brief Method called at the start of OTA mode
 *  
 *  \param [in] agrumino Agrumino instance
 *  
 *  \details Handles safety of connected equipment
 */
void AgruminoOTA::OTAModeStart(Agrumino agrumino)
{
	// Secure safety of equipment
	agrumino.turnWateringOff();
	
	// Signals start of OTA mode by blinking thrice
	agrumino.turnLedOff();
	for(int i=0; i<=3;i++)
	{
		agrumino.turnLedOn();
		delay(200);
		agrumino.turnLedOff();
	}
	
}

//////////////////////////
// OTA types methods    //
/////////////////////////

/**
 *  \brief Direct download of binary file from HTTP server and upload into agrumino board
 *  
 *  \param [in] agrumino Agrumino instance
 *  \param [in] ota_server Server url or ip
 *  \param [in] ota_port Server port
 *  \param [in] ota_path Path to compiled sketch
 *  \param [in] ota_version_string Optional sketch version string, included in request to server as HTTP_X_ESP8266_VERSION header
 *  
 *  \details Requires the board to be connected the a wi-fi network beforehand
 */
void AgruminoOTA::httpUpdate(Agrumino agrumino, const char* ota_server,int ota_port,const char* ota_path,const char* ota_version_string)
{
	OTAModeStart(agrumino); // Handles safety of connected equipment
	Serial.println("Ready to download (a board reset is required after a successful update)."); 
	t_httpUpdate_return ret = ESPhttpUpdate.update(ota_server,ota_port,ota_path,ota_version_string); // Requests update and gets result
	
	// Server side script can respond as follows: - response code 200, and send the firmware image, - or response code 304 to notify ESP that no update is required
	
	switch(ret) 
	{
		case HTTP_UPDATE_FAILED:
            Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
            break;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("HTTP_UPDATE_NO_UPDATES");
            break;

        case HTTP_UPDATE_OK: // Requires board reset after successful update
            Serial.println("HTTP_UPDATE_OK");
            break;
			
        default:
            Serial.print("Undefined HTTP_UPDATE Code: ");Serial.println(ret);

    }
}

/**
 *  \brief Direct download of binary file from HTTP server and upload into agrumino board
 *  
 *  \param [in] agrumino Agrumino instance
 *  \param [in] ota_server Server url or ip
 *  \param [in] ota_port Server port
 *  \param [in] ota_path Path to compiled sketch
 *  
 *  \details Requires the board to be connected the a wi-fi network beforehand
 */
void AgruminoOTA::httpUpdate(Agrumino agrumino, const char* ota_server,int ota_port,const char* ota_path) // Without optional version string
{
	AgruminoOTA::httpUpdate(agrumino, ota_server, ota_port, "");
}

/**
 *  \brief Handles upload of sketch using Arduino IDE or console
 *  
 *  \param [in] agrumino Agrumino instance
 *  
 *  \details Requires the board to be connected the a wi-fi network beforehand
 */
void AgruminoOTA::ideUpdate(Agrumino agrumino)
{
	OTAModeStart(agrumino); // Handles safety of connected equipment
	
	// Port defaults to 8266
	// ArduinoOTA.setPort(8266);

	// Set Hostname to Agrumino-[ChipID]
	char tmp[16];
	sprintf(tmp,"Agrumino-%06x", ESP.getChipId());
	ArduinoOTA.setHostname(tmp);

	// In Arduino IDE (you may need to restart the IDE first) the board will appear  as Agrumino-[ChipID] in Tools -> Port -> Network ports
	
	ArduinoOTA.onStart([]() {
		String type;
		if (ArduinoOTA.getCommand() == U_FLASH)
		type = "sketch";
		else // U_SPIFFS
		type = "filesystem";

		// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
		Serial.println("Start updating " + type);
	});
	
	ArduinoOTA.onEnd([]() {
		Serial.println("\nEnd (Board reset required)");
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
	Serial.println("Ready, release the user button to proceed, press it again to stop OTA functionality (a board reset is required  after a successful update)");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());
	while(agrumino.isButtonPressed()) // Waits release of user button to start
	{
		delay(DELAY_TIME);
	}
	while(!agrumino.isButtonPressed()) // If the user button is pressed again stops OTA mode
	{
		ArduinoOTA.handle();
		delay(DELAY_TIME);
	}
}

/**
 *  \brief Handles upload of sketch using Arduino IDE or console using a password
 *  
 *  \param [in] agrumino Agrumino instance
 *  \param [in] password 
 *  
 *  \details Requires the board to be connected the a wi-fi network beforehand.  
 */
void AgruminoOTA::ideUpdate(Agrumino agrumino, const char* password) // With password
{
	ArduinoOTA.setPassword(password); //It is possible to reveal password entered previously in Arduino IDE, if IDE has not been closed since last upload.
	
	// Password can be set with it's md5 value as well
	// ArduinoOTA.setPasswordHash(MD5(password));
	
	AgruminoOTA::ideUpdate(agrumino);
	
}

/**
 *  \brief The board will host a web server where the upload of a binary file is possible
 *  
 *  \param [in] agrumino Agrumino instance
 *  \param [in] host Decides the url
 *  \param [in] ota_port
 *  
 *  \details Requires the board to be connected the a wi-fi network beforehand. 
 */
void AgruminoOTA::webServer(Agrumino agrumino, const char* host, int ota_port) 
{
	OTAModeStart(agrumino); // Handles safety of connected equipment
	
	ESP8266WebServer httpServer(ota_port);
	ESP8266HTTPUpdateServer httpUpdater;
	
	MDNS.begin(host);
	
	httpUpdater.setup(&httpServer);
	httpServer.begin();
	MDNS.addService("http", "tcp", ota_port);
	Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
	Serial.println("Alternatively you can use http://" + WiFi.localIP().toString() + ":" + String(ota_port) +  "/update");
	// If the url does not work, try replacing it with module’s IP address. 
	Serial.println("Ready, release the user button to proceed, press it again to stop OTA functionality (a board reset is required after a successful update)");
	while(agrumino.isButtonPressed()) // Waits release of user button to start
	{
		delay(DELAY_TIME);
	}
	while(!agrumino.isButtonPressed()) // If the user button is pressed again stops OTA mode
	{
		httpServer.handleClient();
		delay(DELAY_TIME);
	}
}



