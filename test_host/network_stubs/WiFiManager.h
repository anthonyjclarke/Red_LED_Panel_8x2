#pragma once
#include "Arduino.h"
constexpr int WL_CONNECTED = 3;
constexpr int WIFI_STA = 1;
constexpr int HTTP_GET = 1;
constexpr int HTTP_POST = 2;
struct IPAddress {
  const char* text;
  String toString() const { return text; }
};
struct WiFiPort {
  int state = 0;
  unsigned begins = 0;
  bool ap = false;
  int status() const { return state; }
  String SSID() const { return "Panel-Test-2G"; }
  IPAddress localIP() const { return {"192.168.1.80"}; }
  IPAddress softAPIP() const { return {ap ? "192.168.4.1" : "0.0.0.0"}; }
  int RSSI() const { return -52; }
  void mode(int) {}
  void setHostname(const char*) {}
  void setAutoReconnect(bool) {}
  void begin() { ++begins; }
  bool reconnect() {
    ++begins;
    return true;
  }
  int softAPgetStationNum() const { return 0; }
};
extern WiFiPort WiFi;
class WebServer;
extern WebServer* activeServer;
class WebServer {
public:
  std::map<std::string, std::function<void()>> routes;
  std::map<std::string, String> args;
  int code = 0;
  std::string body;
  WebServer() { activeServer = this; }
  ~WebServer() {
    if (activeServer == this) activeServer = nullptr;
  }
  void on(const char* path, int method, std::function<void()> callback) {
    routes[std::to_string(method) + path] = callback;
  }
  void sendHeader(const char*, const char*) {}
  void send(int status, const char*, const String& text) {
    code = status;
    body = text.c_str();
  }
  void send_P(int status, const char*, const char* text) {
    code = status;
    body = text;
  }
  String arg(const char* key) { return args[key]; }
  bool authenticated = true; // Credentials the fixture client presents are correct.
  bool authenticate(const char*, const char*) { return authenticated; }
  void requestAuthentication() { code = 401; }
  void dispatch(const char* path, int method = HTTP_GET) {
    routes.at(std::to_string(method) + path)();
  }
};
class WiFiManager {
public:
  std::unique_ptr<WebServer> server;
  bool config = false, web = false;
  std::function<void()> serverCallback;
  std::function<void(WiFiManager*)> apCallback;
  void setDebugOutput(bool) {}
  void setConfigPortalBlocking(bool) {}
  void setConfigPortalTimeout(unsigned long) {}
  void setConnectTimeout(unsigned long) {}
  void setSaveConnectTimeout(unsigned long) {}
  void setWiFiAutoReconnect(bool) {}
  void setTitle(const char*) {}
  void setMenu(std::vector<const char*>&) {}
  void setCustomMenuHTML(const char*) {}
  void setShowInfoErase(bool) {}
  void setHostname(const char*) {}
  void setWebServerCallback(std::function<void()> cb) { serverCallback = cb; }
  void setAPCallback(std::function<void(WiFiManager*)> cb) { apCallback = cb; }
  bool getConfigPortalActive() { return config; }
  bool getWebPortalActive() { return web; }
  void process() {}
  void startConfigPortal(const char*, const char*) {
    config = true;
    WiFi.ap = true;
    server.reset(new WebServer);
    serverCallback();
    apCallback(this);
  }
  void stopConfigPortal() {
    config = false;
    WiFi.ap = false;
    server.reset();
  }
  void startWebPortal() {
    web = true;
    server.reset(new WebServer);
    serverCallback();
  }
  void stopWebPortal() {
    web = false;
    server.reset();
  }
};
