#include "azure_AzIoTSasToken.h"
#include <time.h>
#include <string.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>

static uint64_t now() { return (uint64_t)time(NULL); }
static void hmac(az_span k, az_span p, az_span o)
{
    mbedtls_md_context_t c;
    mbedtls_md_init(&c);
    mbedtls_md_setup(&c, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&c, (const unsigned char *)az_span_ptr(k), az_span_size(k));
    mbedtls_md_hmac_update(&c, (const unsigned char *)az_span_ptr(p), az_span_size(p));
    mbedtls_md_hmac_finish(&c, (unsigned char *)az_span_ptr(o));
    mbedtls_md_free(&c);
}
static az_result b64e(az_span s, az_span d, az_span *o)
{
    size_t n = 0;
    int rc = mbedtls_base64_encode((unsigned char *)az_span_ptr(d), (size_t)az_span_size(d), &n, (const unsigned char *)az_span_ptr(s), (size_t)az_span_size(s));
    if (rc != 0)
        return AZ_ERROR_NOT_ENOUGH_SPACE;
    *o = az_span_slice(d, 0, (int)n);
    return AZ_OK;
}
AzIoTSasToken::AzIoTSasToken(az_iot_hub_client *c, az_span dk, az_span sb, az_span tb) : client(c), deviceKey(dk), signatureBuffer(sb), sasTokenBuffer(tb), sasToken(AZ_SPAN_EMPTY), expirationUnixTime(0) {}
az_result AzIoTSasToken::Generate(unsigned int m)
{
    uint64_t e = now() + (uint64_t)(m * 60ULL);
    expirationUnixTime = e;
    uint8_t buf[256];
    az_span to = AZ_SPAN_FROM_BUFFER(buf);
    az_span out;
    az_result r = az_iot_hub_client_sas_get_signature(client, e, to, &out);
    if (az_result_failed(r))
        return r;
    to = out;
    size_t dl = 0;
    int rc = mbedtls_base64_decode((unsigned char *)az_span_ptr(signatureBuffer), (size_t)az_span_size(signatureBuffer), &dl, (const unsigned char *)az_span_ptr(deviceKey), (size_t)az_span_size(deviceKey));
#ifdef MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL if (rc == MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) return AZ_ERROR_NOT_ENOUGH_SPACE;
#endif if (rc != 0) return AZ_ERROR_ARG;
    az_span dk = az_span_slice(signatureBuffer, 0, (int)dl);
    az_span ho = az_span_slice(signatureBuffer, 0, 32);
    hmac(dk, to, ho);
    if ((int)dl > 32)
    {
        uint8_t *p = az_span_ptr(signatureBuffer);
        memset(p + 32, 0, (int)dl - 32);
    }
    uint8_t sbuf[256];
    az_span bs = AZ_SPAN_FROM_BUFFER(sbuf);
    r = b64e(ho, bs, &out);
    if (az_result_failed(r))
        return r;
    bs = out;
    size_t pl = 0;
    r = az_iot_hub_client_sas_get_password(client, e, bs, AZ_SPAN_EMPTY, (char *)az_span_ptr(sasTokenBuffer), (size_t)az_span_size(sasTokenBuffer), &pl);
    if (az_result_failed(r))
        return r;
    sasToken = az_span_slice(sasTokenBuffer, 0, (int)pl);
    return AZ_OK;
}
bool AzIoTSasToken::IsExpired() const { return now() >= expirationUnixTime; }
bool AzIoTSasToken::IsExpiringSoon(unsigned int s) const { return now() + (uint64_t)s >= expirationUnixTime; }
az_span AzIoTSasToken::Get() const { return sasToken; }