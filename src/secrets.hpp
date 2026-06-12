/**
 * @file secrets.hpp
 * @author Nilusink
 * @brief secret hashing for Aurora IOT devices
 * @version 0.1
 * @date 2026-06-12
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once
#include <Arduino.h>


#define HMAC_SHA256_OUTPUT_SIZE 65


namespace secrets
{
    void hmac_sha256(
        const uint8_t *key,
        size_t key_len,
        const uint8_t *data,
        size_t data_len,
        char *result
    );
} // namespace secrets
