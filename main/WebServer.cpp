/*
 * Web server for lamp management, HTTP OTA updates, and light control.
 *
 * Endpoints:
 *   GET  /           - Status page (HTML)
 *   GET  /update     - Firmware upload form (HTML)
 *   POST /update     - Receive firmware binary and flash via OTA
 *   POST /reboot     - Reboot the device
 *   POST /factory-reset - Factory reset and reboot
 *   GET  /api/light  - Get light state (JSON)
 *   PUT  /api/light  - Set light state (JSON)
 *
 * JSON API example:
 *   GET  /api/light → {"on":true,"brightness":200,"color_temp":400,"hue":90,"saturation":255}
 *   PUT  /api/light ← {"on":true}
 *   PUT  /api/light ← {"brightness":128}
 *   PUT  /api/light ← {"color_temp":556}
 *   PUT  /api/light ← {"hue":180,"saturation":200}
 *
 * HA RESTful Light config:
 *   light:
 *     - platform: rest
 *       resource: http://192.168.0.212/api/light
 *       name: "Matter Lamp"
 *       state_resource: http://192.168.0.212/api/light
 */

#include "WebServer.h"

#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <cstring>
#include <cstdio>

#include <app/server/Server.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <platform/CHIPDeviceLayer.h>

static const char * TAG = "web-server";
static httpd_handle_t sServer = nullptr;

// ---------------------------------------------------------------------------
// HTML pages
// ---------------------------------------------------------------------------

