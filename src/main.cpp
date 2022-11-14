#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include "EspMQTTClient.h"

#define SERIE_NUMBER "abcdef123456789"

#define CONFIG_START 32        // position in EEPROM where our first byte gets written
#define CONFIG_VERSION "00003" // version string to let us compare current to whatever is in EEPROM

typedef struct
{
    char version[6];   // Version of the configuration in EEPROM, used in case we change the struct
    uint8_t debug;     // Debug on yes/no 1/0
    uint8_t saved;     // WiFi config saved yes/no 1/0
    char nodename[32]; // this node name
    char ssid[32];     // WiFi SSID
    char password[64]; // WiFi Password
    char mqttip[32];   // Mosquitto server IP
    uint16_t mqttport; // Mosquitto port
    char mqttuser[32]; // Mosquitto Username (if needed, or "")
    char mqttpass[64]; // Moqsuitto Password (if needed, or "")
} configuration_t;

configuration_t CONFIGURATION = {
    CONFIG_VERSION,
    1,
    0,
    SERIE_NUMBER,
    "",
    "",
    "broker.emqx.io",
    1883,
    "",
    ""};

const char MAIN_page[] PROGMEM = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
        <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
        <title>Medicare IoT connection</title>
        <style>
            body {color: #434343; font-family: "Helvetica Neue",Helvetica,Arial,sans-serif; font-size: 14px; background-color: #eeeeee; margin-top: 100px;}
            .container {margin: 0 auto; max-width: 400px; padding: 30px; box-shadow: 0 10px 20px rgba(0,0,0,0.19), 0 6px 6px rgba(0,0,0,0.23); background-color: #ffffff; border-radius: 10px;}
            h2 {text-align: center; margin-bottom: 20px; margin-top: 0px; color: #0ee6b1; font-size: 35px;}
            #titleGreen {color: #00E1AA;}
            #titleBlack {color: #000000;}
            h3 {text-align: center; margin-bottom: 40px; margin-top: 0px; color: #336859; font-size: 35px;}
            form .field-group {box-sizing: border-box; clear: both; padding: 4px 0; position: relative; margin: 1px 0; width: 100%;}
            .text-field {font-size: 15px; margin-bottom: 4%; -webkit-appearance: none; display: block; background: #fafafa; color: #636363; width: 100%; padding: 15px 0px 15px 0px; text-indent: 10px; border-radius: 5px; border: 1px solid #e6e6e6; background-color: transparent;}
            .text-field:focus {border-color: #00bcd4; outline: 0;}
            .button-container {box-sizing: border-box; clear: both; margin: 1px 0 0; padding: 4px 0; position: relative; width: 100%;}
            .button {background: #00E1AA; border: none; border-radius: 5px; color: #ffffff; cursor: pointer; display: block; font-weight: bold; font-size: 16px; padding: 15px 0; text-align: center; text-transform: uppercase; width: 100%; -webkit-transition: background 250ms ease; -moz-transition: background 250ms ease; -o-transition: background 250ms ease; transition: background 250ms ease;}
            p {text-align: center; text-decoration: none; color: #87c1d3; font-size: 18px;}
            a {text-decoration: none; color: #ffffff; margin-top: 0%;}
            #status {text-align: center; text-decoration: none; color: #336859; font-size: 14px;}
        </style>
        <script>
        function validateForm()
        {
            var ssid = document.forms["myForm"]["ssid"].value;
            var password = document.forms["myForm"]["password"].value;
            var status = document.getElementById("statusDiv");
            if (ssid == "" && password == "") {
                status.innerHTML = "<p id='status' style='color:red;'>Insira SSID e senha.</p>";
                return false;
            }
            else if (ssid == "") {
                status.innerHTML = "<p id='status' style='color:red;'>Insira SSID.</p>";
                return false;
            }
            else if (password == "") {
                status.innerHTML = "<p id='status' style='color:red;'>Insira senha.</p>";
                return false;
            }
            else {
                status.innerHTML = "<p id='status'>Conectando...</p>";
                return true;
            }
        }
        </script>
    </head>
    <body>
    <div class="container">
        <h2><span id="titleGreen">Medicare </span><span id="titleBlack">IoT</span></h2>
        <h3>Conexão ESP8266</h3>
        <form name="myForm" action="/connect" onsubmit="return validateForm()" method="post">
            <div class="field-group">
                <select class='text-field' name='ssid'></select>
            </div>
            <br>
            <div class="field-group">
                <input class="text-field" type="password" name="password" length=64 placeholder="Password">
            </div>
            <br>
            <div id="statusDiv">
                <br><br>
            </div>
            <div class="button-container">
                <input class="button" type="submit" value="Conectar">
            </div>
        </form>
        <p>OU</p>
        <div class="button-container">
            <button class="button" type="button" onclick="window.location.href='/action_previous_connection'">Conectar à última rede utilizada</button>
        </div>
    </div>
    </body>
    </html>
)=====";

const char *ssid = "Medicare IoT"; // Nome da rede WiFi que será criada
const char *password = "medicare";      // Senha para se conectar nesta rede

EspMQTTClient client;

ESP8266WebServer server(80);

int loadConfig()
{
    int returnValue = 0;
    EEPROM.begin(sizeof(configuration_t) + CONFIG_START);

    if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
        EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
        EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2] &&
        EEPROM.read(CONFIG_START + 3) == CONFIG_VERSION[3] &&
        EEPROM.read(CONFIG_START + 4) == CONFIG_VERSION[4])
    {
        EEPROM.get(CONFIG_START, CONFIGURATION);
        returnValue = 1;
    }
    CONFIGURATION.saved = 0;

    EEPROM.end();
    return returnValue;
}

void saveConfig()
{
    EEPROM.begin(sizeof(configuration_t) + CONFIG_START);

    CONFIGURATION.saved = 1;
    EEPROM.put(CONFIG_START, CONFIGURATION);
    EEPROM.commit();

    EEPROM.end();
}

void onConnectionEstablished()
{
    char COMMAND_TOPIC[60];
    sprintf(COMMAND_TOPIC, "medicare/%s/command", SERIE_NUMBER);
    client.subscribe(COMMAND_TOPIC, [](const String &payload)
                     { Serial.println(payload); });

    char VALUES_TOPIC[60];
    sprintf(VALUES_TOPIC, "medicare/%s/values", SERIE_NUMBER);
    client.publish(VALUES_TOPIC, "Connected");
}

String listSSID()
{
    String index = (const __FlashStringHelper *)MAIN_page; // Leia o conteúdo HTML
    String networks = "";
    int n = WiFi.scanNetworks();
    Serial.println("Scan done.");
    if (n == 0)
    {
        Serial.println("Nenhuma rede encontrada.");
        index.replace("<select class='text-field' name='ssid'></select>", "<select class='text-field' name='ssid'><option value='' disabled selected>Nenhuma rede encontrada</option></select>");
        index.replace("<br><br>", "<p id='status' style='color:red;'>Rede não encontrada.</p>");
        return index;
    }
    else
    {
        Serial.printf("%d networks found.\n", n);
        networks += "<select class='text-field' name='ssid'><option value='' disabled selected>SSID</option>";
        for (int i = 0; i < n; ++i)
        {
            networks += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</option>";
        }
        networks += "</select>";
    }
    index.replace("<select class='text-field' name='ssid'></select>", networks);
    return index;
}

void handleRoot()
{
    String index = listSSID();            // Leia o conteúdo HTML
    server.send(200, "text/html", index); // Enviar pagina Web
}

void startMqttClient()
{
    if (CONFIGURATION.debug)
        client.enableDebuggingMessages();

    client.setWifiCredentials(CONFIGURATION.ssid, CONFIGURATION.password);
    client.setMqttClientName(CONFIGURATION.nodename);
    client.setMqttServer(CONFIGURATION.mqttip, CONFIGURATION.mqttuser, CONFIGURATION.mqttpass, CONFIGURATION.mqttport);
}

void handleForm()
{
    String ssidWifi = server.arg("ssid");
    String passwordWifi = server.arg("password");
    Serial.printf("SSID: %s\n", ssidWifi.c_str());
    Serial.printf("Password: %s\n", passwordWifi.c_str());
    if (!ssidWifi.equals("") && !passwordWifi.equals(""))
    {
        memset(CONFIGURATION.ssid, 0, sizeof(CONFIGURATION.ssid));
        strcpy(CONFIGURATION.ssid, ssidWifi.c_str());
        memset(CONFIGURATION.password, 0, sizeof(CONFIGURATION.password));
        strcpy(CONFIGURATION.password, passwordWifi.c_str());
        saveConfig();
        startMqttClient();
    }
}

void startWebServer()
{
    server.on("/", handleRoot);
    server.on("/connect", handleForm);
    server.begin();

    Serial.println("Servidor HTTP iniciado");
}

void startAccessPoint()
{
    WiFi.softAP(ssid, password);
    Serial.print("Access Point \"");
    Serial.print(ssid);
    Serial.println("\" started");
    Serial.print("IP address:\t");
    Serial.println(WiFi.softAPIP());
}

void setup()
{
    Serial.begin(115200);
    delay(2000);

    if (!loadConfig())
        saveConfig();

    if (CONFIGURATION.saved)
    {
        Serial.printf("Starting MQTT %d\n", CONFIGURATION.saved);
        startMqttClient();
    } else
    {
        startAccessPoint();
        startWebServer();
    }
}

void loop()
{
    server.handleClient();
    if (CONFIGURATION.saved)
        client.loop();
}