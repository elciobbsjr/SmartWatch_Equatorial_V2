#include "http_mgr.h"

#include <WiFi.h>
#include <HTTPClient.h>

static const char* GOOGLE_SCRIPT_URL =
"https://script.google.com/macros/s/AKfycbyIYVnoe_eXx_P4l5f0VEkUgJwdwcmSzNjYLo4qbiKUpEConGJ6kLLdoB4q6N6_OEADXQ/exec";

bool http_send_telemetry(const char* json)
{
    if (WiFi.status() != WL_CONNECTED)
        return false;

    HTTPClient http;

    http.begin(GOOGLE_SCRIPT_URL);

    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST((uint8_t*)json, strlen(json));

    http.end();

    return httpCode > 0;
}