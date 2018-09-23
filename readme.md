# AgruminoOTA
## Introduction
OTA (Over the Air) update is the process of loading the firmware to ESP module using Wi-Fi connection.
This library has the goal of bringing OTA functionalities to the Agrumino board.

OTA may be done using several modes:
-   [HTTP Server](#http-server)
-   [ArduinoIDE](#arduinoide)
-   [Web Browser](#web-browser)

All of the aforementioned methods need an existing network connection, additional functions to establish a Wi-Fi connection and check the connection status have been included in this library. 

The new sketch will be stored in the space between the old sketch and the spiff, the new sketch is then copied over the old one after a board reset. This means that after an upload is sucessfully completed the board will always require a reboot to complete the process and begin running the new sketch.

In order to keep OTA functionalities the new sketches will require some way to activate OTA routines again.

OTA methods are static so they can be called in two equivalent ways:
```c++ 
AgruminoOTA agruminoOTA;
agruminoOTA.isConnected(); //Using class instance
AgruminoOTA::isConnected(); //Using class name
```
## HTTP Server
This mode consist of a direct download of a binary image from a specified IP or domain address on the network or Internet.
```c++
static void httpUpdate(Agrumino agrumino, const char* ota_server,int ota_port,const char* ota_path);
static void httpUpdate(Agrumino agrumino, const char* ota_server, int ota_port, const char* ota_path, const char* ota_version_string);
```
The hosting server may handle the download request in an advanced way using the headers sent in the HTTP request.

Example of header data:
```
[HTTP_USER_AGENT] => ESP8266-http-Update
[HTTP_X_ESP8266_STA_MAC] => 18:FE:AA:AA:AA:AA
[HTTP_X_ESP8266_AP_MAC] => 1A:FE:AA:AA:AA:AA
[HTTP_X_ESP8266_FREE_SPACE] => 671744
[HTTP_X_ESP8266_SKETCH_SIZE] => 373940
[HTTP_X_ESP8266_SKETCH_MD5] => a56f8ef78a0bebd812f62067daf1408a
[HTTP_X_ESP8266_CHIP_SIZE] => 4194304
[HTTP_X_ESP8266_SDK_VERSION] => 1.3.0
[HTTP_X_ESP8266_VERSION] => DOOR-7-g14f53a19
```
The parameter ota_version_string is optional and corresponds to the HTTP_X_ESP8266_VERSION header.
The server can use header information to check if an update is needed. It is also possible to deliver different binaries based on the MAC address for example.

## ArduinoIDE
This mode requires python 2.7 and handles updates done using Arduino IDE, or even directly with a console python command.
In Arduino IDE (you may need to restart the IDE first) the board will appear as Agrumino-[ChipID] in Tools -> Port -> Network ports.
```c++
static void otaUpdate(Agrumino agrumino);
static void otaUpdate(Agrumino agrumino, const char* password);
```
A password can be specified, but it should be noted that it's possible to reveal a password entered previously in Arduino IDE, if the IDE has not been closed since last upload.

The ArduinoIDE mode requires the device to enter a loop in order to handle the reception of updates, to exit from this loop press the user button.

## Web Browser
This mode uses a web site hosted by the board to handle the upload of a binary image containing the new sketch.
When the mode is activated an url (decided by the host parameter) is provided by the board using the serial monitor, if the url does not work, you can replace it with the module’s IP address. 

The hosted web site can be customized by the developers.

```c++
static void webServer(Agrumino agrumino,const char* host, int ota_port);
```
The Web Browser mode requires the device to enter a loop in order to handle the web browser, to exit from this loop press the user button.
## Safety and Security
Security functions can be provided by server side checks in case of HTTP Server mode or the use of a password in Arduino IDE mode.
```c++
static void OTAModeStart(Agrumino agrumino)
```
The function OTAModeStart() is called when any OTA mode is activated, inside this function some of the equipment can be put into a safe state before the update is performed.

```c++
ESP.getFreeSketchSpace();
```
Can be used to check the amount of free space for the new sketch.

The led will blink thrice in a short amount of time to signal the beginning of OTA mode.

## Example
An example skeleton sketch has been written handling various OTA cases, changing the defined value OTA_TYPE will result in the use of a different mode:
- 1 - HTTP Server
- 2 - Web Browser
- 3 - IDE update
- 4 - IDE update with password

The selected mode is activated by pressing the user button for a certain amount of time, an amount specified by the defined value pressInterval (set to 3 seconds by default).

When the button has been pressed for the specified amount of time the board will try to connect to the specified network a certain number of times (defined by WI_FI_FAILED_TRIES_MAX).

In case of a mode that requires entering a loop to handle the update (Web Browser and IDE update) if the user button is pressed once more the program will exit the loop.

Values required by the sketch to connect to a network and handle the selected OTA mode need to be filled out before compiling.

## Documentation for further expansion
[ArduinoOTA documentation (used to develop this library)](https://github.com/esp8266/Arduino/blob/master/doc/ota_updates/readme.rst)