#pragma once
#include <az_iot_hub_client.h>
#include <az_span.h>
#include <az_result.h>

namespace azure_compat
{
  inline az_result get_user_name(const az_iot_hub_client *client,
                                 char *out, size_t out_size, size_t *out_len)
  {
    return az_iot_hub_client_get_user_name(client, out, out_size, out_len);
  }
  inline az_result get_client_id(const az_iot_hub_client *client,
                                 char *out, size_t out_size, size_t *out_len)
  {
    return az_iot_hub_client_get_client_id(client, out, out_size, out_len);
  }
  inline az_result telemetry_get_publish_topic(const az_iot_hub_client *client,
                                               char *out, size_t out_size, size_t *out_len)
  {
    return az_iot_hub_client_telemetry_get_publish_topic(client, nullptr, out, out_size, out_len);
  }
}
