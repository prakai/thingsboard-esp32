#ifndef _WIFI_MANAGER_H
#define _WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

#include "Configuration.h"

#define WIFI_CONNECT_ATTEMPS_TIMOUT 10000
#define WIFI_ATTEMPS_MAX 5

uint8_t lastWiFiStatus = WL_IDLE_STATUS;
uint8_t lastWiFiAttemps = 0;

void WiFi_setup();
void WiFi_connect();
void WiFi_onEvent(WiFiEvent_t event);

void WiFi_setup()
{
    Serial.println("setup_WiFi()");

    WiFi.mode(WIFI_STA);
    WiFi.onEvent(WiFi_onEvent);
    lastWiFiStatus = WiFi.status();
    lastWiFiAttemps = 0;
}

void WiFi_connect()
{
    // Serial.println("WiFi_connect()");

    static uint8_t _lastWiFiStatus = WL_IDLE_STATUS;
    if (WiFi.status() == WL_CONNECTED) {
        if (WiFi.status() != _lastWiFiStatus) {
            // Serial.println();
            // Serial.println("WiFi connected.");
            // Serial.println("IP address: ");
            // Serial.println(WiFi.localIP());
        }
        _lastWiFiStatus = WiFi.status();
        return;
    }

    static unsigned long _lastConnectAttempt = 0;
    if (_lastConnectAttempt != 0 && millis() - _lastConnectAttempt < WIFI_CONNECT_ATTEMPS_TIMOUT) {
        return;
    }
    _lastConnectAttempt = millis();
    // lastWiFiAttemps++;

    Serial.print(lastWiFiAttemps + 1);
    Serial.print(" - Connecting WiFi to ");
    Serial.println(WiFi_ssid);
    WiFi.disconnect(true);
    WiFi.begin(WiFi_ssid, WiFi_pass);
}

void WiFi_onEvent(WiFiEvent_t event)
{
    switch (event) {
        case WIFI_EVENT_STA_CONNECTED:
            Serial.println("WiFi connected.");
            break;
        case IP_EVENT_STA_GOT_IP:
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            lastWiFiAttemps = 0;
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            Serial.println("WiFi disconnected or failed.");
            lastWiFiAttemps++;
            break;
        default:
            break;
    }
}

#endif  // _WIFI_MANAGER_H