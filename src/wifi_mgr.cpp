#include <WiFi.h>
#include "app_config.h"
#include "wifi_mgr.h"

static unsigned long g_lastAttempt = 0;

bool wifi_connect() {
    if (WiFi.status() == WL_CONNECTED) return true;

    // evita ficar tentando toda hora (anti-spam)
    unsigned long now = millis();
    if (now - g_lastAttempt < 1000) return false;
    g_lastAttempt = now;

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // mais estável para IoT em tempo real
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < WIFI_CONNECT_TIMEOUT_MS) {
        delay(200);
    }

    return (WiFi.status() == WL_CONNECTED);
}

bool wifi_is_connected() {
    return WiFi.status() == WL_CONNECTED;
}

String wifi_ip() {
    if (!wifi_is_connected()) return String("0.0.0.0");
    return WiFi.localIP().toString();
}
