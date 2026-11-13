#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <Wire.h>
#include <esp_system.h>

#include "edition.h"

namespace {

namespace pins {
// Rev A IPC-D-356 netlist and ESP32-C3-MINI-1 schematic symbol.
constexpr uint8_t sda = 0;       // U1 pad 12, I2C_SDA
constexpr uint8_t scl = 1;       // U1 pad 13, I2C_SCL
constexpr uint8_t feed = 4;      // U1 pad 18, BTN_FEED
constexpr uint8_t pet = 5;       // U1 pad 19, BTN_PET
constexpr uint8_t battle = 6;    // U1 pad 20, BTN_BATTLE
constexpr uint8_t select = 7;    // U1 pad 21, BTN_SCROLL_SELECT
constexpr uint8_t audio = 10;    // U1 pad 16, AUDIO_PWM
constexpr uint8_t irTx = 3;      // U1 pad 6, IR_TX
constexpr uint8_t irRx = 20;     // U1 pad 30, IR_RX
}

constexpr uint32_t debounceMs = 25;
constexpr uint32_t sleepMs = 60000;
constexpr uint32_t holdMs = 650;
constexpr uint32_t chargeMs = 3000;
constexpr uint32_t linkTimeoutMs = 9000;
constexpr uint8_t motionAddress = 0x18;
constexpr uint8_t audioChannel = 0;
constexpr uint8_t maxLevel = 50;

enum State : uint8_t {
    STATE_IDLE, STATE_SLEEP, STATE_FEED, STATE_PET, STATE_STATS,
    STATE_CHARGE, STATE_IR_LINK, STATE_BATTLE, STATE_RESULT
};

struct Button {
    explicit Button(uint8_t pin) : pin(pin) {}

    void begin() const { pinMode(pin, INPUT_PULLUP); }

    void update(uint32_t now) {
        pressed = released = false;
        const bool raw = digitalRead(pin) == LOW;
        if (raw != sampled) {
            sampled = raw;
            changedAt = now;
        }
        if (down != sampled && now - changedAt >= debounceMs) {
            down = sampled;
            pressed = down;
            released = !down;
            if (pressed) pressedAt = now;
        }
    }

    const uint8_t pin;
    bool pressed = false;
    bool released = false;
    bool down = false;
    uint32_t pressedAt = 0;

private:
    bool sampled = false;
    uint32_t changedAt = 0;
};

struct Progress {
    uint8_t version = 1;
    uint8_t level = 1;
    uint8_t exp = 0;
    uint8_t hp = 50;
    uint8_t hunger = 65;
    uint8_t happiness = 60;
    uint16_t wins = 0;
};
static_assert(sizeof(Progress) == 8, "Unexpected save layout");

uint8_t maxHp(uint8_t level) { return 48 + level * 2; }
uint8_t attackPower(uint8_t level) { return 16 + level * 2; }
uint8_t speed(uint8_t level) { return 20 + level; }

class Save {
public:
    Progress pet;

    void begin() {
        ready = nvs.begin("dsh-dex", false);
        if (!ready) return;
        Progress previous;
        if (nvs.getBytesLength(edition::profile.saveKey) == sizeof(previous) &&
            nvs.getBytes(edition::profile.saveKey, &previous, sizeof(previous)) == sizeof(previous) &&
            previous.version == 1 && previous.level >= 1 && previous.level <= maxLevel &&
            previous.exp < 100 && previous.hp <= maxHp(previous.level) &&
            previous.hunger <= 100 && previous.happiness <= 100) {
            pet = previous;
        } else if (edition::wireId == infrared::garchomp && nvs.isKey("pet-v1")) {
            const uint32_t legacy = nvs.getUInt("pet-v1", UINT32_MAX);
            if (uint8_t(legacy) <= 100 && uint8_t(legacy >> 8) <= 100) {
                pet.hunger = uint8_t(legacy);
                pet.happiness = uint8_t(legacy >> 8);
                pet.wins = uint8_t(legacy >> 24);
                dirty = true;
            }
        }
        if (!dirty) saved = pet;
    }

    void changed() { dirty = true; }

