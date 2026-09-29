#include "wled.h"

// WLED Network Display native renderer, prototype for standalone v2.0.0-rc8.
// Python sends only low-rate activity/config telemetry. Particle motion is
// generated locally inside WLED's effect scheduler.

namespace NetworkDisplayNative {

static constexpr uint16_t DEFAULT_PORT = 21325;
static constexpr uint8_t PACKET_VERSION = 1;
static constexpr size_t PACKET_SIZE = 41;
static constexpr uint8_t MAX_PARTICLES = 24;

struct Telemetry {
  bool valid = false;
  bool active = false;
  uint16_t sequence = 0;
  float rxActivity = 0.0f;
  float txActivity = 0.0f;
  float rxSpeed = 15.0f;
  float txSpeed = 13.5f;
  float rxSpawn = 3.0f;
  float txSpawn = 2.4f;
  float rxTrail = 8.0f;
  float txTrail = 7.0f;
  uint8_t maxParticles = 24;
  uint16_t smoothingMs = 150;
  uint8_t rxColor[3] = {0, 205, 255};
  uint8_t rxHighlight[3] = {210, 255, 255};
  uint8_t txColor[3] = {0, 255, 100};
  uint8_t txHighlight[3] = {70, 255, 220};
  uint16_t staleMs = 3000;
  uint32_t lastPacketMs = 0;
};

static Telemetry telemetry;

static inline uint16_t readU16(const uint8_t* p) {
  return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
}

static inline float q8_8(uint16_t v) {
  return float(v) / 256.0f;
}

static bool parsePacket(const uint8_t* p, size_t len) {
  if (len != PACKET_SIZE) return false;
  if (p[0] != 'W' || p[1] != 'N' || p[2] != 'D' || p[3] != 'T') return false;
  if (p[4] != PACKET_VERSION) return false;

  size_t o = 5;
  const uint8_t flags = p[o++];
  telemetry.sequence = readU16(p + o); o += 2;
  telemetry.rxActivity = float(readU16(p + o)) / 65535.0f; o += 2;
  telemetry.txActivity = float(readU16(p + o)) / 65535.0f; o += 2;
  telemetry.rxSpeed = q8_8(readU16(p + o)); o += 2;
  telemetry.txSpeed = q8_8(readU16(p + o)); o += 2;
  telemetry.rxSpawn = q8_8(readU16(p + o)); o += 2;
  telemetry.txSpawn = q8_8(readU16(p + o)); o += 2;
  telemetry.rxTrail = q8_8(readU16(p + o)); o += 2;
  telemetry.txTrail = q8_8(readU16(p + o)); o += 2;
  uint8_t requestedParticles = p[o++];
  if (requestedParticles < 1) requestedParticles = 1;
  telemetry.maxParticles = requestedParticles > MAX_PARTICLES ? MAX_PARTICLES : requestedParticles;
  telemetry.smoothingMs = readU16(p + o); o += 2;

  memcpy(telemetry.rxColor, p + o, 3); o += 3;
  memcpy(telemetry.rxHighlight, p + o, 3); o += 3;
  memcpy(telemetry.txColor, p + o, 3); o += 3;
  memcpy(telemetry.txHighlight, p + o, 3); o += 3;
  telemetry.staleMs = readU16(p + o); o += 2;
  if (telemetry.staleMs < 100) telemetry.staleMs = 100;

  telemetry.active = (flags & 0x01) != 0;
  telemetry.lastPacketMs = millis();
  telemetry.valid = true;
  return o == PACKET_SIZE;
}

struct Particle {
  float pos;
  float speedMul;
  float trailMul;
  float strengthMul;
  float accent;
  bool active;
};

struct SegmentState {
  uint32_t magic;
  uint32_t lastMs;
  float rxActivity;
  float txActivity;
  float rxSpawnAccumulator;
  float txSpawnAccumulator;
  Particle rx[MAX_PARTICLES];
  Particle tx[MAX_PARTICLES];
};

static constexpr uint32_t STATE_MAGIC = 0x574E4454;

static void initState(SegmentState* state) {
  memset(state, 0, sizeof(SegmentState));
  state->magic = STATE_MAGIC;
  state->lastMs = strip.now;
}

static uint32_t blendColor(const uint8_t base[3], const uint8_t hi[3], float t, float level) {
  t = constrain(t, 0.0f, 1.0f);
  level = constrain(level, 0.0f, 1.0f);
  uint8_t r = uint8_t(constrain((base[0] + (hi[0] - base[0]) * t) * level, 0.0f, 255.0f));
  uint8_t g = uint8_t(constrain((base[1] + (hi[1] - base[1]) * t) * level, 0.0f, 255.0f));
  uint8_t b = uint8_t(constrain((base[2] + (hi[2] - base[2]) * t) * level, 0.0f, 255.0f));
  return RGBW32(r, g, b, 0);
}

static void addPixel(uint16_t index, uint32_t add) {
  if (index >= SEGLEN) return;
  uint32_t old = SEGMENT.getPixelColor(index);
  uint16_t r = ((old >> 16) & 0xFF) + ((add >> 16) & 0xFF);
  uint16_t g = ((old >> 8) & 0xFF) + ((add >> 8) & 0xFF);
  uint16_t b = (old & 0xFF) + (add & 0xFF);
  SEGMENT.setPixelColor(index, RGBW32(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b, 0));
}

static void spawnParticle(Particle* particles, uint8_t limit, int direction, uint16_t length,
                          float activity, float& accumulator, float maxRate, float dt) {
  if (activity <= 0.0f || length == 0) return;
  float rate = 0.12f + activity * max(0.0f, maxRate - 0.12f);
  accumulator += rate * dt;
  while (accumulator >= 1.0f) {
    accumulator -= 1.0f;
    for (uint8_t i = 0; i < limit; i++) {
      if (!particles[i].active) {
        particles[i].active = true;
        particles[i].pos = direction > 0 ? 0.0f : float(length - 1);
        particles[i].speedMul = 0.90f + (float(hw_random8()) / 255.0f) * 0.20f;
        particles[i].trailMul = 0.90f + (float(hw_random8()) / 255.0f) * 0.20f;
        particles[i].strengthMul = 0.92f + (float(hw_random8()) / 255.0f) * 0.08f;
        particles[i].accent = float(hw_random8()) / 255.0f;
        break;
      }
    }
  }
}

static void updateDirection(Particle* particles, uint8_t limit, int direction, uint16_t length,
                            float activity, float speed, float trail,
                            const uint8_t base[3], const uint8_t highlight[3],
                            float dt, float& spawnAccumulator, float maxRate) {
  spawnParticle(particles, limit, direction, length, activity, spawnAccumulator, maxRate, dt);

  const float trailScale = 0.75f + 0.70f * activity;
  const float strength = 0.55f + 0.45f * activity;

  for (uint8_t p = 0; p < limit; p++) {
    Particle& particle = particles[p];
    if (!particle.active) continue;

    particle.pos += float(direction) * speed * particle.speedMul * dt;
    const float particleTrail = max(1.0f, trail * trailScale * particle.trailMul);
    const float trailDivisor = max(1.0f, particleTrail / 2.6f);
    const float particleStrength = strength * particle.strengthMul;
    const float accent = 0.45f + 0.35f * particle.accent;
    const int maxDistance = int(ceilf(particleTrail));

    for (int distance = 0; distance <= maxDistance; distance++) {
      float x = particle.pos - float(direction * distance);
      int i0 = int(floorf(x));
      float frac = x - floorf(x);
      float decay = expf(-float(distance) / trailDivisor);
      float head = expf(-float(distance) / 1.15f) * accent;
      float level = decay * particleStrength;
      uint32_t c0 = blendColor(base, highlight, head, level * (1.0f - frac));
      uint32_t c1 = blendColor(base, highlight, head, level * frac);
      if (i0 >= 0 && i0 < int(length)) addPixel(uint16_t(i0), c0);
      if ((i0 + 1) >= 0 && (i0 + 1) < int(length)) addPixel(uint16_t(i0 + 1), c1);
    }

    if (particle.pos <= -particleTrail - 1.0f || particle.pos >= float(length) + particleTrail + 1.0f) {
      particle.active = false;
    }
  }
}

static void mode_network_traffic_native(void) {
  if (SEGLEN < 1) return;
  if (!SEGENV.allocateData(sizeof(SegmentState))) {
    SEGMENT.fill(BLACK);
    return;
  }
  SegmentState* state = reinterpret_cast<SegmentState*>(SEGENV.data);
  if (SEGENV.call == 0 || state->magic != STATE_MAGIC) initState(state);

  uint32_t now = strip.now;
  float dt = float(now - state->lastMs) / 1000.0f;
  state->lastMs = now;
  if (dt <= 0.0f || dt > 0.25f) dt = float(FRAMETIME) / 1000.0f;

  bool fresh = telemetry.valid && telemetry.active && (millis() - telemetry.lastPacketMs <= telemetry.staleMs);
  float targetRx = fresh ? telemetry.rxActivity : 0.0f;
  float targetTx = fresh ? telemetry.txActivity : 0.0f;

  float alpha = 1.0f;
  if (telemetry.smoothingMs > 0) {
    alpha = 1.0f - expf(-(dt * 1000.0f) / float(telemetry.smoothingMs));
  }
  state->rxActivity += (targetRx - state->rxActivity) * alpha;
  state->txActivity += (targetTx - state->txActivity) * alpha;

  SEGMENT.fill(BLACK);
  uint8_t role = SEGMENT.custom1;
  uint8_t limit = telemetry.maxParticles > MAX_PARTICLES ? MAX_PARTICLES : telemetry.maxParticles;

  if (role == 0 || role == 1) {
    updateDirection(state->rx, limit, +1, SEGLEN, state->rxActivity,
                    telemetry.rxSpeed, telemetry.rxTrail,
                    telemetry.rxColor, telemetry.rxHighlight,
                    dt, state->rxSpawnAccumulator, telemetry.rxSpawn);
  }
  if (role == 0 || role == 2) {
    int direction = (role == 0) ? -1 : +1;
    updateDirection(state->tx, limit, direction, SEGLEN, state->txActivity,
                    telemetry.txSpeed, telemetry.txTrail,
                    telemetry.txColor, telemetry.txHighlight,
                    dt, state->txSpawnAccumulator, telemetry.txSpawn);
  }
}

static const char _data_FX_NETWORK_TRAFFIC_NATIVE[] PROGMEM =
  "Network Traffic Native@,,,Role;;;;1;c1=0";

class NetworkDisplayUsermod : public Usermod {
private:
  WiFiUDP udp;
  uint16_t port = DEFAULT_PORT;
  bool udpStarted = false;