static const char STATUS_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Matter Lamp</title>
<style>
body{font-family:sans-serif;max-width:500px;margin:40px auto;padding:0 20px;background:#1a1a2e;color:#eee}
h1{color:#e94560}a{color:#0f3460}
.card{background:#16213e;padding:20px;border-radius:10px;margin:15px 0}
.btn{display:inline-block;padding:10px 20px;margin:5px;border:none;border-radius:5px;cursor:pointer;font-size:16px;color:#fff}
.btn-blue{background:#0f3460}.btn-red{background:#e94560}.btn-green{background:#2d6a4f}
table{width:100%%}td{padding:4px 8px}td:first-child{color:#999}
</style></head><body>
<h1>Matter Lamp</h1>
<div class="card"><table>
<tr><td>Version</td><td>%d (%s)</td></tr>
<tr><td>Uptime</td><td>%d min</td></tr>
<tr><td>Free heap</td><td>%d KB</td></tr>
<tr><td>WiFi RSSI</td><td>%d dBm</td></tr>
<tr><td>IP</td><td>%s</td></tr>
</table></div>
<div class="card">
<a class="btn btn-green" href="/update">Update Firmware</a>
<form style="display:inline" method="POST" action="/reboot">
<button class="btn btn-blue" type="submit">Reboot</button></form>
<form style="display:inline" method="POST" action="/factory-reset"
 onsubmit="return confirm('Factory reset? Device will need re-commissioning.')">
<button class="btn btn-red" type="submit">Factory Reset</button></form>
</div>
<div class="card"><h3 style="margin-top:0">API</h3>
<pre style="color:#999;font-size:13px">GET  /api/light
PUT  /api/light  {"on":true,"brightness":200,"color_temp":400}</pre>
</div></body></html>
)rawliteral";

static const char UPDATE_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Firmware Update</title>
<style>
body{font-family:sans-serif;max-width:500px;margin:40px auto;padding:0 20px;background:#1a1a2e;color:#eee}
h1{color:#e94560}.card{background:#16213e;padding:20px;border-radius:10px;margin:15px 0}
.btn{padding:12px 24px;border:none;border-radius:5px;cursor:pointer;font-size:16px;color:#fff;background:#2d6a4f}
input[type=file]{margin:10px 0;color:#eee}
#progress{display:none;margin:10px 0}
</style></head><body>
<h1>Firmware Update</h1>
<div class="card">
<form method="POST" action="/update" enctype="multipart/form-data" id="form">
<input type="file" name="firmware" accept=".bin" required><br>
<button class="btn" type="submit">Upload & Flash</button>
</form>
<div id="progress">Uploading... please wait</div>
</div>
<script>
document.getElementById('form').onsubmit=function(){
document.getElementById('progress').style.display='block';
};
</script>
<p><a href="/" style="color:#999">&larr; Back</a></p>
</body></html>
)rawliteral";

// ---------------------------------------------------------------------------
// Light control API
// ---------------------------------------------------------------------------

static esp_err_t api_light_get_handler(httpd_req_t * req)
{
    using namespace chip::app::Clusters;

    bool onOff = false;
    uint8_t level = 0;
    uint16_t colorTemp = 0;
    uint8_t colorMode = 0;
    uint8_t hue = 0;
    uint8_t saturation = 0;

    OnOff::Attributes::OnOff::Get(1, &onOff);
    {
        chip::app::DataModel::Nullable<uint8_t> nullableLevel;
        LevelControl::Attributes::CurrentLevel::Get(1, nullableLevel);
        if (!nullableLevel.IsNull()) level = nullableLevel.Value();
    }
    ColorControl::Attributes::ColorTemperatureMireds::Get(1, &colorTemp);
    ColorControl::Attributes::ColorMode::Get(1, &colorMode);
    ColorControl::Attributes::CurrentHue::Get(1, &hue);
    ColorControl::Attributes::CurrentSaturation::Get(1, &saturation);

    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"on\":%s,\"brightness\":%d,\"color_temp\":%d,\"color_mode\":%d,\"hue\":%d,\"saturation\":%d}",
        onOff ? "true" : "false", level, colorTemp, colorMode, hue, saturation);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buf, strlen(buf));
}

// Simple JSON value parser (avoids pulling in a JSON library)
static bool json_get_bool(const char * json, const char * key, bool * out)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char * p = strstr(json, search);
    if (!p) return false;
    p += strlen(search);
    while (*p == ' ') p++;
    *out = (*p == 't');
    return true;
}

static bool json_get_int(const char * json, const char * key, int * out)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char * p = strstr(json, search);
    if (!p) return false;
    p += strlen(search);
    while (*p == ' ') p++;
    *out = atoi(p);
    return true;
}

static esp_err_t api_light_put_handler(httpd_req_t * req)
{
    using namespace chip::app::Clusters;

    char buf[256] = {};
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    bool on;
    int brightness, color_temp, hue, saturation;

    if (json_get_bool(buf, "on", &on))
        OnOff::Attributes::OnOff::Set(1, on);

    if (json_get_int(buf, "brightness", &brightness))
    {
        chip::app::DataModel::Nullable<uint8_t> val;
        val.SetNonNull((uint8_t)brightness);
        LevelControl::Attributes::CurrentLevel::Set(1, val);
    }

    if (json_get_int(buf, "color_temp", &color_temp))
        ColorControl::Attributes::ColorTemperatureMireds::Set(1, (uint16_t)color_temp);

    if (json_get_int(buf, "hue", &hue))
        ColorControl::Attributes::CurrentHue::Set(1, (uint8_t)hue);

    if (json_get_int(buf, "saturation", &saturation))
        ColorControl::Attributes::CurrentSaturation::Set(1, (uint8_t)saturation);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"ok\":true}", 11);
}

// ---------------------------------------------------------------------------
// Management handlers
// ---------------------------------------------------------------------------

static esp_err_t status_handler(httpd_req_t * req)
{
    char buf[sizeof(STATUS_HTML) + 200];
    const esp_app_desc_t * app = esp_app_get_description();

    wifi_ap_record_t ap;
    int rssi = 0;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
        rssi = ap.rssi;

    esp_netif_ip_info_t ip_info;
    esp_netif_t * netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    char ip_str[16] = "unknown";
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));

    int uptime_min = (int)(esp_timer_get_time() / 1000000 / 60);

    snprintf(buf, sizeof(buf), STATUS_HTML,
             CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION,
             app->version,
             uptime_min,
             (int)(esp_get_free_heap_size() / 1024),
             rssi,
             ip_str);

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, buf, strlen(buf));
}

