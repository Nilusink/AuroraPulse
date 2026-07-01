#include "secrets.hpp"

#ifdef ESP32

#include <mbedtls/md.h>

#else

#include <bearssl/bearssl_hash.h>

#endif

static const char hex[] = "0123456789abcdef";
void secrets::hmac_sha256(
    const uint8_t *key,
    size_t key_len,
    const uint8_t *data,
    size_t data_len,
    char *result
)
{
    uint8_t output[32];

#ifdef ESP32  // use simpler built-in API

    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *info;

    mbedtls_md_init(&ctx);
    info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_setup(&ctx, info, 1);

    mbedtls_md_hmac_starts(&ctx, key, key_len);
    mbedtls_md_hmac_update(&ctx, data, data_len);
    mbedtls_md_hmac_finish(&ctx, output);

    mbedtls_md_free(&ctx);

#else  // manual HMAC for ESP8266

    uint8_t k_ipad[64] = {0};
    uint8_t k_opad[64] = {0};
    uint8_t tk[32];

    const uint8_t *k = key;

    // If key > 64 bytes, hash it first
    if (key_len > 64) {
        br_sha256_context c;
        br_sha256_init(&c);
        br_sha256_update(&c, key, key_len);
        br_sha256_out(&c, tk);
        k = tk;
        key_len = 32;
    }

    for (size_t i = 0; i < key_len; i++) {
        k_ipad[i] = k[i] ^ 0x36;
        k_opad[i] = k[i] ^ 0x5c;
    }

    // inner hash
    br_sha256_context c;
    uint8_t inner[32];

    br_sha256_init(&c);
    br_sha256_update(&c, k_ipad, 64);
    br_sha256_update(&c, data, data_len);
    br_sha256_out(&c, inner);

    // outer hash
    br_sha256_init(&c);
    br_sha256_update(&c, k_opad, 64);
    br_sha256_update(&c, inner, 32);
    br_sha256_out(&c, output);

#endif

    for (int i = 0; i < 32; ++i) {
        result[i * 2]     = hex[output[i] >> 4];
        result[i * 2 + 1] = hex[output[i] & 0x0F];
    }

    result[64] = '\0';
}
