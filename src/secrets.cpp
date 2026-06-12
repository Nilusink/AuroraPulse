#include "mbedtls/md.h"
#include "secrets.hpp"


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

    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *info;

    // generate HMAC sequence
    mbedtls_md_init(&ctx);

    info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_setup(&ctx, info, 1);

    mbedtls_md_hmac_starts(&ctx, key, key_len);
    mbedtls_md_hmac_update(&ctx, data, data_len);
    mbedtls_md_hmac_finish(&ctx, output);

    mbedtls_md_free(&ctx);

    // write to result as hex string
    for (int i = 0; i < 32; i++) {
        result[i * 2]     = hex[output[i] >> 4];
        result[i * 2 + 1] = hex[output[i] & 0x0F];
    }

    result[64] = '\0';
}
