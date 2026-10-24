// Red LED Panel 8x2 — a looping showcase for a 64x16 HUB08 display.
#include <Arduino.h>
#include <string.h>
#include "Hub08Panel.h"

Hub08Panel panel;

enum class Scene : uint8_t {
  Letters, Checkerboard, Bounce, Stars, Rain, Equalizer, Life, Hearts, Scroll,
  Pong, Clock,
  Count, AllOff, AllOn, Blank
};

struct SceneInfo {
  char key;
  const char* name;
  const char* description;
  uint16_t frames;
  uint32_t intervalMs;
};

static const SceneInfo scenes[] = {
  {'1', "LETTERS", "A-H above I-P, one letter per module", 1, LETTER_HOLD_MS},
  {'2', "CHECKERBOARD", "Alternating pixels and their inverse", 6, CHECKER_STEP_MS},
  {'3', "BOUNCING BALLS", "Three balls with different paths and speeds", 140, ANIMATION_STEP_MS},
  {'4', "STARFIELD", "Stars travelling across the full display", 140, ANIMATION_STEP_MS},
  {'5', "PIXEL RAIN", "Staggered falling trails", 120, RAIN_STEP_MS},
  {'6', "ANIMATED BARS", "A simulated equalizer; no audio input needed", 160, ANIMATION_STEP_MS},
  {'7', "CONWAY'S LIFE", "Gliders and oscillators on a wrapping grid", 72, LIFE_STEP_MS},
  {'8', "BEATING HEARTS", "Three hearts with a gentle brightness pulse", 100, ANIMATION_STEP_MS},
  {'9', "SCROLLING TEXT", SCROLL_TEXT, 0, SCROLL_STEP_MS},
  {'g', "PONG", "Automatic paddles, rebounds, and scoring", 400, PONG_STEP_MS},
  {'c', "CLOCK", "Simulated HH:MM:SS with midnight rollover", 32, CLOCK_STEP_MS},
};
static constexpr uint8_t SCENE_COUNT = static_cast<uint8_t>(Scene::Count);
static_assert(sizeof(scenes) / sizeof(scenes[0]) == SCENE_COUNT, "One description per scene");

static Scene scene = Scene::Letters;
static uint16_t frame = 0;
static uint32_t lastStep = 0;
static uint32_t pausedAt = 0;
static bool paused = false;
static uint8_t brightness = DEFAULT_BRIGHTNESS;
static int16_t textWidth = 0;
static uint8_t life[PANEL_HEIGHT][PANEL_WIDTH / 8];

static constexpr int16_t PONG_SCALE = 4;  // Quarter-pixel ball positions.
static constexpr int16_t PADDLE_HEIGHT = 5;
static constexpr int16_t BALL_SIZE = 2;
struct PongState {
  int16_t x, y;
  int8_t vx, vy;
  int16_t leftY, rightY;
  uint8_t leftScore, rightScore, rally, serveFrames;
};
static PongState pong;

static bool inPlaylist() {
  return static_cast<uint8_t>(scene) < SCENE_COUNT;
}

// Deterministic variation keeps animations reproducible without a random seed.
static uint32_t noise(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352dUL;
  value ^= value >> 15;
  value *= 0x846ca68bUL;
  return value ^ (value >> 16);
}

static int16_t triangle(uint32_t value, int16_t span) {
  const uint32_t phase = value % (2 * span);
  return phase <= static_cast<uint32_t>(span) ? phase : 2 * span - phase;
}

static void drawDigit(int16_t x, int16_t y, uint8_t digit, uint8_t scale) {
  static const uint8_t digits[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7},
    {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7},
    {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7},
    {7, 5, 7, 1, 7}
  };
  for (uint8_t row = 0; row < 5; ++row) {
    for (uint8_t col = 0; col < 3; ++col) {
      if (digits[digit % 10][row] & (4 >> col)) {
        panel.fillRect(x + col * scale, y + row * scale, scale, scale, 1);
      }
    }
  }
}

static void servePong(bool toRight) {
  pong.x = (PANEL_WIDTH / 2 - 1) * PONG_SCALE;
  pong.y = (PANEL_HEIGHT / 2 - 1) * PONG_SCALE;
  pong.vx = toRight ? 5 : -5;
  pong.vy = (pong.leftScore + pong.rightScore) % 2 ? -3 : 3;
  pong.rally = 0;
  pong.serveFrames = 15;
}