  void startUdp() {
    if (udpStarted) udp.stop();
    udpStarted = udp.begin(port);
  }

public:
  void setup() override {
    strip.addEffect(255, &mode_network_traffic_native, _data_FX_NETWORK_TRAFFIC_NATIVE);
  }

  void connected() override {
    startUdp();
  }

  void loop() override {
    if (!WLED_CONNECTED) return;
    if (!udpStarted) startUdp();

    int packetSize = udp.parsePacket();
    while (packetSize > 0) {
      uint8_t buffer[PACKET_SIZE];
      int readLen = 0;
      if (packetSize == int(PACKET_SIZE)) {
        readLen = udp.read(buffer, PACKET_SIZE);
        if (readLen == int(PACKET_SIZE)) parsePacket(buffer, PACKET_SIZE);
      } else {
        while (udp.available()) udp.read();
      }
      packetSize = udp.parsePacket();
    }
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");
    JsonArray info = user.createNestedArray("Network Display");
    info.add(telemetry.valid ? "telemetry received" : "waiting for telemetry");
    info.add(port);
  }

  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject("NetworkDisplay");
    top["port"] = port;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root["NetworkDisplay"];
    if (top.isNull()) return false;
    uint16_t newPort = top["port"] | DEFAULT_PORT;
    if (newPort > 0) port = newPort;
    return true;
  }
};

}

static NetworkDisplayNative::NetworkDisplayUsermod network_display_usermod;
REGISTER_USERMOD(network_display_usermod);
