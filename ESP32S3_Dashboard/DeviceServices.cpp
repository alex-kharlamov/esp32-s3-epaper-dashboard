#include "DeviceServices.h"
#include "HardwareServices.h"
#include "LiveData.h"
#include "PowerManager.h"
#include "RuntimeSettings.h"
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <atomic>
#include <esp_ota_ops.h>
#include <esp_system.h>
// Override the Arduino core's default immediate OTA acceptance. The bundled
// bootloader has rollback enabled; accept only after a successful panel draw.
extern "C" bool verifyRollbackLater() { return true; }
namespace {
WebServer server(80);
std::atomic<bool> requested{false};
std::atomic<bool> active{false};
bool uploadOk = false, uploadStarted = false;
uint32_t portalStart = 0, rebootAt = 0;
String password, csrf;
char lastFault[161]{};
unsigned faults = 0;
uint32_t otaPending = 0, otaPrevious = 0;
String randomSecret() {
  char out[33];
  for (int i = 0; i < 4; i++)
    snprintf(out + i * 8, 9, "%08lx", (unsigned long)esp_random());
  return out;
}
void clearPending() {
  Preferences p;
  if (p.begin("dashota", false)) {
    p.remove("pending");
    p.remove("attempts");
    p.end();
  }
  otaPending = 0;
}
bool rollback() {
  const auto *running = esp_ota_get_running_partition();
  const auto *previous = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
  esp_partition_iterator_t it = esp_partition_find(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, nullptr);
  previous = nullptr;
  while (it) {
    const auto *part = esp_partition_get(it);
    if (part->address == otaPrevious)
      previous = part;
    it = esp_partition_next(it);
  }
  if (!previous || previous->address == running->address ||
      esp_ota_set_boot_partition(previous) != ESP_OK)
    return false;
  clearPending();
  return true;
}
bool auth() {
  if (server.authenticate("admin", password.c_str()))
    return true;
  server.requestAuthentication();
  return false;
}
bool action() {
  if (!auth())
    return false;
  if (server.arg("csrf") != csrf) {
    server.send(403, "text/plain", "Invalid session token");
    return false;
  }
  return true;
}
void jsonReply(JsonDocument &d) {
  String body;
  serializeJson(d, body);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", body);
}
void status(JsonDocument &d) {
  liveDiagnostics(d);
  hardwareJson(d);
  d["firmware"] = "v21";
  d["maintenance"] = active.load();
  d["fault_count"] = faults;
  d["last_fault"] = lastFault;
  d["ota_pending"] = otaPending != 0;
  d["heap_free"] = ESP.getFreeHeap();
  d["uptime_ms"] = millis();
}
void routes() {
  server.on("/", HTTP_GET, [] {
    if (!auth())
      return;
    String page =
        F("<!doctype html><meta name='viewport' "
          "content='width=device-width'><title>Dashboard "
          "setup</title><style>body{font:18px "
          "system-ui;max-width:760px;margin:2em "
          "auto;padding:1em}textarea,input{font:inherit;width:100%;box-sizing:"
          "border-box;margin:8px "
          "0}button{font:inherit;padding:12px}small{display:block}</"
          "style><h1>Dashboard setup</h1><p>This temporary Wi-Fi session "
          "closes after ten minutes. Saving settings restarts the "
          "dashboard.</p><p><a href='/status'>Device health</a> · <a "
          "href='/settings'>Current settings</a></p><form method='post' "
          "action='/settings'><label>Settings (JSON)</label><textarea "
          "name='json' rows='15'>");
    JsonDocument d;
    settingsJson(d);
    String json;
    serializeJsonPretty(d, json);
    page += json;
    page += F("</textarea><input type='hidden' name='csrf' value='");
    page += csrf;
    page += F("'><button>Save settings</button></form><h2>Wi-Fi</h2><form "
              "method='post' action='/wifi'><input name='ssid' "
              "placeholder='Network name' maxlength='32' required><input "
              "name='password' type='password' placeholder='Password' "
              "maxlength='63'><input type='hidden' name='csrf' value='");
    page += csrf;
    page +=
        F("'><button>Save Wi-Fi</button></form><h2>Firmware "
          "update</h2><p>Choose the application .bin, then paste its SHA-256 "
          "checksum from the release or build. Keep power connected. An update "
          "is accepted after the first successful dashboard draw.</p><form "
          "id='ota' method='post' enctype='multipart/form-data'><input "
          "id='hash' placeholder='64-character SHA-256' "
          "pattern='[0-9a-fA-F]{64}' required><input type='file' "
          "name='firmware' accept='.bin' required><button>Upload "
          "firmware</button></"
          "form><script>document.getElementById('ota').onsubmit=function(){"
          "this.action='/update?csrf=");
    page += csrf;
    page += F("&sha256='+encodeURIComponent(document.getElementById('hash')."
              "value)}</script><form method='post' action='/rollback'><input "
              "type='hidden' name='csrf' value='");
    page += csrf;
    page += F("'><button>Restore previous firmware</button></form>");
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/html", page);
  });
  server.on("/settings", HTTP_GET, [] {
    if (!auth())
      return;
    JsonDocument d;
    settingsJson(d);
    jsonReply(d);
  });
  server.on("/status", HTTP_GET, [] {
    if (!auth())
      return;
    JsonDocument d;
    status(d);
    jsonReply(d);
  });
  server.on("/settings", HTTP_POST, [] {
    if (!action())
      return;
    String body = server.arg("json");
    if (body.length() > 2048) {
      server.send(413, "text/plain", "Settings too large");
      return;
    }
    JsonDocument d;
    String error;
    if (deserializeJson(d, body) ||
        !settingsSave(d.as<JsonVariantConst>(), error)) {
      server.send(400, "text/plain", error.length() ? error : "Invalid JSON");
      return;
    }
    server.send(200, "text/plain",
                "Saved. Restarting; allow the panel startup cooldown.");
    rebootAt = millis() + 1000;
  });
  server.on("/wifi", HTTP_POST, [] {
    if (!action())
      return;
    String ssid = server.arg("ssid"), pass = server.arg("password");
    if (!ssid.length() || ssid.length() > 32 || pass.length() > 63) {
      server.send(400, "text/plain", "Invalid network credentials");
      return;
    }
    Preferences p;
    bool ok = p.begin("dashboardwifi", false);
    if (ok) {
      ok = p.putString("ssid", ssid) > 0;
      p.putString("pass", pass);
      ok = p.isKey("pass") && p.getString("pass") == pass && ok;
      p.end();
    }
    pass.clear();
    server.send(ok ? 200 : 500, "text/plain",
                ok ? "Saved. Restarting." : "Storage failure");
    if (ok)
      rebootAt = millis() + 1000;
  });
  server.on("/rollback", HTTP_POST, [] {
    if (!action())
      return;
    if (!rollback()) {
      server.send(409, "text/plain",
                  "No verified previous firmware is available");
      return;
    }
    server.send(200, "text/plain", "Restoring previous firmware.");
    rebootAt = millis() + 1000;
  });
  server.on(
      "/update", HTTP_POST,
      [] {
        if (!action())
          return;
        server.send(uploadOk ? 200 : 400, "text/plain",
                    uploadOk ? "Verified. Restarting into new firmware."
                             : "Upload rejected: check application binary, "
                               "size and SHA-256.");
        if (uploadOk)
          rebootAt = millis() + 1000;
      },
      [] {
        if (!server.authenticate("admin", password.c_str()) ||
            server.arg("csrf") != csrf)
          return;
        auto &u = server.upload();
        if (u.status == UPLOAD_FILE_START) {
          uploadOk = false;
          uploadStarted = false;
          String hash = server.arg("sha256");
          bool valid = hash.length() == 64;
          for (char c : hash)
            valid &= isxdigit((unsigned char)c);
          if (!valid)
            return;
          const auto *next = esp_ota_get_next_update_partition(nullptr);
          const auto *running = esp_ota_get_running_partition();
          if (!next || next->address == running->address)
            return;
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH) ||
              !Update.setSHA256(hash.c_str())) {
            Update.abort();
            return;
          }
          Preferences p;
          if (!p.begin("dashota", false)) {
            Update.abort();
            return;
          }
          bool saved = p.putUInt("previous", running->address) > 0;
          saved = p.putUInt("pending", next->address) > 0 && saved;
          saved = p.putUInt("attempts", 0) > 0 && saved;
          p.end();
          if (!saved) {
            Update.abort();
            clearPending();
            return;
          }
          otaPrevious = running->address;
          otaPending = next->address;
          uploadStarted = true;
        } else if (u.status == UPLOAD_FILE_WRITE) {
          if (uploadStarted &&
              Update.write(u.buf, u.currentSize) != u.currentSize) {
            Update.abort();
            uploadStarted = false;
            clearPending();
          }
        } else if (u.status == UPLOAD_FILE_END) {
          if (uploadStarted) {
            uploadOk = Update.end(true);
            uploadStarted = false;
            if (!uploadOk)
              clearPending();
          }
        } else if (u.status == UPLOAD_FILE_ABORTED) {
          Update.abort();
          uploadStarted = false;
          clearPending();
        }
      });
  server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
}
} // namespace
void deviceBegin() {
  Preferences p;
  if (p.begin("dashfault", true)) {
    faults = p.getUInt("count", 0);
    String reason = p.getString("reason", "");
    snprintf(lastFault, sizeof(lastFault), "%s", reason.c_str());
    p.end();
  }
  if (p.begin("dashota", false)) {
    otaPending = p.getUInt("pending", 0);
    otaPrevious = p.getUInt("previous", 0);
    uint32_t attempts = p.getUInt("attempts", 0);
    auto *running = esp_ota_get_running_partition();
    if (otaPending && running->address == otaPending) {
      if (attempts) {
        p.end();
        if (rollback())
          ESP.restart();
        Serial.println("OTA_ROLLBACK_FAILED: USB recovery required");
      } else {
        p.putUInt("attempts", 1);
        p.end();
        Serial.println("OTA_PENDING: first dashboard draw must succeed");
      }
    } else {
      p.remove("pending");
      p.remove("attempts");
      otaPending = 0;
      p.end();
    }
  }
  routes();
}
bool deviceMaintenanceRequested() { return requested.load() || active.load(); }
void deviceService() {
  if (rebootAt && int32_t(millis() - rebootAt) >= 0) {
    ESP.restart();
    return;
  }
  if (requested.load() && !active && !liveNetworkActive() &&
      !powerDisplayActive()) {
    powerNetworkBegin();
    WiFi.mode(WIFI_AP);
    password = randomSecret();
    csrf = randomSecret();
    if (!WiFi.softAP("Epaper-Setup", password.c_str(), 1, false, 1)) {
      WiFi.mode(WIFI_OFF);
      powerNetworkEnd();
      requested = false;
      Serial.println("PORTAL_FAILED");
      return;
    }
    server.begin();
    active = true;
    portalStart = millis();
    Serial.printf("PORTAL_READY: join Epaper-Setup; password=%s; open "
                  "http://192.168.4.1; user=admin, same password; closes in 10 "
                  "minutes.\n",
                  password.c_str());
  }
  if (active) {
    server.handleClient();
    if (millis() - portalStart >= 600000 && uploadStarted) {
      Update.abort();
      uploadStarted = false;
      uploadOk = false;
      clearPending();
      Serial.println("OTA_ABORTED: maintenance session expired");
    }
    if ((!requested.load() || millis() - portalStart >= 600000) &&
        !uploadStarted) {
      server.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_OFF);
      active = false;
      requested = false;
      password = "";
      csrf = "";
      powerNetworkEnd();
      Serial.println("PORTAL_CLOSED: normal operation resumed");
    }
  }
}
bool deviceHandleCommand(const String &line) {
  if (line == "CONFIG CLOSE") {
    if (uploadStarted) {
      Update.abort();
      uploadStarted = false;
      clearPending();
    }
    requested = false;
    Serial.println("PORTAL_CLOSE_REQUESTED");
    return true;
  }
  if (line == "CONFIG PORTAL") {
    requested = true;
    Serial.println("PORTAL_REQUESTED: waiting for safe radio/display handover");
    return true;
  }
  if (line == "DIAGNOSTICS") {
    JsonDocument d;
    status(d);
    serializeJson(d, Serial);
    Serial.println();
    return true;
  }
  if (line == "CONFIG SHOW") {
    JsonDocument d;
    settingsJson(d);
    serializeJson(d, Serial);
    Serial.println();
    return true;
  }
  if (line.startsWith("CONFIG ")) {
    JsonDocument d;
    String error;
    auto parse = deserializeJson(d, line.substring(7));
    if (parse || !settingsSave(d.as<JsonVariantConst>(), error))
      Serial.printf("CONFIG_FAILED: %s\n",
                    parse ? "Invalid JSON" : error.c_str());
    else {
      Serial.println("CONFIG_SAVED: restarting");
      rebootAt = millis() + 1000;
    }
    return true;
  }
  if (line == "FAULT RESET") {
    Preferences p;
    if (p.begin("dashfault", false)) {
      p.clear();
      p.end();
    }
    ESP.restart();
    return true;
  }
  return false;
}
void deviceMarkHealthy() {
  esp_ota_img_states_t state;
  bool pending = esp_ota_get_state_partition(esp_ota_get_running_partition(),
                                             &state) == ESP_OK &&
                 state == ESP_OTA_IMG_PENDING_VERIFY;
  if (otaPending || pending) {
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
      clearPending();
      Serial.println("OTA_HEALTHY: dashboard draw completed");
    }
  }
  if (faults && millis() > 1800000) {
    Preferences p;
    if (p.begin("dashfault", false)) {
      p.putUInt("count", 0);
      p.end();
    }
    faults = 0;
  }
}
[[noreturn]] void deviceFault(const char *stage, const char *reason,
                              bool recoverable) {
  snprintf(lastFault, sizeof(lastFault), "%s: %s", stage, reason);
  ++faults;
  Preferences p;
  if (p.begin("dashfault", false)) {
    p.putUInt("count", faults);
    p.putString("reason", lastFault);
    p.end();
  }
  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) ==
          ESP_OK &&
      state == ESP_OTA_IMG_PENDING_VERIFY)
    esp_ota_mark_app_invalid_rollback_and_reboot();
  if (otaPending && rollback())
    ESP.restart();
  uint32_t started = millis();
  bool retry = recoverable && faults <= 2;
  Serial.printf("FAULT_RECORDED: %u; recovery=%s\n", faults,
                retry ? "cooldown then restart"
                      : "parked; FAULT RESET or maintenance required");
  for (;;) {
    serviceLiveSetup();
    if (retry && millis() - started >= 180000)
      ESP.restart();
    powerIdleWait(200);
  }
}
