#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include "app_config.h"
#include "mqtt_mgr.h"

static WiFiClientSecure secureClient;
static PubSubClient client(secureClient);

static bool configured = false;
static uint32_t lastReconnectAttempt = 0;

// tempo mínimo entre tentativas de reconexão
#define MQTT_RECONNECT_INTERVAL 5000

//--------------------------------------------------
// CONFIGURAÇÃO MQTT + TLS
//--------------------------------------------------
static void mqtt_setup()
{
    if (configured) return;

    Serial.println("[MQTT] Configurando cliente SSL...");

    // ESSENCIAL no ESP32-C3
    secureClient.setInsecure();

    // tempo maior para handshake TLS
    secureClient.setHandshakeTimeout(30);

    // timeout de socket maior
    secureClient.setTimeout(30000);

    // buffer maior para JSON
    client.setBufferSize(1024);

    // tempo de keepalive
    client.setKeepAlive(60);

    // define servidor
    client.setServer(MQTT_HOST, 8883);

    configured = true;

    Serial.println("[MQTT] Setup concluído");
}

//--------------------------------------------------
// CONEXÃO MQTT
//--------------------------------------------------
bool mqtt_connect()
{
    mqtt_setup();

    if (client.connected())
        return true;

    // evita spam de reconexão
    if (millis() - lastReconnectAttempt < MQTT_RECONNECT_INTERVAL)
        return false;

    lastReconnectAttempt = millis();

    // garante WiFi
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[MQTT] WiFi nao conectado");
        return false;
    }

    String clientId =
        "SMARTWATCH_" +
        String((uint32_t)ESP.getEfuseMac(), HEX);

    Serial.println("[MQTT] Iniciando conexão TLS...");
    Serial.printf("[MQTT] Broker: %s\n", MQTT_HOST);
    Serial.printf("[MQTT] ClientID: %s\n", clientId.c_str());

    bool ok = client.connect(
        clientId.c_str(),
        MQTT_USER,
        MQTT_PASS
    );

    if (ok)
    {
        Serial.println("[MQTT] Conectado com sucesso!");
        return true;
    }

    Serial.printf("[MQTT] Falha. Codigo PubSub: %d\n",
                  client.state());

    switch (client.state())
    {
        case -4:
            Serial.println("Erro: timeout conexão");
            break;

        case -3:
            Serial.println("Erro: conexão perdida");
            break;

        case -2:
            Serial.println("Erro SSL ou porta incorreta (8883)");
            break;

        case -1:
            Serial.println("Erro: cliente desconectado");
            break;

        case 1:
            Serial.println("Erro: versão MQTT incorreta");
            break;

        case 2:
            Serial.println("Erro: clientID rejeitado");
            break;

        case 3:
            Serial.println("Erro: broker indisponível");
            break;

        case 4:
            Serial.println("Erro: usuário ou senha inválidos");
            break;

        case 5:
            Serial.println("Erro: não autorizado");
            break;
    }

    return false;
}

//--------------------------------------------------
// LOOP MQTT
//--------------------------------------------------
void mqtt_loop()
{
    if (!client.connected())
        return;

    client.loop();
}

//--------------------------------------------------
// PUBLICAÇÃO
//--------------------------------------------------
bool mqtt_publish(
    const String &topic,
    const String &payload
)
{
    if (!client.connected())
        return false;

    bool ok = client.publish(
        topic.c_str(),
        payload.c_str()
    );

    if (!ok)
        Serial.println("[MQTT] Falha ao publicar");

    return ok;
}

//--------------------------------------------------
bool mqtt_is_connected()
{
    return client.connected();
}