    void flush(uint32_t now) {
        if (!ready || !dirty || now - lastAttempt < 500) return;
        lastAttempt = now;
        if (memcmp(&pet, &saved, sizeof(pet)) == 0 ||
            nvs.putBytes(edition::profile.saveKey, &pet, sizeof(pet)) == sizeof(pet)) {
            saved = pet;
            dirty = false;
        }
    }

private:
    Preferences nvs;
    Progress saved;
    bool ready = false;
    bool dirty = false;
    uint32_t lastAttempt = 0;
};

struct Note { uint16_t hz; uint16_t ms; };
constexpr Note eat[] = {{622, 65}, {830, 65}, {988, 80}, {1245, 110}};
constexpr Note cry[] = {{247, 60}, {330, 55}, {415, 65}, {294, 80}, {174, 140}};
constexpr Note victory[] = {{523, 75}, {659, 75}, {784, 75}, {1047, 170}};
constexpr Note defeat[] = {{440, 105}, {294, 110}, {196, 160}};
constexpr Note hit[] = {{220, 60}, {130, 65}};

class Audio {
public:
    void begin() {
        ready = ledcSetup(audioChannel, 1000, 8) != 0;
        if (ready) ledcAttachPin(pins::audio, audioChannel);
    }

    template <size_t N>
    void play(const Note (&song)[N], uint32_t now) {
        if (!ready) return;
        notes = song;
        count = N;
        index = 0;
        started = now;
        ledcWriteTone(audioChannel, notes[0].hz);
    }

    void tick(uint32_t now) {
        if (!notes || now - started < notes[index].ms) return;
        started = now;
        if (++index == count) {
            notes = nullptr;
            ledcWriteTone(audioChannel, 0);
        } else {
            ledcWriteTone(audioChannel, notes[index].hz);
        }
    }

private:
    const Note *notes = nullptr;
    uint32_t started = 0;
    size_t count = 0;
    size_t index = 0;
    bool ready = false;
};

bool readAccel(uint8_t reg, uint8_t *data, size_t count) {
    Wire.beginTransmission(motionAddress);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(motionAddress, count, true) != count) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) data[i] = static_cast<uint8_t>(Wire.read());
    return true;
}

