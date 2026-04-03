#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include "app_config.h"
#include "mqtt_mgr.h"

static WiFiClientSecure secureClient;
static PubSubClient client(secureClient);

static bool configured = false;

static void mqtt_setup() {
    if (configured) return;

    secureClient.setInsecure(); // para testes (TLS sem certificado)
    client.setServer(MQTT_HOST, MQTT_PORT);

    configured = true;
}

bool mqtt_connect() {

    mqtt_setup();

    if (client.connected()) return true;

    String clientId = "SMARTWATCH_" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
        return true;
    }

    return false;
}

void mqtt_loop() {
    if (client.connected()) {
        client.loop();
    }
}

bool mqtt_publish(const String &topic, const String &payload) {
    if (!client.connected()) return false;
    return client.publish(topic.c_str(), payload.c_str());
}

bool mqtt_is_connected() {
    return client.connected();
}