static int16_t followBall(int16_t paddleY, bool receiving) {
  int16_t target = pong.y / PONG_SCALE - (PADDLE_HEIGHT - BALL_SIZE) / 2;
  // A deliberate late-rally miss gives the simulation goals and fresh serves.
  if (receiving && pong.rally >= 3) target = target < PANEL_HEIGHT / 2 ? PANEL_HEIGHT : 0;
  if (target < 0) target = 0;
  if (target > PANEL_HEIGHT - PADDLE_HEIGHT) target = PANEL_HEIGHT - PADDLE_HEIGHT;
  return paddleY + (target > paddleY ? 1 : target < paddleY ? -1 : 0);
}

static void pongRebound(int16_t paddleY) {
  pong.vx = -pong.vx;
  const int16_t offset = pong.y / PONG_SCALE + BALL_SIZE / 2 - (paddleY + PADDLE_HEIGHT / 2);
  pong.vy = offset ? offset * 2 : (pong.vy < 0 ? -3 : 3);
  ++pong.rally;
}

static void stepPong() {
  pong.leftY = followBall(pong.leftY, pong.vx < 0);
  pong.rightY = followBall(pong.rightY, pong.vx > 0);
  if (pong.serveFrames) {
    --pong.serveFrames;
    return;
  }
  const int16_t previousX = pong.x;
  pong.x += pong.vx;
  pong.y += pong.vy;
  const int16_t bottom = (PANEL_HEIGHT - BALL_SIZE) * PONG_SCALE;
  if (pong.y < 0) {
    pong.y = -pong.y;
    pong.vy = -pong.vy;
  } else if (pong.y > bottom) {
    pong.y = 2 * bottom - pong.y;
    pong.vy = -pong.vy;
  }
  const int16_t left = 3 * PONG_SCALE;
  const int16_t right = (PANEL_WIDTH - 3 - BALL_SIZE) * PONG_SCALE;
  if (pong.vx < 0 && previousX >= left && pong.x <= left &&
      pong.y + BALL_SIZE * PONG_SCALE > pong.leftY * PONG_SCALE &&
      pong.y < (pong.leftY + PADDLE_HEIGHT) * PONG_SCALE) {
    pong.x = 2 * left - pong.x;
    pongRebound(pong.leftY);
  } else if (pong.vx > 0 && previousX <= right && pong.x >= right &&
             pong.y + BALL_SIZE * PONG_SCALE > pong.rightY * PONG_SCALE &&
             pong.y < (pong.rightY + PADDLE_HEIGHT) * PONG_SCALE) {
    pong.x = 2 * right - pong.x;
    pongRebound(pong.rightY);
  }
  if (pong.x < -BALL_SIZE * PONG_SCALE) {
    pong.rightScore = (pong.rightScore + 1) % 10;
    servePong(false);
  } else if (pong.x >= PANEL_WIDTH * PONG_SCALE) {
    pong.leftScore = (pong.leftScore + 1) % 10;
    servePong(true);
  }
}

static uint32_t clockSeconds() {
  // Two half-second frames per simulated second; no network or RTC required.
  return (CLOCK_START_SECONDS + frame / 2) % 86400UL;
}

static void renderClock() {
  const uint32_t time = clockSeconds();
  const uint8_t hours = time / 3600;
  const uint8_t minutes = (time / 60) % 60;
  const uint8_t seconds = time % 60;
  const uint8_t digits[] = {static_cast<uint8_t>(hours / 10), static_cast<uint8_t>(hours % 10),
                          static_cast<uint8_t>(minutes / 10), static_cast<uint8_t>(minutes % 10),
                          static_cast<uint8_t>(seconds / 10), static_cast<uint8_t>(seconds % 10)};
  const uint8_t positions[] = {5, 13, 25, 33, 45, 53};
  for (uint8_t i = 0; i < 6; ++i) drawDigit(positions[i], 0, digits[i], 2);
  if ((frame & 1) == 0) {
    for (uint8_t x : {21, 41}) {
      panel.fillRect(x, 2, 2, 2, 1);
      panel.fillRect(x, 6, 2, 2, 1);
    }
  }
  // A small SIM label distinguishes the demonstration from a real-time clock.
  const uint8_t label[3][5] = {{7, 4, 7, 1, 7}, {7, 2, 2, 2, 7}, {5, 7, 7, 5, 5}};
  for (uint8_t letter = 0; letter < 3; ++letter) {
    for (uint8_t row = 0; row < 5; ++row) {
      for (uint8_t col = 0; col < 3; ++col) {
        if (label[letter][row] & (4 >> col)) panel.drawPixel(2 + letter * 5 + col, 11 + row, 1);
      }
    }
  }
  panel.drawRect(20, 12, 42, 4, 1);
  panel.fillRect(21, 13, static_cast<uint16_t>(seconds) * 40 / 59, 2, 1);
}

