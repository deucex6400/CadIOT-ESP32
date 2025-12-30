#ifndef AZURE_AZIOTSASTOKEN_H
#define AZURE_AZIOTSASTOKEN_H
#include <Arduino.h>
#include <az_iot_hub_client.h>
#include <az_span.h>
#include <az_result.h>

class AzIoTSasToken
{
public:
    AzIoTSasToken(az_iot_hub_client *c, az_span dk, az_span sb, az_span tb);
    az_result Generate(unsigned int m);
    bool IsExpired() const;
    bool IsExpiringSoon(unsigned int s = 300) const;
    az_span Get() const;

private:
    az_iot_hub_client *client;
    az_span deviceKey;
    az_span signatureBuffer;
    az_span sasTokenBuffer;
    az_span sasToken;
    uint64_t expirationUnixTime;
};
#endif