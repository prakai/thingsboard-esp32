#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#include <Arduino.h>

//
// RMUTI IoT WiFi configuration
//
// String WiFi_ssid = "RMUTI-IoT";
// String WiFi_pass = "IoT@RMUTI";

//
// Local WiFi configuration
//
String WiFi_ssid = "IoT";
String WiFi_pass = "MyIoT-WiFi";

//
// ThingsBoard server and device access token
//
String ThingsBoard_server = "iot-so.rmuti.ac.th";
uint16_t ThingsBoard_port = 1883;
String ThingsBoard_token = "ukmeru2idhja01fhv9kf";  // Device Access Token

#endif  // _CONFIGURATION_H