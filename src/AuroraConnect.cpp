#include "AuroraConnect.hpp"
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <time.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif

#ifdef ESP32
#include <WiFi.h>
#include <HTTPClient.h>
#endif


using namespace Aurora;

// constructors
Connection::Connection(IPAddress manager_address, uint16_t manager_port, IPAddress gateway, IPAddress subnet, IPAddress dns)
  : manager_address(manager_address), manager_port(manager_port), gateway(gateway), subnet(subnet), dns(dns)
{
    client.setReuse(true);
}

Connection::Connection(IPAddress manager_address, uint16_t manager_port, IPAddress gateway, IPAddress subnet)
  : manager_address(manager_address), manager_port(manager_port), gateway(gateway), subnet(subnet), dns(1, 1, 1, 1)
{
    client.setReuse(true);
}


// private methods
inline void Connection::reset_buffer()
{
    memset(url_buff, 0, AC_REQUREST_BUFF_SIZE);
    memset(signature, 0, HMAC_SHA256_OUTPUT_SIZE);
    doc.clear();
}


void Connection::send_keepalive()
{
    // reset stuff
    reset_buffer();

    // get current time
    time_t now;
    time(&now);

    // send keep msg alive to server
    snprintf(
        url_buff,
        AC_REQUREST_BUFF_SIZE-1,
        "http://%s:%d/device/%d/keepalive?secret=%ld",
        manager_address.toString(),
        manager_port,
        DEVICE_ID,
        device_secret
    );
    if (client.begin(wifiClient, url_buff))
    {
        // hijack url buffer for payload
        snprintf(
            url_buff,
            AC_REQUREST_BUFF_SIZE-1,
            "{\"token\":%s,\"ts\":\"%d\"}",
            session_token,
            now
        );

        client.addHeader("Content-Type", "application/json");
        int code = client.POST(url_buff);

        if (code > 0)
        {
            if (code == HTTP_CODE_OK)
            {
                Serial.println("Sent keep-alive to server");
            }
            else
            {
                if (code == HTTP_CODE_UNAUTHORIZED)
                {
                    Serial.println("Session token expired or invalid, re-registering device...");
                    authorized = false;
                }
                else
                {
                    Serial.printf("Failed to send keep-alive, HTTP code: %d\n", code);
                }
            }
        }
        else
        {
            Serial.printf("Failed to send keep-alive, error: %s\n", client.errorToString(code).c_str());
        }
    }

    // close client connection
    client.end();
}


void Connection::update_wrapper(void *pv)
{
    Connection *conn = static_cast<Connection*>(pv);

    for (;;)
    {
        uint32_t now = millis();
        conn->update(now);

        Serial.printf(
            "Remaining a stack: %u\n",
            uxTaskGetStackHighWaterMark(NULL)
        );
    }
}


// public methods
bool Connection::initialize(const char* ssid, const char* password)
{
    // reset stuff
    connected = false;
    reset_buffer();

    // connect to wifi network
    WiFi.begin(ssid, password);

    // wait for wifi to connect
    Serial.println("Connecting ...");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(100);
        Serial.print(".");
    }

    Serial.print("\nConnected with IP address: ");
    Serial.print(WiFi.localIP());
    Serial.println(", configurint static ip...");

    // request ip for device id
    snprintf(
        url_buff, 
        AC_REQUREST_BUFF_SIZE-1,
        "http://%s:%d/device/%d/address",
        manager_address.toString(),
        manager_port,
        DEVICE_ID
    );
    if (client.begin(wifiClient, url_buff))
    {
        int http_code = client.GET();

        // check for valid response
        if (http_code > 0)
        {
            if (http_code == HTTP_CODE_OK)
            {
                String payload = client.getString();
                
                // convert to json
                DeserializationError error = deserializeJson(doc, payload);

                 // Check for parsing errors
                if (error) {
                    Serial.print(F("deserializeJson() failed: "));
                    Serial.println(error.f_str());
                    client.end();
                    return false;
                }
            }
            else
            {
                Serial.printf("Failed to get IP address from server, HTTP code: %d\n", http_code);
                client.end();
                return false;
            }
        }
        else
        {
            Serial.printf("Failed to connect to server, error: %s\n", client.errorToString(http_code).c_str());
            client.end();
            return false;
        }
    }
    else
    {
        Serial.println("Failed to initialize HTTP client");
        client.end();
        return false;
    }
    client.end();

    // set static ip
    String static_ip = doc["ip"];
    port = doc["port"];
    local_ip.fromString(static_ip);

    Serial.printf("got assigned ip: %s\n", static_ip.c_str());

    if (!WiFi.config(local_ip, gateway, subnet, dns))
    {
        Serial.println("Failed to configure static IP");
        return false;
    }
    Serial.println("reconfigured static ip");

    // wait for wifi to connect
    Serial.println("Connecting ...");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(100);
        Serial.print(".");
    }

    // NTP setup
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    // wait for time to sync
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) {
        delay(500);
    }

    // enable OTA
    ArduinoOTA.begin();

    // update terminal
    Serial.print("\nConnected with IP address: ");
    Serial.println(WiFi.localIP());

    connected = true;
    return true;
}


