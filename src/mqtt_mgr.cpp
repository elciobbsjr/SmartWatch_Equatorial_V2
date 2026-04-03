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

    // ESSENCIAL para ESP32-C3
    secureClient.setInsecure();

    // tempo maior para handshake TLS
    secureClient.setHandshakeTimeout(30);

    // melhora estabilidade SSL no ESP32
    secureClient.setTimeout(30000);

    // tamanho do pacote MQTT
    client.setBufferSize(1024);

    // aumenta keepalive (HiveMQ derruba conexões lentas)
    client.setKeepAlive(60);

    // define servidor
    client.setServer(MQTT_HOST, 8883);

    configured = true;
}

bool mqtt_connect() {

    mqtt_setup();

    if (client.connected())
        return true;

    // verifica WiFi antes
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[MQTT] WiFi nao conectado");
        return false;
    }

    String clientId = "SMARTWATCH_C3_" + String((uint32_t)ESP.getEfuseMac(), HEX);

    Serial.printf("[MQTT] Conectando via SSL a: %s\n", MQTT_HOST);

    bool ok = client.connect(
        clientId.c_str(),
        MQTT_USER,
        MQTT_PASS
    );

    if (ok) {
        Serial.println("[MQTT] Conectado com sucesso ao HiveMQ!");
        return true;
    }

    Serial.printf("[MQTT] Falha. Codigo PubSub: %d\n", client.state());

    switch(client.state()) {

        case -4:
            Serial.println("Timeout de conexao");
            break;

        case -3:
            Serial.println("Conexao perdida");
            break;

        case -2:
            Serial.println("Falha SSL ou porta incorreta");
            break;

        case -1:
            Serial.println("Cliente desconectado");
            break;

        case 1:
            Serial.println("Versao MQTT incorreta");
            break;

        case 2:
            Serial.println("ClientID rejeitado");
            break;

        case 3:
            Serial.println("Broker indisponivel");
            break;

        case 4:
            Serial.println("Usuario ou senha incorretos");
            break;

        case 5:
            Serial.println("Nao autorizado");
            break;
    }

    return false;
}

void mqtt_loop() {

    if (!client.connected())
        return;

    client.loop();
}

bool mqtt_publish(const String &topic, const String &payload) {

    if (!client.connected())
        return false;

    return client.publish(topic.c_str(), payload.c_str());
}

bool mqtt_is_connected() {
    return client.connected();
}