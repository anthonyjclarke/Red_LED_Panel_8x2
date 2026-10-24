#include "PanelNetwork.h"
#include "config.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif
#include <cassert>
#include <fstream>
#include <iostream>
#include <sys/time.h>
uint32_t fakeMillis = 0;
SerialPort Serial;
WiFiPort WiFi;
WebServer* activeServer = nullptr;
std::vector<uint8_t> nvs;
bool nvsWritable = true;
std::function<void(timeval*)> syncCallback;
unsigned ntpRequests = 0;
static std::string token() {
  activeServer->dispatch("/api/status");
  const auto first = activeServer->body.find("\"token\":\"") + 9;
  return activeServer->body.substr(first, 16);
}
int main(int argc, char** argv) {
  assert(argc == 2);
  PanelNetwork network;
  network.begin();
  assert(strcmp(network.settings().location, "Sydney") == 0 && !network.timeValid());
  fakeMillis = WIFI_CONNECT_TIMEOUT_MS - 1;
  network.loop();
  assert(!activeServer);
  ++fakeMillis;
  network.loop();
  assert(network.portalActive() && activeServer);
  activeServer->dispatch("/panel");
  assert(activeServer->code == 200 &&
         activeServer->body.find("Panel settings") != std::string::npos);
  const auto csrf = token();
  assert(csrf.size() == 16);
  activeServer->args = {
      {"location", " Workshop "}, {"zone", "Etc/UTC"}, {"brightness", "75"}, {"view", "clock"}};
  activeServer->dispatch("/api/settings", HTTP_POST);
  assert(activeServer->code == 403);
  activeServer->args["token"] = csrf.c_str();
  activeServer->args["zone"] = "Bad/Zone";
  activeServer->dispatch("/api/settings", HTTP_POST);
  assert(activeServer->code == 400 && nvs.empty());
  activeServer->args["zone"] = "Etc/UTC";
  activeServer->args["brightness"] = "256";
  activeServer->dispatch("/api/settings", HTTP_POST);
  assert(activeServer->code == 400);
  activeServer->args["brightness"] = "75";
  activeServer->dispatch("/api/settings", HTTP_POST);
  assert(activeServer->code == 200 && strcmp(network.settings().location, "Workshop") == 0);
  assert(network.settings().brightness == 75 && network.settings().startClock &&
         network.takeSettingsChanged());
  assert(!network.takeSettingsChanged() && nvs.size() == sizeof(ClockSettings));
  nvsWritable = false;
  activeServer->args["location"] = "Lost write";
  activeServer->dispatch("/api/settings", HTTP_POST);
  assert(activeServer->code == 500 && strcmp(network.settings().location, "Workshop") == 0 &&
         !network.takeSettingsChanged());
  nvsWritable = true;
  activeServer->dispatch("/api/sync", HTTP_POST);
  assert(activeServer->code == 409);
  WiFi.state = WL_CONNECTED;
  network.loop();
  assert(!network.portalActive() && activeServer && ntpRequests == 1);
  assert(!network.timeValid());
  timeval value = {};
  syncCallback(&value);
  network.loop();
  tm local = {};
  assert(network.timeValid() && network.localTime(local));
  if (*SECRET_PANEL_PASSWORD) {
    // With a page password configured, unauthenticated clients are challenged.
    activeServer->authenticated = false;
    activeServer->dispatch("/api/status");
    assert(activeServer->code == 401);
    activeServer->authenticated = true;
  }
  activeServer->dispatch("/api/status");
  std::ofstream(argv[1]) << activeServer->body;
  assert(activeServer->body.find("Workshop") != std::string::npos &&
         activeServer->body.find("Panel-Test-2G") != std::string::npos);
  // Opening setup from the LAN recreates one server, keeping all custom routes.
  activeServer->args = {{"token", csrf.c_str()}};
  activeServer->dispatch("/api/wifi", HTTP_POST);
  assert(activeServer->code == 200);
  network.loop();
  assert(network.portalActive() && activeServer);
  activeServer->dispatch("/api/status");
  assert(activeServer->code == 200);
  WiFi.state = 0;
  network.loop();
  assert(network.localTime(local));
  fakeMillis += WIFI_CONNECT_TIMEOUT_MS;
  network.loop();
  const auto attempts = WiFi.begins;
  fakeMillis += WIFI_RETRY_MS;
  network.loop();
  assert(WiFi.begins == attempts + 1);
  fakeMillis += WIFI_CONNECT_TIMEOUT_MS;
  network.loop();
  fakeMillis += WIFI_RETRY_MS;
  network.loop();
  assert(WiFi.begins == attempts + 2);
  // Holdover ends after the configured period without a fresh sync.
  assert(network.timeValid());
  fakeMillis += CLOCK_HOLDOVER_MAX_MS;
  network.loop();
  assert(!network.timeValid() && !network.localTime(local));
  // Serial brightness persistence writes the record without a settings event.
  assert(network.saveBrightness(120) && network.settings().brightness == 120);
  assert(!network.takeSettingsChanged());
  // Saved settings reload as one validated record; damaged records use defaults.
  PanelNetwork reloaded;
  reloaded.begin();
  assert(reloaded.settings().brightness == 120 && reloaded.settings().startClock);
  nvs[0] = 99;
  PanelNetwork damaged;
  damaged.begin();
  assert(damaged.settings().brightness == DEFAULT_BRIGHTNESS);
  assert(Serial.output.find("PanelSetup32") ==
         std::string::npos); // Never log setup or station passwords.
  std::cout << "PASS: real network application state machine, portal/LAN routes, tokens, "
               "validation, NVS reload/failure, NTP/holdover, reconnects\n";
}
