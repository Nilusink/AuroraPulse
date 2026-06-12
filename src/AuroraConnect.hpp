/**
 * @file AuroraConnect.hpp
 * @author Nilusink
 * @brief Connection for Aurora IOT devices
 * @version 0.1
 * @date 2026-06-11
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once
#include <ArduinoJson.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif

#ifdef ESP32
#include <WiFi.h>
#include <HTTPClient.h>
#endif

#include "secrets.hpp"


#define AC_REQUREST_BUFF_SIZE 256
#define SECRET_LENGTH 64
#define SESSION_TOKEN_LENGTH 16


namespace Aurora
{
    class Connection
    {
        private:
            HTTPClient client;
            WiFiClient wifiClient;

            IPAddress manager_address;
            IPAddress local_ip;
            uint16_t port = 80;
            IPAddress gateway;
            IPAddress subnet;
            IPAddress dns;

            // buffer variables
            char url_buff[AC_REQUREST_BUFF_SIZE];
            char signature[HMAC_SHA256_OUTPUT_SIZE];
            JsonDocument doc;

            // secrets
            char device_secret[SECRET_LENGTH];
            char session_token[SESSION_TOKEN_LENGTH];
            unsigned long token_expiry = 0;

            // states
            bool connected = false;
            bool authorized = false;
            bool in_idle = false;  // invalid token, no connection possible

            // config
            uint32_t last_update = 0;
            uint16_t update_interval = 2000; // 2 seconds

            inline void reset_buffer();

            void send_keepalive();
            static void update_wrapper(void *pv);

        public:
            Connection(IPAddress manager_address, IPAddress gateway, IPAddress subnet, IPAddress dns);
            Connection(IPAddress manager_address, IPAddress gateway, IPAddress subnet);

            /**
             * @brief initialize connection and get assigned IP
             * 
             */
            bool initialize(const char* ssid, const char* password);

            /**
             * @brief register device with Aurora server
             * 
             */
            bool register_device();

            /**
             * @brief start connection task. DON'T call update if you use this.
             * 
             */
            void start_task();

            /**
             * @brief update connection status
             * 
             */
            void update(uint32_t timestamp);

            /**
             * @brief set keep alive interval
             * 
             * @param interval interval to use
             */
            void set_update_interval(uint16_t interval);

            /**
             * @brief get assigned port
             * 
             * @return uint16_t port assigned by manager
             */
            uint16_t get_port();

            bool get_connected();
            bool get_authorized();
    };
}
