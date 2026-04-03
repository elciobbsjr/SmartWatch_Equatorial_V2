#include <WiFi.h>
#include "app_config.h"
#include "wifi_mgr.h"

static unsigned long g_lastAttempt = 0;

bool wifi_connect()
{
    // já conectado
    if (WiFi.status() == WL_CONNECTED)
        return true;

    Serial.println("[WIFI] Conectando...");

    WiFi.mode(WIFI_STA);

    // importante para estabilidade MQTT + TLS
    WiFi.setSleep(false);

    // inicia conexão
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int tentativas = 0;

    while (WiFi.status() != WL_CONNECTED && tentativas < 30)
    {
        delay(500);
        Serial.print(".");
        tentativas++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\n[WIFI] Conectado!");
        Serial.print("[WIFI] IP: ");
        Serial.println(WiFi.localIP());

        // ======================================
        // CORREÇÃO DO DNS (resolve erro HiveMQ)
        // ======================================

        IPAddress dns1(8,8,8,8);
        IPAddress dns2(1,1,1,1);

        WiFi.config(
            WiFi.localIP(),
            WiFi.gatewayIP(),
            WiFi.subnetMask(),
            dns1,
            dns2
        );

        Serial.println("[WIFI] DNS configurado manualmente");

        // aguarda rede estabilizar
        delay(2000);

        // teste DNS
        IPAddress ipTeste;

        if (WiFi.hostByName(MQTT_HOST, ipTeste))
        {
            Serial.print("[WIFI] DNS OK -> ");
            Serial.println(ipTeste);
        }
        else
        {
            Serial.println("[WIFI] DNS ainda falhou");
        }

        return true;
    }

    Serial.println("\n[WIFI] Falha ao conectar");
    return false;
}

bool wifi_is_connected()
{
    return WiFi.status() == WL_CONNECTED;
}

String wifi_ip()
{
    if (!wifi_is_connected())
        return String("0.0.0.0");

    return WiFi.localIP().toString();
}