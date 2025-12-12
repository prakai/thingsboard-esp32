#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#include <Arduino.h>

//
// Copy or rename this file to Configuration.h and fill in the following fields
//

//
// WiFi configuration
//
String WiFi_ssid = "LOCAL_WIFI_SSID";
String WiFi_pass = "LOCAL_WIFI_PASSWORD";

//
// ThingsBoard server and device access token
//
String ThingsBoard_server = "iot-so.rmuti.ac.th";
uint16_t ThingsBoard_port = 1883;
String ThingsBoard_token = "DEVICE_ACCESS_TOKEN";  // Device Access Token

// See https://thingsboard.io/docs/user-guide/device-provisioning/
// to understand how to create a device profile to be able to provision a device
constexpr char ThingsBoard_Provision_Device_Key[] = "YOUR_PROVISION_DEVICE_KEY";
constexpr char ThingsBoard_Provision_Device_Secret[] = "YOUR_PROVISION_DEVICE_SECRET";

#endif  // _CONFIGURATION_H