static bool lifeAlive(int16_t x, int16_t y) {
  x = (x + PANEL_WIDTH) % PANEL_WIDTH;
  y = (y + PANEL_HEIGHT) % PANEL_HEIGHT;
  return life[y][x / 8] & (0x80 >> (x & 7));
}

static void seedLife() {
  memset(life, 0, sizeof(life));
  const int8_t glider[][2] = {{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}};
  const int8_t origins[][2] = {{1, 1}, {17, 7}, {35, 2}, {51, 9}};
  for (const auto& origin : origins) {
    for (const auto& cell : glider) {
      const int16_t x = origin[0] + cell[0];
      life[origin[1] + cell[1]][x / 8] |= 0x80 >> (x & 7);
    }
  }
  const int8_t blinkers[][2] = {{10, 4}, {27, 11}, {43, 6}};
  for (const auto& origin : blinkers) {
    for (int16_t x = origin[0]; x < origin[0] + 3; ++x) {
      life[origin[1]][x / 8] |= 0x80 >> (x & 7);
    }
  }
}

static void stepLife() {
  uint8_t next[PANEL_HEIGHT][PANEL_WIDTH / 8] = {};
  for (int16_t y = 0; y < PANEL_HEIGHT; ++y) {
    for (int16_t x = 0; x < PANEL_WIDTH; ++x) {
      uint8_t neighbours = 0;
      for (int8_t dy = -1; dy <= 1; ++dy) {
        for (int8_t dx = -1; dx <= 1; ++dx) {
          if (dx || dy) neighbours += lifeAlive(x + dx, y + dy);
        }
      }
      if (neighbours == 3 || (neighbours == 2 && lifeAlive(x, y))) {
        next[y][x / 8] |= 0x80 >> (x & 7);
      }
    }
  }
  memcpy(life, next, sizeof(life));
}

