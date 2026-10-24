#include <cassert>
#include <fstream>
#include <set>
#include <iostream>
#include "app_under_test.cpp"
#include <iomanip>
uint32_t fakeMillis = 0;
SerialPort Serial;
time_t fixtureEpoch = 1792846800;
static std::ofstream captures;
static bool firstCapture = true;
static void key(const char* keys) {
  Serial.input = keys;
  handleSerial();
}
static unsigned lit() {
  unsigned n = 0;
  for (int y = 0; y < PANEL_HEIGHT; ++y)
    for (int x = 0; x < PANEL_WIDTH; ++x)
      n += panel.getPixel(x, y);
  return n;
}
static std::string pixels() {
  return std::string(reinterpret_cast<char*>(panel.getBuffer()), 128);
}
static void capture(const char* name, char key, uint32_t interval, bool first) {
  if (first) {
    if (!firstCapture) captures << ",";
    firstCapture = false;
    captures << "{\"name\":\"" << name << "\",\"key\":\"" << key << "\",\"interval\":" << interval
             << ",\"frames\":[";
  } else
    captures << ",";
  captures << "{\"brightness\":" << unsigned(panel.level) << ",\"bitmap\":\"";
  for (int i = 0; i < 128; ++i)
    captures << std::hex << std::setfill('0') << std::setw(2) << unsigned(panel.getBuffer()[i]);
  captures << std::dec << "\"}";
}
static time_t utc(int year, int month, int day, int hour, int minute, int second) {
  tm value = {};
  value.tm_year = year - 1900;
  value.tm_mon = month - 1;
  value.tm_mday = day;
  value.tm_hour = hour;
  value.tm_min = minute;
  value.tm_sec = second;
  return timegm(&value);
}
int main(int argc, char** argv) {
  assert(argc == 2);
  captures.open(argv[1]);
  captures << "[";
  setenv("TZ", findTimeZone("Australia/Sydney")->rule, 1);
  tzset();
  setup();
  for (int i = 0; i < 3; ++i) {
    renderBootCard(i);
    capture((std::string("Boot ") + std::to_string(i + 1)).c_str(), '!', 250, true);
    captures << "]}";
  }
  fakeMillis = 3 * BOOT_CARD_MS;
  loop();
  assert(bootFinished);
  assert(scene == Scene::Clock && lit() > 0 && !paused);
  // Clock leads the cycle. Every demo is followed by a full ten-second visit,
  // then the next demo; the cursor wraps after Pong without skipping letters.
  for (unsigned demo = 0; demo < static_cast<unsigned>(Scene::Clock) * 2; ++demo) {
    assert(scene == Scene::Clock);
    fakeMillis = sceneStarted + CLOCK_HOLD_MS - 1;
    updateDemo();
    assert(scene == Scene::Clock);
    ++fakeMillis;
    updateDemo();
    assert(static_cast<unsigned>(scene) == demo % static_cast<unsigned>(Scene::Clock));
    const auto duration = frameCount();
    const auto interval = scenes[static_cast<unsigned>(scene)].intervalMs;
    for (unsigned i = 0; i < duration; ++i) {
      fakeMillis += interval;
      updateDemo();
    }
    assert(scene == Scene::Clock);
  }
  key("r");
  assert(scene == Scene::Clock && nextDemo == 0);
  fakeMillis += 2000;
  key("p");
  fakeMillis += 60000;
  updateDemo();
  assert(scene == Scene::Clock && paused);
  key("p");
  fakeMillis += 7999;
  updateDemo();
  assert(scene == Scene::Clock);
  ++fakeMillis;
  updateDemo();
  assert(scene == Scene::Letters);
  fakeMillis = UINT32_MAX - 5000;
  key("r");
  fakeMillis += CLOCK_HOLD_MS + 800; // Delayed main loop and millis rollover.
  updateDemo();
  assert(scene == Scene::Letters);
  for (int index = 0; index < SCENE_COUNT; ++index) {
    char command[2] = {scenes[index].key, 0};
    key(command);
    assert(static_cast<int>(scene) == index);
    if (scene == Scene::Clock) {
      assert(paused);
      key("p");
    } else
      assert(!paused);
    std::set<std::string> patterns;
    bool hasPixels = false;
    const auto frames = frameCount();
    for (unsigned i = 0; i < frames; ++i) {
      const unsigned stride = std::max(1u, unsigned(frames) / 48);
      if (i % stride == 0)
        capture(scenes[index].name, scenes[index].key, scenes[index].intervalMs * stride, i == 0);
      patterns.insert(pixels());
      hasPixels |= lit() > 0;
      assert(panel.level <= brightness);
      fakeMillis += scenes[index].intervalMs;
      updateDemo();
    }
    captures << "]}";
    assert(hasPixels);
    if (index > 0 && index != 7) assert(patterns.size() > 1);
    assert(scene == (index == static_cast<int>(Scene::Clock) ? Scene::Letters : Scene::Clock));
    std::cout << scenes[index].name << ": " << frames << " frames, " << patterns.size()
              << " pixel patterns\n";
  }
  // Checkerboard phases are complements, not merely changed images.
  key("2");
  const auto first = pixels();
  fakeMillis += CHECKER_STEP_MS;
  updateDemo();
  const auto second = pixels();
  assert(lit() == 512);
  for (unsigned i = 0; i < 128; ++i)
    assert((uint8_t(first[i]) ^ uint8_t(second[i])) == 255);
  // A blinker straddling both edges verifies real Life rules and wrapping.
  memset(life, 0, sizeof(life));
  life[0][0] = 0xC0;
  life[0][7] = 0x01;
  stepLife();
  assert(lifeAlive(0, 15) && lifeAlive(0, 0) && lifeAlive(0, 1));
  assert(!lifeAlive(63, 0) && !lifeAlive(1, 0));
  // Pausing holds the frame and preserves the remaining interval, including rollover.
  key("3");
  fakeMillis += 10;
  key("p");
  auto shown = panel.shows;
  fakeMillis += 10000;
  updateDemo();
  assert(panel.shows == shown);
  key("p");
  fakeMillis += ANIMATION_STEP_MS - 11;
  updateDemo();
  assert(panel.shows == shown);
  ++fakeMillis;
  updateDemo();
  assert(panel.shows == shown + 1);
  fakeMillis = UINT32_MAX - 20;
  key("4");
  fakeMillis += ANIMATION_STEP_MS;
  updateDemo();
  assert(frame == 1);
  // Held controls, brightness bounds, blanking, and resumption.
  key("f");
  capture("All pixels off", 'f', 1000, true);
  captures << "]}";
  assert(paused && lit() == 0);
  fakeMillis += 100000;
  updateDemo();
  assert(scene == Scene::AllOff);
  key("o");
  capture("All pixels on", 'o', 1000, true);
  captures << "]}";
  assert(lit() == 1024 && panel.level == brightness);
  for (int i = 0; i < 30; ++i)
    key("+");
  assert(brightness == 255 && panel.level == 255);
  for (int i = 0; i < 30; ++i)
    key("-");
  assert(brightness == 0 && panel.level == 0);
  key("+");
  key("b");
  capture("Output blanked", 'b', 1000, true);
  captures << "]}";
  key("+");
  assert(panel.level == 0 && brightness == 30);
  key("p");
  assert(scene == Scene::Clock && panel.level == 30 && !paused);
  key("t");
  assert(paused && scene == Scene::Letters);
  key("p");
  assert(!paused);
  key("s");
  assert(scene == Scene::Scroll && !paused);
  key("n");
  assert(scene == Scene::Clock);
  key("n");
  assert(scene == Scene::Pong);
  key("8");
  bool low = false, high = false;
  for (unsigned i = 0; i < 40; ++i) {
    low |= panel.level == brightness / 5;
    high |= panel.level == brightness;
    fakeMillis += ANIMATION_STEP_MS;
    updateDemo();
  }
  assert(low && high);
  // Pong: both paddle collisions, wall reflection, goals, serves and pause.
  key("G");
  assert(scene == Scene::Pong && pong.leftScore == 0 && pong.rightScore == 0);
  pong.serveFrames = 0;
  pong.x = 3 * PONG_SCALE;
  pong.y = 5 * PONG_SCALE;
  pong.vx = -5;
  pong.vy = 0;
  pong.leftY = 5;
  stepPong();
  assert(pong.vx > 0 && pong.rally == 1);
  pong.x = (PANEL_WIDTH - 3 - BALL_SIZE) * PONG_SCALE;
  pong.y = 5 * PONG_SCALE;
  pong.vx = 5;
  pong.vy = 0;
  pong.rightY = 5;
  stepPong();
  assert(pong.vx < 0 && pong.rally == 2);
  pong.x = 31 * PONG_SCALE;
  pong.y = 0;
  pong.vy = -3;
  stepPong();
  assert(pong.y == 3 && pong.vy == 3);
  pong.x = -BALL_SIZE * PONG_SCALE;
  pong.vx = -5;
  stepPong();
  assert(pong.rightScore == 1 && pong.serveFrames == 15 && pong.x == 31 * PONG_SCALE);
  pong.serveFrames = 0;
  pong.x = PANEL_WIDTH * PONG_SCALE - 1;
  pong.vx = 5;
  stepPong();
  assert(pong.leftScore == 1 && pong.serveFrames == 15);
  key("g");
  for (int i = 0; i < 399; ++i) {
    fakeMillis += PONG_STEP_MS;
    updateDemo();
    assert(pong.y >= 0 && pong.y <= (PANEL_HEIGHT - BALL_SIZE) * PONG_SCALE);
    assert(pong.leftY >= 0 && pong.leftY <= PANEL_HEIGHT - PADDLE_HEIGHT);
    assert(pong.rightY >= 0 && pong.rightY <= PANEL_HEIGHT - PADDLE_HEIGHT);
  }
  assert(pong.leftScore + pong.rightScore > 0);
  const auto savedPong = pong;
  key("p");
  fakeMillis += 5000;
  updateDemo();
  key("+");
  assert(memcmp(&savedPong, &pong, sizeof(pong)) == 0 && paused);
  // Network-clock rendering is based on time, not animation frames or scene entry.
  fakeMillis = 0;
  fixtureEpoch = utc(2026, 10, 24, 12, 59, 59);
  key("C");
  assert(scene == Scene::Clock && paused);
  tm local = {};
  assert(network.localTime(local) && local.tm_hour == 23 && local.tm_min == 59 &&
         local.tm_sec == 59);
  const auto beforeMidnight = pixels();
  fakeMillis += 1000;
  updateDemo();
  assert(network.localTime(local) && local.tm_hour == 0 && local.tm_min == 0 && local.tm_sec == 0);
  assert(pixels() != beforeMidnight && paused && frame == 0);
  key("c");
  assert(network.localTime(local) && local.tm_hour == 0); // Selecting c must not reset time.
  fakeMillis += 500;
  updateDemo();
  const auto frozenPixels = pixels();
  fakeMillis += 1000;
  updateDemo();
  assert(pixels() != frozenPixels && paused); // Clock keeps running while playlist held.
  network.online = false;
  fakeMillis += 1000;
  updateDemo();
  assert(network.localTime(local));
  capture("Clock offline holdover", 'c', 250, true);
  captures << "]}";
  network.synced = false;
  renderFrame();
  const auto noWifi = pixels();
  capture("Clock waiting for Wi-Fi", 'c', 250, true);
  captures << "]}";
  network.online = true;
  renderFrame();
  assert(pixels() != noWifi);
  capture("Clock waiting for NTP", 'c', 250, true);
  captures << "]}";
  network.synced = true;
  // Settings applied by the network loop must select persisted view/brightness.
  network.values.brightness = 45;
  network.values.startClock = 1;
  network.changed = true;
  loop();
  assert(scene == Scene::Clock && paused && brightness == 45);
  // A brightness-only save applies live and keeps the current scene.
  key("3");
  network.values.brightness = 60;
  network.changed = true;
  loop();
  assert(scene == Scene::Bounce && brightness == 60 && panel.level == 60);
  // Serial brightness is saved once, after a quiet period, as the startup level.
  const auto saves = network.brightnessSaves;
  key("+");
  key("+");
  fakeMillis += BRIGHTNESS_SAVE_DELAY_MS - 1;
  loop();
  assert(network.brightnessSaves == saves);
  ++fakeMillis;
  loop();
  assert(network.brightnessSaves == saves + 1 && network.values.brightness == 90);
  loop();
  assert(network.brightnessSaves == saves + 1);
  key("w");
  assert(network.portal);
  // Time zone conversion including both Sydney daylight-saving boundaries.
  const auto* syd = findTimeZone("Australia/Sydney");
  setenv("TZ", syd->rule, 1);
  tzset();
  time_t t = utc(2026, 1, 15, 12, 0, 0);
  localtime_r(&t, &local);
  assert(local.tm_hour == 23 && local.tm_isdst > 0);
  t = utc(2026, 7, 15, 12, 0, 0);
  localtime_r(&t, &local);
  assert(local.tm_hour == 22 && local.tm_isdst == 0);
  t = utc(2026, 10, 3, 15, 59, 59);
  localtime_r(&t, &local);
  assert(local.tm_hour == 1 && local.tm_min == 59);
  ++t;
  localtime_r(&t, &local);
  assert(local.tm_hour == 3 && local.tm_min == 0);
  t = utc(2026, 4, 4, 15, 59, 59);
  localtime_r(&t, &local);
  assert(local.tm_hour == 2 && local.tm_min == 59 && local.tm_isdst > 0);
  ++t;
  localtime_r(&t, &local);
  assert(local.tm_hour == 2 && local.tm_min == 0 && local.tm_isdst == 0);
  auto settings = defaultClockSettings(90);
  assert(validClockSettings(settings));
  memset(settings.location, 'X', sizeof(settings.location));
  assert(!validClockSettings(settings));
  settings = defaultClockSettings(90);
  strcpy(settings.zone, "Unknown/Place");
  assert(!validClockSettings(settings));
  assert(!validLocation(" ") && !validLocation("bad\nlocation") && validLocation("Workshop"));
  uint8_t level = 0;
  assert(parseBrightness("255", level) && level == 255);
  assert(parseBrightness("0", level) && level == 0);
  assert(!parseBrightness("-1", level) && !parseBrightness("256", level) &&
         !parseBrightness("99999999999999999999", level));
  // OTA progress replaces the picture; a failed upload restores the scene.
  key("t");
  const auto letters = pixels();
  network.otaProgress(40);
  assert(pixels() != letters && panel.level == brightness);
  network.otaProgress(-1);
  assert(pixels() == letters);
  // Return to a normal frame before testing unchanged help/unknown commands.
  key("3");
  // Help and retired/unknown keys must not alter a paused picture.
  key("p");
  const auto before = pixels();
  shown = panel.shows;
  key("?hel\r\n0");
  assert(pixels() == before && shown == panel.shows && paused);
  captures << "]";
  captures.close();
  std::cout << "PASS: playlist, movement, Life rules/wrapping, checkerboard, pause/rollover, "
               "holds, brightness, serial commands, Pong collisions/goals/AI, live clock, offline "
               "states, persisted defaults, DST boundaries, input validation\n";
}