bool Connection::register_device()
{
    reset_buffer();

    // get current time
    time_t now;
    time(&now);

    // generate signature (hijack url_buff for data)
    snprintf(url_buff, AC_REQUREST_BUFF_SIZE-1, "%d|%d", DEVICE_ID, now);

    secrets::hmac_sha256(
        (const uint8_t*)device_secret,
        SECRET_LENGTH,
        (const uint8_t*)url_buff,
        strlen(url_buff),
        signature
    );

    // reset url buff again
    memset(url_buff, 0, AC_REQUREST_BUFF_SIZE);

    // generate URL
    snprintf(
        url_buff,
        AC_REQUREST_BUFF_SIZE-1,
        "http://%s:%d/device/%d/register",
        manager_address.toString(),
        manager_port,
        DEVICE_ID
    );

    if (client.begin(wifiClient, url_buff))
    {
        // hijack url buffer again for payload
        snprintf(
            url_buff,
            AC_REQUREST_BUFF_SIZE-1,
            "{\"ts\":%d,\"sig\":\"%s\"}",
            now,
            signature
        );

        // send POST data
        client.addHeader("Content-Type", "application/json");
        int code = client.POST(url_buff);

        if (code)
        {
            if (code == HTTP_CODE_OK)
            {
                String payload = client.getString();
                
                // convert to json
                DeserializationError error = deserializeJson(doc, payload);

                 // Check for parsing errors
                if (error) {
                    Serial.print(F("deserializeJson() failed: "));
                    Serial.println(error.f_str());
                    client.end();
                    return false;
                }
            }
            else 
            {
                // close client
                client.end();

                // check code
                if (code == HTTP_CODE_UNAUTHORIZED)
                {
                    Serial.println("Invalid device secret!");
                    in_idle = true;
                }
                else if (code == HTTP_CODE_CONFLICT)
                {
                    Serial.println("Device already registered...");
                }
                else
                {
                    Serial.printf("Failed to get IP address from server, HTTP code: %d\n", code);
                }

                // return error
                return false;
            }
        }
        {
            client.end();
            return false;
        }
    }
    else
    {
        client.end();
        return false;
    }

    // process response
    snprintf(session_token, SESSION_TOKEN_LENGTH, "%d", doc["session_token"]);
    token_expiry = doc["expires"];

    // change state
    authorized = true;
    return true;
}


void Connection::start_task()
{
    xTaskCreate(
        update_wrapper,
        "connection task",
        2048,
        this,
        1,
        NULL
    );
}


void Connection::update(uint32_t timestamp)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi connection lost, attempting to reconnect...");
        connected = false;
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(100);
            Serial.print(".");
        }
        Serial.println("\nReconnected to WiFi!");
        connected = true;
    }
    else
    {
        // handle OTA
        ArduinoOTA.handle();

        if (authorized)
        {
            // send periodic updates
            if (timestamp - last_update > update_interval)
            {
                send_keepalive();
            }
        }
        else
        {
            register_device();
            delay(200);  // don't spam the server
        }
    }
}


// setters
void Connection::set_update_interval(uint16_t interval)
{
    update_interval = interval;
}


// getters
uint16_t Connection::get_port()
{
    return port;
}

bool Connection::get_connected()
{
    return connected;
}

bool Connection::get_authorized()
{
    return authorized;
}