bool writeAccel(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(motionAddress);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

class Motion {
public:
    void begin() {
        uint8_t who = 0;
        ready = readAccel(0x0f, &who, 1) && who == 0x33 &&
                writeAccel(0x23, 0x88) && writeAccel(0x20, 0x47);
    }

    void tick(uint32_t now) {
        jerk = 0;
        if (!ready || now - sampledAt < 20) return;
        sampledAt = now;

        uint8_t bytes[6];
        if (!readAccel(0x28 | 0x80, bytes, sizeof(bytes))) {
            sampled = false;
            return;
        }
        int delta = 0;
        for (uint8_t i = 0; i < 3; ++i) {
            const uint16_t raw = uint16_t(bytes[2 * i]) | (uint16_t(bytes[2 * i + 1]) << 8);
            const int sample = static_cast<int16_t>(raw) / 16;
            delta += abs(sample - previous[i]);
            previous[i] = sample;
        }
        tilt = sampled ? (tilt * 3 + previous[0]) / 4 : previous[0];
        jerk = sampled ? delta : 0;
        sampled = true;
    }

    int tilt = 0;
    int jerk = 0;

private:
    int previous[3]{};
    uint32_t sampledAt = 0;
    bool sampled = false;
    bool ready = false;
};

struct Fight {
    infrared::Fighter self{};
    infrared::Fighter rival{};
    bool local = false;
    bool haveRival = false;
    bool acknowledged = false;
    bool sendAck = false;
    uint8_t acknowledgmentsSent = 0;
    uint8_t charge = 0;
    uint8_t ownHp = 0;
    uint8_t rivalHp = 0;
    uint8_t round = 0;
    uint8_t dealt = 0;
    uint8_t taken = 0;
    uint32_t lastShakeAt = 0;
    uint32_t nextSendAt = 0;
    uint32_t linkAt = 0;
    uint32_t turnAt = 0;
    const char *result = nullptr;
};

Button feed(pins::feed), pet(pins::pet), battle(pins::battle), selectButton(pins::select);
Adafruit_SSD1306 oled(128, 64, &Wire, -1, 400000, 400000);
Save save;
Audio audio;
Motion motion;
infrared::Link ir;
Fight fight;
State state = STATE_IDLE;
bool displayReady = false;
bool irReady = false;
bool battleHeld = false;
bool dizzy = false;
uint8_t statsPage = 0;
int petX = 28 * 16;
int velocity = 0;
int walk = 1;
uint32_t stateAt = 0;
uint32_t activeAt = 0;
uint32_t physicsAt = 0;
uint32_t careAt = 0;
uint32_t frameAt = 0;
uint32_t dizzyAt = 0;

void enter(State next, uint32_t now) {
    state = next;
    stateAt = now;
}

void care(uint32_t now) {
    if (now - careAt < 300000) return;
    careAt = now;
    Progress &p = save.pet;
    p.hunger = max(0, int(p.hunger) - 4);
    p.happiness = max(0, int(p.happiness) - 2);
    if (p.hunger >= 20) p.hp = min(int(maxHp(p.level)), p.hp + 3);
    save.changed();
}

void physics(uint32_t now) {
    if (state != STATE_IDLE) return;
    if (abs(motion.tilt) > 575 && motion.jerk > 260) {
        dizzy = true;
        dizzyAt = now;
    }
    if (now - physicsAt < 24) return;
    physicsAt = now;
    if (abs(motion.tilt) > 160) {
        velocity = constrain((velocity * 3) / 4 + motion.tilt / 85, -38, 38);
    } else {
        velocity = (velocity * 3) / 4 + 2 * walk;
    }
    petX = constrain(petX + velocity, 0, (128 - sprites::width) * 16);
    if (petX == 0 || petX == (128 - sprites::width) * 16) {
        walk = petX == 0 ? 1 : -1;
        velocity = 0;
    }
    if (dizzy && now - dizzyAt >= 950) dizzy = false;
}

void startCharge(bool local, uint32_t now) {
    fight = Fight{};
    fight.local = local;
    fight.lastShakeAt = now - 160;
    if (!local && !irReady) {
        fight.result = "IR unavailable";
        enter(STATE_RESULT, now);
    } else if (save.pet.hp == 0) {
        fight.result = "Feed to recover HP";
        enter(STATE_RESULT, now);
    } else {
        if (!local) {
            infrared::Fighter unused{};
            bool ack = false;
            while (ir.receive(unused, ack)) {}
        }
        enter(STATE_CHARGE, now);
    }
}

void startFight(uint32_t now) {
    fight.ownHp = maxHp(fight.self.level);
    fight.rivalHp = maxHp(fight.rival.level);
    fight.turnAt = now;
    enter(STATE_BATTLE, now);
}

void tickCharge(uint32_t now) {
    if (motion.jerk > 480 && now - fight.lastShakeAt >= 160) {
        fight.lastShakeAt = now;
        fight.charge = min(25, int(fight.charge) + 5);
    }
    if (now - stateAt < chargeMs) return;

    const uint8_t power = attackPower(save.pet.level);
    fight.self = {edition::wireId, save.pet.level, uint8_t((power * (100 + fight.charge) + 50) / 100)};
    if (fight.local) {
        const uint8_t level = uint8_t(constrain(int(save.pet.level) + int(esp_random() % 5) - 2, 1, int(maxLevel)));
        const uint8_t rivalDex = esp_random() % 2 ? infrared::pikachu : infrared::charizard;
        fight.rival = {rivalDex, level, attackPower(level)};
        startFight(now);
    } else {
        fight.linkAt = now;
        fight.nextSendAt = now + esp_random() % 240;
        enter(STATE_IR_LINK, now);
    }
}

void tickLink(uint32_t now) {
    infrared::Fighter packet{};
    bool ack = false;
    while (ir.receive(packet, ack)) {
        if (ack) {
            if (infrared::same(packet, fight.self)) fight.acknowledged = true;
        } else if (!fight.haveRival || infrared::same(packet, fight.rival)) {
            fight.rival = packet;
            fight.haveRival = true;
            fight.sendAck = true;
        }
    }

    if (int32_t(now - fight.nextSendAt) >= 0) {
        if (fight.sendAck && ir.send(fight.rival, true)) {
            fight.sendAck = false;
            ++fight.acknowledgmentsSent;
            fight.nextSendAt = now + 160 + esp_random() % 145;
        } else if (!fight.acknowledged && ir.send(fight.self, false)) {
            fight.nextSendAt = now + 210 + esp_random() % 170;
        }
    }

    if (state != STATE_IR_LINK) return;
    if (fight.haveRival && fight.acknowledged && fight.acknowledgmentsSent) {
        startFight(now);
    } else if (now - fight.linkAt >= linkTimeoutMs) {
        fight.result = "No IR opponent";
        enter(STATE_RESULT, now);
    }
}

uint8_t damage(infrared::Fighter from, infrared::Fighter to, uint8_t round) {
    // Use the same deterministic calculation on both ends of a link.
    uint32_t seed = infrared::pack(from, false) ^ (uint32_t(to.dex) << 16) ^
                    (uint32_t(round) * 0x9e3779b9u);
    seed ^= seed >> 16;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    if (seed % 100 < 8) return 0;
    return uint8_t(4 + from.attack * (57 + (seed >> 8) % 18) / 100);
}

void finishFight(uint32_t now) {
    const bool won = fight.ownHp > fight.rivalHp;
    const bool draw = fight.ownHp == fight.rivalHp;
    fight.result = won ? "Victory!" : draw ? "Draw" : "Defeat";
    Progress &p = save.pet;
    const int hpLost = maxHp(fight.self.level) - fight.ownHp;
    p.hp = max(0, int(p.hp) - hpLost);
    p.hunger = max(0, int(p.hunger) - 8);
    p.happiness = constrain(int(p.happiness) + (won ? 6 : draw ? 0 : -3), 0, 100);
    if (won && p.wins != UINT16_MAX) ++p.wins;
    if (p.level < maxLevel) {
        p.exp += won ? 35 : draw ? 20 : 12;
        if (p.exp >= 100) {
            ++p.level;
            p.exp = p.level == maxLevel ? 0 : p.exp - 100;
            p.hp = maxHp(p.level);
        }
    }
    save.changed();
    if (won) audio.play(victory, now);
    else if (!draw) audio.play(defeat, now);
    enter(STATE_RESULT, now);
}

void tickFight(uint32_t now) {
    if (now - fight.turnAt < 1000) return;
    if (fight.ownHp == 0 || fight.rivalHp == 0 || fight.round >= 6) {
        finishFight(now);
        return;
    }
    fight.turnAt = now;
    ++fight.round;
    fight.dealt = damage(fight.self, fight.rival, fight.round);
    fight.taken = damage(fight.rival, fight.self, fight.round);
    fight.ownHp = max(0, int(fight.ownHp) - fight.taken);
    fight.rivalHp = max(0, int(fight.rivalHp) - fight.dealt);
    audio.play(hit, now);
}

void input(uint32_t now) {
    feed.update(now);
    pet.update(now);
    battle.update(now);
    selectButton.update(now);

    if (feed.pressed || pet.pressed || battle.pressed || selectButton.pressed ||
        motion.jerk > 360) activeAt = now;
    if (state == STATE_SLEEP && (feed.pressed || pet.pressed || battle.pressed ||
                                  selectButton.pressed || motion.jerk > 360)) {
        activeAt = now;
        enter(STATE_IDLE, now);
    }

    if (state == STATE_CHARGE || state == STATE_IR_LINK || state == STATE_BATTLE ||
        state == STATE_RESULT) {
        if (battle.down || battle.released) battleHeld = true;
        if (selectButton.pressed) {
            fight.haveRival = false;
            enter(STATE_IDLE, now);
        }
        return;
    }

    if (battle.pressed) battleHeld = false;
    if (battle.down && !battleHeld && now - battle.pressedAt >= holdMs) {
        battleHeld = true;
        startCharge(true, now);
    } else if (battle.released && !battleHeld) {
        startCharge(false, now);
    } else if (feed.pressed) {
        Progress &p = save.pet;
        p.hunger = min(100, p.hunger + 18);
        p.hp = min(int(maxHp(p.level)), p.hp + 12);
        save.changed();
        audio.play(eat, now);
        enter(STATE_FEED, now);
    } else if (pet.pressed) {
        save.pet.happiness = min(100, save.pet.happiness + 12);
        save.changed();
        audio.play(cry, now);
        enter(STATE_PET, now);
    } else if (selectButton.pressed) {
        if (state == STATE_STATS && statsPage == 1) {
            enter(STATE_IDLE, now);
        } else {
            statsPage = state == STATE_STATS ? 1 : 0;
            enter(STATE_STATS, now);
        }
    }
}

void tickState(uint32_t now) {
    switch (state) {
        case STATE_IDLE:
        case STATE_STATS:
            if (now - activeAt >= sleepMs) enter(STATE_SLEEP, now);
            break;
        case STATE_FEED:
        case STATE_PET:
            if (now - stateAt >= 1050) enter(STATE_IDLE, now);
            break;
        case STATE_CHARGE:
            tickCharge(now);
            break;
        case STATE_IR_LINK:
            tickLink(now);
            break;
        case STATE_BATTLE:
            tickFight(now);
            break;
        case STATE_RESULT:
            if (now - stateAt >= 6500) {
                activeAt = now;
                enter(STATE_IDLE, now);
            }
            break;
        case STATE_SLEEP:
            break;
    }
    if (!fight.local && fight.haveRival && state != STATE_IR_LINK &&
        now - fight.linkAt < linkTimeoutMs) {
        // An opponent can still need an acknowledgment while this unit shows its battle.
        tickLink(now);
    }
}

void label(int x, int y, const char *text) {
    oled.setCursor(x, y);
    oled.print(text);
}

void number(int x, int y, unsigned value) {
    oled.setCursor(x, y);
    oled.print(value);
}

void header(const char *text) {
    oled.fillRect(0, 0, 128, 10, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    label(3, 1, text);
    oled.setTextColor(SSD1306_WHITE);
}

void footer(const char *text) {
    oled.drawFastHLine(4, 53, 120, SSD1306_WHITE);
    label(4, 56, text);
}

void gauge(int x, int y, int w, unsigned value, unsigned maximum) {
    oled.drawRect(x, y, w, 6, SSD1306_WHITE);
    if (value) oled.fillRect(x + 1, y + 1, (value * (w - 2) + maximum - 1) / maximum, 4, SSD1306_WHITE);
}

void hearts(int x, unsigned value) {
    for (int i = 0; i < 4; ++i) {
        const int here = x + i * 8;
        if (value > unsigned(i * 25)) {
            oled.drawBitmap(here, 1, sprites::heart, 7, 6, SSD1306_BLACK);
        } else {
            oled.drawFastHLine(here + 1, 4, 5, SSD1306_BLACK);
        }
    }
}

void drawPetScene(uint32_t now) {
    const uint32_t elapsed = now - stateAt;
    const Progress &p = save.pet;
    oled.fillRect(0, 0, 128, 9, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    label(2, 1, "H");
    hearts(9, p.hunger);
    label(45, 1, "M");
    hearts(52, p.happiness);
    label(95, 1, "Lv");
    number(108, 1, p.level);
    oled.setTextColor(SSD1306_WHITE);

    int x = petX / 16;
    const bool second = (now / 250) % 2 != 0;
    const bool happy = state == STATE_PET || (state == STATE_FEED && elapsed > 420);
    if (dizzy && state == STATE_IDLE) x += (now / 75) % 2 ? 2 : -2;
    if (happy) x += (elapsed / 85) % 2 ? 2 : -2;
    x = constrain(x, 0, 128 - sprites::width);
    const auto &profile = edition::profile;
    if (profile.idle) {
        const bool animate = second && state != STATE_SLEEP;
        const uint8_t *frame = animate && profile.step ? profile.step : profile.idle;
        const int bob = animate && !profile.step ? 1 : 0;
        oled.drawBitmap(x, 9 - bob, frame, sprites::width, sprites::height, SSD1306_WHITE);
    } else {
        label(x, 33, infrared::name(edition::wireId));
    }
    if (state == STATE_SLEEP) {
        label(x > 27 ? 5 : 108, 19 + (now / 500) % 3 * 3, "Zz");
    } else if (dizzy && state == STATE_IDLE) {
        label(x > 27 ? 5 : 108, 15, second ? "*o*" : "o*o");
    } else if (state == STATE_FEED && elapsed < 420) {
        oled.drawBitmap(x + 15, 10 + elapsed / 19, sprites::berry, 7, 7, SSD1306_WHITE);
    } else if (happy) {
        const int drift = (elapsed / 33) % 20;
        oled.drawBitmap(x + 7, 34 - drift, sprites::heart, 7, 6, SSD1306_WHITE);
        oled.drawBitmap(x + 51, 37 - drift, sprites::heart, 7, 6, SSD1306_WHITE);
        oled.drawBitmap(x + 62, 28 - drift, sprites::heart, 7, 6, SSD1306_WHITE);
    }
}

void drawStats() {
    const Progress &p = save.pet;
    header(edition::profile.pokedexTitle);

    if (statsPage == 0) {
        label(4, 15, "LEVEL");
        number(64, 15, p.level);
        label(4, 26, "EXP");
        gauge(29, 27, 57, p.level == maxLevel ? 100 : p.exp, 100);
        if (p.level == maxLevel) label(93, 26, "MAX");
        else number(93, 26, p.exp);
        label(4, 40, "HP");
        number(64, 40, p.hp);
        oled.print('/');
        oled.print(maxHp(p.level));
        footer("SELECT  1/2");
    } else {
        label(4, 15, "ATTACK");
        number(64, 15, attackPower(p.level));
        label(4, 26, "SPEED");
        number(64, 26, speed(p.level));
        label(4, 40, "WINS");
        number(64, 40, p.wins);
        footer("SELECT  2/2");
    }
}

void drawBattle(uint32_t now) {
    if (state == STATE_CHARGE) {
        header(fight.local ? "PRACTICE" : "IR BATTLE");
        label(4, 15, "Shake to charge!");
        number(107, 15, (chargeMs - (now - stateAt) + 999) / 1000);
        gauge(4, 30, 98, fight.charge, 25);
        label(4, 41, "Attack bonus +");
        oled.print(fight.charge);
        oled.print('%');
        footer("SELECT Cancel");
        return;
    }
    if (state == STATE_IR_LINK) {
        header("IR BATTLE");
        label(4, 18, "Point devices");
        label(4, 30, "Waiting for rival");
        footer("SELECT Cancel");
        return;
    }
    if (state == STATE_RESULT) {
        header("BATTLE RESULT");
        label(4, 19, fight.result);
        if (fight.round) {
            label(4, 34, "Wins");
            number(35, 34, save.pet.wins);
        }
        footer("SELECT Exit");
        return;
    }

    header(fight.local ? "PRACTICE" : "IR BATTLE");
    oled.setTextColor(SSD1306_BLACK);
    number(109, 1, fight.round);
    oled.setTextColor(SSD1306_WHITE);
    label(4, 15, infrared::name(fight.self.dex));
    gauge(62, 16, 35, fight.ownHp, maxHp(fight.self.level));
    number(103, 15, fight.ownHp);
    label(4, 26, infrared::name(fight.rival.dex));
    gauge(62, 27, 35, fight.rivalHp, maxHp(fight.rival.level));
    number(103, 26, fight.rivalHp);
    if (fight.round) {
        label(4, 42, "Hit");
        number(26, 42, fight.dealt);
        label(59, 42, "Took");
        number(88, 42, fight.taken);
    } else {
        label(4, 42, "Ready!");
    }
    footer("SELECT Exit");
}

bool responds(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

void render(uint32_t now) {
    if (!displayReady || now - frameAt < 67) return;
    frameAt = now;
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setTextWrap(false);
    if (state <= STATE_PET) drawPetScene(now);
    else if (state == STATE_STATS) drawStats();
    else drawBattle(now);
    oled.display();
}

}  // namespace

void setup() {
    feed.begin();
    pet.begin();
    battle.begin();
    selectButton.begin();
    digitalWrite(pins::irTx, LOW);
    pinMode(pins::irTx, OUTPUT);
    pinMode(pins::irRx, INPUT);
    audio.begin();
    irReady = ir.begin(pins::irTx, pins::irRx);
    if (Wire.begin(pins::sda, pins::scl, 400000)) {
        Wire.setTimeOut(15);
        const uint8_t address = responds(0x3c) ? 0x3c : responds(0x3d) ? 0x3d : 0;
        displayReady = address && oled.begin(SSD1306_SWITCHCAPVCC, address, false, false);
        motion.begin();
    }
    save.begin();
    activeAt = careAt = stateAt = millis();
}

void loop() {
    const uint32_t now = millis();
    if (irReady) ir.poll();
    motion.tick(now);
    input(now);
    tickState(now);
    physics(now);
    care(now);
    audio.tick(now);
    save.flush(now);
    render(now);
}