static void renderFrame() {
  panel.fillScreen(0);
  uint8_t level = brightness;
  switch (scene) {
    case Scene::Letters:
      for (int16_t row = 0; row < PANEL_HEIGHT / 8; ++row) {
        for (int16_t col = 0; col < PANEL_WIDTH / 8; ++col) {
          panel.drawChar(col * 8 + 1, row * 8, 'A' + row * (PANEL_WIDTH / 8) + col, 1, 0, 1);
        }
      }
      break;
    case Scene::Checkerboard:
      for (int16_t y = 0; y < PANEL_HEIGHT; ++y) {
        for (int16_t x = 0; x < PANEL_WIDTH; ++x) {
          if (((x + y + frame) & 1) == 0) panel.drawPixel(x, y, 1);
        }
      }
      break;
    case Scene::Bounce:
      panel.drawRect(0, 0, PANEL_WIDTH, PANEL_HEIGHT, 1);
      for (uint8_t ball = 0; ball < 3; ++ball) {
        const int16_t x = 3 + triangle(frame * (ball + 1) + ball * 19, PANEL_WIDTH - 7);
        const int16_t y = 3 + triangle(frame / (ball + 1) + ball * 5, PANEL_HEIGHT - 7);
        if (ball == 1) panel.drawCircle(x, y, 2, 1);
        else panel.fillCircle(x, y, 2, 1);
      }
      break;
    case Scene::Stars:
      for (uint8_t star = 0; star < 24; ++star) {
        const uint8_t speed = 1 + star % 3;
        const int16_t x = PANEL_WIDTH - 1 - (noise(star + 1) + frame * speed) % PANEL_WIDTH;
        const int16_t y = noise(star + 101) % PANEL_HEIGHT;
        panel.drawPixel(x, y, 1);
        if (speed == 3 && x + 1 < PANEL_WIDTH) panel.drawPixel(x + 1, y, 1);
      }
      break;
    case Scene::Rain:
      for (uint8_t column = 0; column < PANEL_WIDTH / 4; ++column) {
        const int16_t x = column * 4 + noise(column + 11) % 3;
        const int16_t head = (frame * (1 + column % 2) + noise(column + 23)) % (PANEL_HEIGHT + 10);
        const uint8_t length = 3 + column % 5;
        for (uint8_t tail = 0; tail < length; ++tail) {
          const int16_t y = head - tail;
          if (y >= 0 && y < PANEL_HEIGHT && (tail < 2 || (tail + column) % 2)) {
            panel.drawPixel(x, y, 1);
          }
        }
      }
      break;
    case Scene::Equalizer:
      for (uint8_t bar = 0; bar < 8; ++bar) {
        const uint16_t tick = frame / 6;
        const int16_t from = 2 + noise(tick * 8 + bar) % 13;
        const int16_t to = 2 + noise((tick + 1) * 8 + bar) % 13;
        const int16_t height = from + (to - from) * (frame % 6) / 6;
        panel.fillRect(bar * 8 + 1, PANEL_HEIGHT - height, 5, height, 1);
        panel.drawFastHLine(bar * 8 + 1, PANEL_HEIGHT - height - 2, 5, 1);
      }
      break;
    case Scene::Life:
      for (int16_t y = 0; y < PANEL_HEIGHT; ++y) {
        for (int16_t x = 0; x < PANEL_WIDTH; ++x) {
          if (lifeAlive(x, y)) panel.drawPixel(x, y, 1);
        }
      }
      break;
    case Scene::Hearts: {
      const uint8_t heart[] = {0x00, 0x66, 0xff, 0xff, 0x7e, 0x3c, 0x18, 0x00};
      for (uint8_t copy = 0; copy < 3; ++copy) {
        for (uint8_t y = 0; y < 8; ++y) {
          for (uint8_t x = 0; x < 8; ++x) {
            if (heart[y] & (0x80 >> x)) panel.fillRect(2 + copy * 22 + x * 2, y * 2, 2, 2, 1);
          }
        }
      }
      level = static_cast<uint16_t>(brightness) * (5 + triangle(frame, 20)) / 25;
      break;
    }
    case Scene::Scroll:
      panel.setTextSize(2);
      panel.setCursor(PANEL_WIDTH - frame, 1);
      panel.print(SCROLL_TEXT);
      break;
    case Scene::Pong:
      for (int16_t y = 0; y < PANEL_HEIGHT; y += 3) panel.drawPixel(PANEL_WIDTH / 2 - 1, y, 1);
      panel.fillRect(23, 0, 18, 5, 0);
      drawDigit(24, 0, pong.leftScore, 1);
      drawDigit(37, 0, pong.rightScore, 1);
      panel.fillRect(1, pong.leftY, 2, PADDLE_HEIGHT, 1);
      panel.fillRect(PANEL_WIDTH - 3, pong.rightY, 2, PADDLE_HEIGHT, 1);
      panel.fillRect(pong.x / PONG_SCALE, pong.y / PONG_SCALE, BALL_SIZE, BALL_SIZE, 1);
      break;
    case Scene::Clock:
      renderClock();
      break;
    case Scene::AllOn:
      panel.fillScreen(1);
      break;
    case Scene::Blank:
      level = 0;
      break;
    case Scene::AllOff:
    case Scene::Count:
      break;
  }
  panel.setBrightness(level);
  panel.show();
}

static uint16_t frameCount() {
  return scene == Scene::Scroll ? PANEL_WIDTH + textWidth + 1 : scenes[static_cast<uint8_t>(scene)].frames;
}

