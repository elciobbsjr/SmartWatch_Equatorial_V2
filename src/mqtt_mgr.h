#pragma once
#include <Arduino.h>

bool mqtt_connect();
void mqtt_loop();
bool mqtt_publish(const String &topic, const String &payload);
bool mqtt_is_connected();
