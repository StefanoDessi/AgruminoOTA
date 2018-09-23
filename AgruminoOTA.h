#ifndef AgruminoOTA_h
#define AgruminoOTA_h

#include "Arduino.h"
#include "Agrumino.h"

class AgruminoOTA {
	
  public:
	// Constructors
    AgruminoOTA();
	
	// OTA types, requires wifi connection
	static void httpUpdate(Agrumino agrumino, const char* ota_server,int ota_port,const char* ota_path,const char* ota_version_string);
	static void httpUpdate(Agrumino agrumino, const char* ota_server,int ota_port,const char* ota_path);
	static void ideUpdate(Agrumino agrumino);
	static void ideUpdate(Agrumino agrumino, const char* password);
	static void webServer(Agrumino agrumino,const char* host, int ota_port);
	
	// Support methods
	static boolean wifiConnect(const char* ssid,const char* password);
	static boolean wifiConnect(const char* ssid,const char* password, int max_tries);
	static boolean isConnected();
	static void OTAModeStart(Agrumino agrumino); // Handles safety in OTA mode
	
};

#endif