static void enterScene(Scene next, bool hold = false) {
  scene = next;
  frame = 0;
  paused = hold;
  if (scene == Scene::Life) seedLife();
  if (scene == Scene::Pong) {
    pong = {};
    pong.leftY = pong.rightY = (PANEL_HEIGHT - PADDLE_HEIGHT) / 2;
    servePong(true);
  }
  renderFrame();
  lastStep = millis();
  pausedAt = lastStep;
  if (inPlaylist()) {
    const uint8_t index = static_cast<uint8_t>(scene);
    Serial.printf("\nSCENE %u/%u: %s - %s%s\n", index + 1, SCENE_COUNT,
                  scenes[index].name, scenes[index].description, hold ? " (held)" : "");
  } else {
    const char* name = scene == Scene::AllOn ? "ALL ON" : scene == Scene::Blank ? "DISPLAY BLANKED" : "ALL OFF";
    Serial.printf("\n%s - held; p/r resumes the playlist, n selects the first scene\n", name);
  }
}

static void nextScene() {
  const uint8_t next = inPlaylist() ? (static_cast<uint8_t>(scene) + 1) % SCENE_COUNT : 0;
  enterScene(static_cast<Scene>(next));
}

static void printHelp() {
  Serial.println("\nCommands: 1-9/g/c = select scene, n = next, p = pause/resume, r = restart playlist");
  Serial.println("g = Pong simulation, c = clock simulation (no network or RTC required)");
  Serial.println("t = hold letters, s = scrolling text, f = all off, o = all on, b = blank display");
  Serial.println("+/- = brightness, ?/h = help. Type a key; no Enter needed.");
  for (uint8_t i = 0; i < SCENE_COUNT; ++i) Serial.printf("  %c: %s\n", scenes[i].key, scenes[i].name);
  Serial.printf("Brightness: %u/255 (hearts pulse up to this level)\n", brightness);
}

static void handleSerial() {
  for (uint8_t count = 0; count < 16 && Serial.available(); ++count) {
    char key = Serial.read();
    if (key >= 'A' && key <= 'Z') key += 'a' - 'A';
    bool selected = false;
    for (uint8_t i = 0; i < SCENE_COUNT; ++i) {
      if (key == scenes[i].key) {
        enterScene(static_cast<Scene>(i));
        selected = true;
        break;
      }
    }
    if (selected) continue;
    switch (key) {
      case 'r': case 'R': enterScene(Scene::Letters); break;
      case 'n': case 'N': nextScene(); break;
      case 't': case 'T': enterScene(Scene::Letters, true); break;
      case 's': case 'S': enterScene(Scene::Scroll); break;
      case 'f': case 'F': enterScene(Scene::AllOff, true); break;
      case 'o': case 'O': enterScene(Scene::AllOn, true); break;
      case 'b': case 'B': enterScene(Scene::Blank, true); break;
      case '+': case '=': case '-':
        brightness = key == '-' ? (brightness > 15 ? brightness - 15 : 0) :
                                 (brightness < 240 ? brightness + 15 : 255);
        renderFrame();
        Serial.printf("Brightness: %u/255\n", brightness);
        break;
      case 'p': case 'P':
        if (!inPlaylist()) enterScene(Scene::Letters);
        else if (paused) {
          lastStep += millis() - pausedAt;
          paused = false;
          Serial.println("RESUMED");
        } else {
          pausedAt = millis();
          paused = true;
          Serial.println("PAUSED - refresh continues");
        }
        break;
      case '?': case 'h': case 'H': printHelp(); break;
      default: break;
    }
  }
}

static void updateDemo() {
  if (paused || !inPlaylist()) return;
  const uint32_t now = millis();
  if (now - lastStep < scenes[static_cast<uint8_t>(scene)].intervalMs) return;
  if (++frame >= frameCount()) {
    nextScene();
    return;
  }
  if (scene == Scene::Life) stepLife();
  if (scene == Scene::Pong) stepPong();
  renderFrame();
  lastStep = now;
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\nRed LED Panel 8x2 - %dx%d HUB08 showcase\n", PANEL_WIDTH, PANEL_HEIGHT);
  panel.begin();
  panel.setTextWrap(false);
  panel.setTextColor(1);
  panel.setTextSize(2);
  int16_t x1, y1;
  uint16_t w, h;
  panel.getTextBounds(SCROLL_TEXT, 0, 0, &x1, &y1, &w, &h);
  textWidth = w;
  printHelp();
  enterScene(Scene::Letters);
}

void loop() {
  handleSerial();
  updateDemo();
  delay(1);
}