static esp_err_t update_page_handler(httpd_req_t * req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, UPDATE_HTML, strlen(UPDATE_HTML));
}

static esp_err_t update_handler(httpd_req_t * req)
{
    esp_ota_handle_t ota_handle;
    const esp_partition_t * update_partition = esp_ota_get_next_update_partition(nullptr);
    if (!update_partition)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }

    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    char buf[1024];
    int remaining = req->content_len;
    bool header_skipped = false;
    ESP_LOGI(TAG, "OTA upload starting, size: %d", remaining);

    while (remaining > 0)
    {
        int received = httpd_req_recv(req, buf, sizeof(buf) < remaining ? sizeof(buf) : remaining);
        if (received <= 0)
        {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive error");
            return ESP_FAIL;
        }

        // Skip multipart form header on first chunk
        if (!header_skipped)
        {
            char * body = strstr(buf, "\r\n\r\n");
            if (body)
            {
                body += 4;
                int header_len = body - buf;
                err = esp_ota_write(ota_handle, body, received - header_len);
                header_skipped = true;
            }
        }
        else
        {
            err = esp_ota_write(ota_handle, buf, received);
        }

        if (err != ESP_OK)
        {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
            return ESP_FAIL;
        }

        remaining -= received;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA validation failed");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA update successful, rebooting in 2s...");
    const char * resp = "<html><body style='background:#1a1a2e;color:#eee;font-family:sans-serif;text-align:center;padding:60px'>"
                        "<h1 style='color:#2d6a4f'>Update successful!</h1><p>Rebooting in 2 seconds...</p></body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, strlen(resp));

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

static esp_err_t reboot_handler(httpd_req_t * req)
{
    const char * resp = "<html><body style='background:#1a1a2e;color:#eee;font-family:sans-serif;text-align:center;padding:60px'>"
                        "<h1>Rebooting...</h1></body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, strlen(resp));

    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();

    return ESP_OK;
}

static esp_err_t factory_reset_handler(httpd_req_t * req)
{
    const char * resp = "<html><body style='background:#1a1a2e;color:#eee;font-family:sans-serif;text-align:center;padding:60px'>"
                        "<h1 style='color:#e94560'>Factory reset...</h1><p>Device will need re-commissioning.</p></body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, strlen(resp));

    vTaskDelay(pdMS_TO_TICKS(500));
    chip::Server::GetInstance().ScheduleFactoryReset();

    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Server init
// ---------------------------------------------------------------------------

void WebServerStart(void)
{
    if (sServer)
        return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = 10;

    if (httpd_start(&sServer, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start web server");
        return;
    }

    httpd_uri_t status_uri      = { .uri = "/",              .method = HTTP_GET,  .handler = status_handler };
    httpd_uri_t update_page_uri = { .uri = "/update",        .method = HTTP_GET,  .handler = update_page_handler };
    httpd_uri_t update_uri      = { .uri = "/update",        .method = HTTP_POST, .handler = update_handler };
    httpd_uri_t reboot_uri      = { .uri = "/reboot",        .method = HTTP_POST, .handler = reboot_handler };
    httpd_uri_t reset_uri       = { .uri = "/factory-reset", .method = HTTP_POST, .handler = factory_reset_handler };
    httpd_uri_t api_get_uri     = { .uri = "/api/light",     .method = HTTP_GET,  .handler = api_light_get_handler };
    httpd_uri_t api_put_uri     = { .uri = "/api/light",     .method = HTTP_PUT,  .handler = api_light_put_handler };

    httpd_register_uri_handler(sServer, &status_uri);
    httpd_register_uri_handler(sServer, &update_page_uri);
    httpd_register_uri_handler(sServer, &update_uri);
    httpd_register_uri_handler(sServer, &reboot_uri);
    httpd_register_uri_handler(sServer, &reset_uri);
    httpd_register_uri_handler(sServer, &api_get_uri);
    httpd_register_uri_handler(sServer, &api_put_uri);

    ESP_LOGI(TAG, "Web server started on port %d", config.server_port);
}
