#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <Wire.h>
#include <esp_system.h>

#include "board_pins.h"
#include "battle_session.h"

namespace {
constexpr uint8_t kAccelAddress = 0x18;
constexpr uint8_t kAudioChannel = 0;
constexpr uint32_t kDebounceMs = 25;
constexpr uint32_t kCareIntervalMs = 5 * 60 * 1000;
constexpr uint32_t kMotionIntervalMs = 40;
constexpr uint8_t kMaxStat = 100;

struct Button {
    explicit Button(uint8_t gpio) : pin(gpio) {}

    const uint8_t pin;
    bool rawPressed = false;
    bool stablePressed = false;
    uint32_t changedAt = 0;

    void begin() const {
        pinMode(pin, INPUT_PULLUP);
    }

    bool pressed(uint32_t now) {
        const bool reading = digitalRead(pin) == LOW;
        if (reading != rawPressed) {
            rawPressed = reading;
            changedAt = now;
        }
        if (stablePressed == rawPressed || now - changedAt < kDebounceMs) {
            return false;
        }
        stablePressed = rawPressed;
        return stablePressed;
    }
};

struct Pet {
    uint8_t fullness = 65;
    uint8_t mood = 60;
    uint8_t energy = 80;
    uint8_t wins = 0;
};

struct Note {
    uint16_t hz;
    uint16_t duration;
};

constexpr Note kFeedNotes[] = {{540, 75}, {740, 85}};
constexpr Note kPetNotes[] = {{420, 70}, {550, 75}, {840, 105}};
constexpr Note kWinNotes[] = {{550, 65}, {740, 65}, {1100, 125}};
constexpr Note kLoseNotes[] = {{540, 70}, {340, 130}};

Button feedButton(pins::feed);
Button petButton(pins::pet);
Button battleButton(pins::battle);
Button selectButton(pins::select);
Adafruit_SSD1306 screen(128, 64, &Wire, -1, 100000, 100000);
Preferences storage;
Pet pet;
// Change this Pokédex ID when building firmware for another species in the roster.
#ifndef DSH_DEX_SPECIES
#define DSH_DEX_SPECIES 445
#endif
constexpr uint16_t kLocalDex = DSH_DEX_SPECIES;
static_assert(duel::species(kLocalDex) != nullptr, "DSH_DEX_SPECIES must match a battle roster entry");
constexpr const duel::Species &localSpecies = *duel::species(kLocalDex);
duel::Session battle(localSpecies);
size_t practiceRival = kLocalDex == 25 ? 2 : 1;
bool irReady = false;
bool displayReady = false;
bool motionReady = false;
bool storageReady = false;
bool audioReady = false;
bool dirty = true;
bool motionSampleReady = false;
uint8_t page = 0;
uint32_t careAt = 0;
uint32_t lastSaved = UINT32_MAX;
uint32_t sampledAt = 0;
uint32_t shakeAt = 0;
int16_t previous[3] = {};
const char *status = "Ready";
const Note *notes = nullptr;
size_t noteCount = 0;
size_t noteIndex = 0;
uint32_t noteStartedAt = 0;

uint8_t adjusted(uint8_t value, int change) {
    return static_cast<uint8_t>(constrain(static_cast<int>(value) + change, 0, kMaxStat));
}

void savePet() {
    if (!storageReady) {
        return;
    }
    const uint32_t packed = uint32_t(pet.fullness) | (uint32_t(pet.mood) << 8) |
                            (uint32_t(pet.energy) << 16) | (uint32_t(pet.wins) << 24);
    if (packed == lastSaved) {
        return;
    }
    if (storage.putUInt("pet-v1", packed) == sizeof(packed)) {
        lastSaved = packed;
    } else {
        Serial.println("Could not save pet");
    }
}

void loadPet() {
    storageReady = storage.begin("dsh-dex", false);
    if (!storageReady || !storage.isKey("pet-v1")) {
        return;
    }
    const uint32_t packed = storage.getUInt("pet-v1", UINT32_MAX);
    Pet saved;
    saved.fullness = uint8_t(packed);
    saved.mood = uint8_t(packed >> 8);
    saved.energy = uint8_t(packed >> 16);
    saved.wins = uint8_t(packed >> 24);
    if (saved.fullness <= kMaxStat && saved.mood <= kMaxStat && saved.energy <= kMaxStat) {
        pet = saved;
        lastSaved = packed;
    }
}

bool responds(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

bool readAccel(uint8_t reg, uint8_t *buffer, size_t count) {
    Wire.beginTransmission(kAccelAddress);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(kAccelAddress, count, true) != count) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        buffer[i] = static_cast<uint8_t>(Wire.read());
    }
    return true;
}

bool writeAccel(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(kAccelAddress);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool setupMotion() {
    uint8_t who = 0;
    return readAccel(0x0F, &who, 1) && who == 0x33 &&
           writeAccel(0x23, 0x88) &&   // Block updates between byte reads, high resolution, +/-2 g.
           writeAccel(0x20, 0x47);     // 50 Hz, all three axes.
}

void setStatus(const char *message) {
    status = message;
    dirty = true;
    Serial.println(message);
}

template <size_t N>
void play(const Note (&sequence)[N], uint32_t now) {
    if (!audioReady) {
        return;
    }
    notes = sequence;
    noteCount = N;
    noteIndex = 0;
    noteStartedAt = now;
    ledcWriteTone(kAudioChannel, notes[0].hz);
}

void tickAudio(uint32_t now) {
    if (!notes || now - noteStartedAt < notes[noteIndex].duration) {
        return;
    }
    if (++noteIndex == noteCount) {
        ledcWriteTone(kAudioChannel, 0);
        notes = nullptr;
        return;
    }
    noteStartedAt = now;
    ledcWriteTone(kAudioChannel, notes[noteIndex].hz);
}

void tickMotion(uint32_t now) {
    if (!motionReady || now - sampledAt < kMotionIntervalMs) {
        return;
    }
    sampledAt = now;
    uint8_t bytes[6];
    if (!readAccel(0x28 | 0x80, bytes, sizeof(bytes))) {
        motionSampleReady = false;
        return;
    }
    int totalChange = 0;
    for (size_t i = 0; i < 3; ++i) {
        const uint16_t raw = uint16_t(bytes[i * 2]) | (uint16_t(bytes[i * 2 + 1]) << 8);
        const int16_t sample = static_cast<int16_t>(raw) / 16;
        totalChange += abs(int(sample) - previous[i]);
        previous[i] = sample;
    }
    if (motionSampleReady && totalChange > 1550 && now - shakeAt >= 4500) {
        shakeAt = now;
        pet.mood = adjusted(pet.mood, 2);
        setStatus("Shake bonus!");
        play(kPetNotes, now);
    }
    motionSampleReady = true;
}

void tickCare(uint32_t now) {
    if (now - careAt < kCareIntervalMs) {
        return;
    }
    careAt = now;
    pet.fullness = adjusted(pet.fullness, -4);
    pet.mood = adjusted(pet.mood, -2);
    pet.energy = adjusted(pet.energy, 5);
    savePet();
    dirty = true;
}

bool readyToBattle() {
    return pet.energy >= 15 && pet.fullness >= 10;
}

void payBattleCost() {
    pet.energy = adjusted(pet.energy, -15);
    pet.fullness = adjusted(pet.fullness, -8);
    savePet();
}

void battleResult(uint32_t now) {
    if (battle.ownHp() > battle.opponentHp()) {
        if (pet.wins != UINT8_MAX) ++pet.wins;
        savePet();
        setStatus("Battle won!");
        play(kWinNotes, now);
    } else if (battle.ownHp() < battle.opponentHp()) {
        setStatus("Battle lost");
        play(kLoseNotes, now);
    } else {
        setStatus("Battle draw");
        play(kPetNotes, now);
    }
}

void tickButtons(uint32_t now) {
    const bool feed = feedButton.pressed(now);
    const bool petPress = petButton.pressed(now);
    const bool confirm = battleButton.pressed(now);
    const bool back = selectButton.pressed(now);
    if (!feed && !petPress && !confirm && !back) return;
    dirty = true;

    switch (battle.view) {
        case duel::View::Menu:
            if (back) battle.enter(duel::View::Closed, now);
            else if (feed || (!irReady && confirm)) battle.enter(duel::View::Practice, now);
            else if (irReady && (petPress || confirm)) battle.pair(petPress, now);
            return;
        case duel::View::Practice:
            if (back) battle.enter(duel::View::Menu, now);
            else if (feed || petPress) {
                practiceRival = (practiceRival + (feed ? 1 : duel::speciesCount - 1)) % duel::speciesCount;
            } else if (confirm) {
                battle.practice(duel::roster[practiceRival], now);
                payBattleCost();
            }
            return;
        case duel::View::Fight:
            if (back) battle.cancel(now);
            else if (!battle.locked && !battle.match.finished()) {
                if (feed || petPress) battle.chooseNext(feed);
                else if (confirm) battle.lock(now);
            }
            return;
        case duel::View::Pairing:
        case duel::View::Result:
        case duel::View::Notice:
            if (back) battle.enter(duel::View::Closed, now);
            return;
        case duel::View::Closed:
            break;
    }

    if (feed) {
        pet.fullness = adjusted(pet.fullness, 18);
        savePet();
        setStatus("Pet ate!");
        play(kFeedNotes, now);
    }
    if (petPress) {
        pet.mood = adjusted(pet.mood, 12);
        savePet();
        setStatus("Pet grins!");
        play(kPetNotes, now);
    }
    if (confirm) {
        if (readyToBattle()) {
            battle.linked = false;
            battle.enter(duel::View::Menu, now);
        } else {
            battle.notice = pet.fullness < 10 ? "Feed your pet first" : "Needs 15 energy";
            battle.enter(duel::View::Notice, now);
            setStatus("Feed or rest first");
            play(kLoseNotes, now);
        }
    }
    if (back) {
        page = (page + 1) % 3;
        Serial.printf("Page %u\n", unsigned(page));
    }
}

void label(int16_t x, int16_t y, const char *value) {
    screen.setCursor(x, y);
    screen.print(value);
}

void stat(int16_t y, const char *name, uint8_t value) {
    label(4, y, name);
    for (uint8_t i = 0; i < 10; ++i) {
        const int16_t x = 49 + 5 * i;
        screen.drawRect(x, y + 1, 4, 6, SSD1306_WHITE);
        if (value > 10 * i) {
            const int16_t width = value >= 10 * (i + 1) ? 2 : 1;
            screen.fillRect(x + 1, y + 2, width, 4, SSD1306_WHITE);
        }
    }
    screen.setCursor(106, y);
    screen.print(value);
}

void marginGauge(int16_t y, const char *name, uint8_t value) {
    label(103, y, name);
    screen.drawRect(103, y + 9, 23, 4, SSD1306_WHITE);
    screen.drawFastHLine(104, y + 10, (value * 21 + 50) / 100, SSD1306_WHITE);
}

const char *shortStatus() {
    if (strcmp(status, "Pet ate!") == 0) return "ATE";
    if (strcmp(status, "Pet grins!") == 0) return "PET";
    if (strcmp(status, "Battle won!") == 0) return "WIN";
    if (strcmp(status, "Battle lost") == 0) return "LOSS";
    if (strcmp(status, "Battle draw") == 0) return "DRAW";
    if (strcmp(status, "Feed or rest first") == 0) return "REST";
    if (strcmp(status, "Shake bonus!") == 0) return "MOVE";
    return "IDLE";
}

void pageTitle(const char *name, const char *number) {
    screen.fillRect(0, 0, 128, 11, SSD1306_WHITE);
    screen.setTextColor(SSD1306_BLACK);
    label(4, 2, name);
    label(105, 2, number);
    screen.setTextColor(SSD1306_WHITE);
}

void drawPet() {
    static const uint8_t garchompSprite[] PROGMEM = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x0e, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x3f, 0x80, 0x3f, 0xc0, 0x00, 0x00, 0x00, 0x00,
        0x00, 0xf0, 0x7f, 0xbb, 0xe0, 0x00, 0x00, 0x00, 0x00,
        0x01, 0xe1, 0xc0, 0x1f, 0x30, 0x00, 0x00, 0x00, 0x00,
        0x03, 0x63, 0x00, 0x0c, 0x08, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x0f, 0xf0, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x0f, 0xe0, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x0f, 0x8c, 0x77, 0xc0, 0x00, 0x00, 0x00, 0x00,
        0x01, 0xce, 0x18, 0x20, 0x00, 0x3f, 0x80, 0x00, 0x00,
        0x00, 0x0c, 0x38, 0x30, 0x00, 0xe1, 0x00, 0x00, 0x00,
        0x00, 0x18, 0x00, 0x10, 0x03, 0x82, 0x00, 0x00, 0x00,
        0x00, 0x09, 0xff, 0x18, 0x0e, 0x06, 0x00, 0x00, 0x00,
        0x00, 0x0a, 0x3f, 0x08, 0x30, 0x04, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x7f, 0x84, 0xe0, 0x08, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xf3, 0x86, 0x80, 0x08, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x03, 0xc3, 0x80, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x03, 0xc2, 0xc0, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x0f, 0xe4, 0x20, 0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x31, 0xec, 0x10, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x27, 0xdc, 0x12, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x27, 0xfc, 0x16, 0x20, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xef, 0xfc, 0x0e, 0xc0, 0x00, 0x00, 0x00,
        0x00, 0x30, 0xcf, 0xfe, 0x06, 0x1c, 0x00, 0x00, 0x00,
        0x00, 0xf1, 0x8f, 0xfc, 0xe3, 0xb6, 0x00, 0x00, 0x00,
        0x01, 0xf3, 0x1f, 0xf8, 0x90, 0xfb, 0x00, 0x00, 0x00,
        0x07, 0xde, 0x33, 0xf9, 0x08, 0x7d, 0x00, 0x00, 0x00,
        0x0f, 0x80, 0x41, 0xf9, 0x08, 0x7f, 0x80, 0x00, 0x00,
        0x0f, 0x80, 0x41, 0xf1, 0x88, 0x7e, 0x80, 0x00, 0x00,
        0x1f, 0x00, 0x80, 0xf0, 0x84, 0x7f, 0x80, 0x00, 0x00,
        0x1f, 0x07, 0x03, 0xf0, 0xc7, 0x3f, 0x40, 0x00, 0x3e,
        0x3f, 0x18, 0x0e, 0xf2, 0x31, 0xbf, 0xc0, 0x00, 0xe4,
        0x3e, 0x30, 0x19, 0xfc, 0x18, 0xdf, 0x80, 0x03, 0x08,
        0x3e, 0xc0, 0x21, 0xf8, 0x0c, 0x3f, 0x80, 0x06, 0x10,
        0x7c, 0x80, 0x41, 0xf8, 0x14, 0x0f, 0x80, 0x08, 0x30,
        0x7d, 0x06, 0x01, 0xf8, 0x72, 0x07, 0x80, 0x30, 0x20,
        0x7b, 0x07, 0x83, 0xf0, 0xf3, 0x07, 0x80, 0x20, 0x40,
        0x7a, 0x07, 0x82, 0xf8, 0xe1, 0x83, 0x80, 0x40, 0x40,
        0x78, 0x03, 0x02, 0x68, 0x61, 0x63, 0x80, 0xc0, 0x80,
        0x70, 0x19, 0x03, 0x28, 0x6c, 0xbb, 0x87, 0xc0, 0x80,
        0x70, 0x1e, 0x01, 0x08, 0x1c, 0x8f, 0xfe, 0x00, 0x80,
        0x70, 0x1c, 0x01, 0x0c, 0x3c, 0x81, 0xf0, 0x00, 0x80,
        0x60, 0x18, 0x01, 0x84, 0x18, 0x81, 0x80, 0x01, 0x00,
        0x60, 0x10, 0x06, 0x64, 0x09, 0x01, 0x80, 0x31, 0x00,
        0x60, 0x00, 0x18, 0x36, 0x01, 0x00, 0x00, 0x61, 0x00,
        0x20, 0x10, 0x30, 0x0e, 0x01, 0x00, 0x01, 0x91, 0x00,
        0x00, 0x18, 0x20, 0x03, 0x81, 0x00, 0x06, 0x09, 0x00,
        0x00, 0x04, 0x20, 0x00, 0xc1, 0x00, 0x3c, 0x04, 0x80,
        0x00, 0x06, 0x10, 0x00, 0x21, 0x01, 0xf0, 0x03, 0x80,
        0x00, 0x02, 0x08, 0x00, 0x30, 0xff, 0x80, 0x01, 0x80,
        0x00, 0x02, 0x08, 0x00, 0x10, 0x80, 0x00, 0x00, 0x00,
        0x00, 0x04, 0x10, 0x00, 0x10, 0x40, 0x00, 0x00, 0x00,
        0x00, 0x18, 0x20, 0x00, 0x10, 0x40, 0x00, 0x00, 0x00,
        0x00, 0x70, 0x20, 0x00, 0x20, 0x20, 0x00, 0x00, 0x00,
        0x01, 0x80, 0x00, 0x00, 0x20, 0x10, 0x00, 0x00, 0x00,
        0x03, 0xfe, 0x40, 0x00, 0x73, 0x28, 0x00, 0x00, 0x00,
        0x07, 0xfe, 0x40, 0x00, 0x7b, 0xf0, 0x00, 0x00, 0x00,
        0x06, 0xfe, 0x80, 0x00, 0xfb, 0xf8, 0x00, 0x00, 0x00,
        0x01, 0x98, 0x00, 0x00, 0x7b, 0xb8, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x61, 0x98, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    screen.drawBitmap(28, 0, garchompSprite, 72, 64, SSD1306_WHITE);
}

const char *moveName(const duel::Species &species, duel::Move move) {
    if (move == duel::Move::Guard) return "Guard";
    return move == duel::Move::Power ? species.power.name : species.quick.name;
}

void battleFooter(const char *text) {
    screen.drawFastHLine(4, 53, 120, SSD1306_WHITE);
    label(4, 56, text);
}

void drawHp(int16_t y, const char *name, uint8_t hp) {
    label(4, y, name);
    screen.drawRect(60, y, 43, 7, SSD1306_WHITE);
    if (hp) screen.fillRect(61, y + 1, (hp * 41 + duel::maxHp - 1) / duel::maxHp, 5, SSD1306_WHITE);
    screen.setCursor(109, y);
    screen.print(hp);
}

void drawHit(int16_t y, const char *name, duel::Hit hit) {
    label(4, y, name);
    if (hit.immune) label(88, y, "No hit");
    else if (hit.missed) label(88, y, "Miss");
    else {
        screen.setCursor(94, y);
        if (hit.damage) screen.print('-');
        screen.print(hit.damage);
        screen.print("HP");
    }
}

void drawBattle() {
    switch (battle.view) {
        case duel::View::Menu:
            pageTitle("BATTLE", "   ");
            label(4, 16, "F Practice");
            if (irReady) {
                label(4, 28, "P Host IR duel");
                label(4, 40, "B Join IR duel");
            } else {
                label(4, 30, "IR link unavailable");
                label(4, 41, "Costs 15 energy");
            }
            battleFooter("S Back");
            return;
        case duel::View::Practice: {
            pageTitle("CHOOSE RIVAL", "   ");
            const duel::Species &rival = duel::roster[practiceRival];
            label(4, 19, rival.name);
            screen.setCursor(4, 33);
            screen.print('#');
            screen.print(rival.dex);
            battleFooter("F/P Pick  B Fight");
            return;
        }
        case duel::View::Pairing:
            pageTitle(battle.hosting ? "HOST IR DUEL" : "JOIN IR DUEL", " IR");
            label(4, 21, "Point devices");
            label(4, 33, battle.hosting ? "Ask rival to join" : "Waiting for host");
            battleFooter("S Cancel");
            return;
        case duel::View::Notice:
            pageTitle("BATTLE", "   ");
            label(4, 24, battle.notice);
            battleFooter("S Back");
            return;
        case duel::View::Closed:
            return;
        default:
            break;
    }

    const bool result = battle.view == duel::View::Result;
    const char *title = battle.linked ? "LINK BATTLE" : "PRACTICE";
    if (result) {
        title = battle.ownHp() > battle.opponentHp() ? "VICTORY" :
                battle.ownHp() < battle.opponentHp() ? "DEFEAT" : "DRAW";
    }
    const uint8_t round = battle.match.turn - (result || battle.match.finished() ? 1 : 0);
    const char turn[] = {'T', char('0' + round / 10), char('0' + round % 10), '\0'};
    pageTitle(title, turn);
    drawHp(14, localSpecies.name, battle.ownHp());
    drawHp(23, battle.opponent().name, battle.opponentHp());
    if (battle.match.turn > 1) {
        const duel::Round &last = battle.match.last;
        drawHit(34, moveName(localSpecies, battle.isHost() ? last.hostMove : last.guestMove),
                battle.isHost() ? last.hostHit : last.guestHit);
        drawHit(43, moveName(battle.opponent(), battle.isHost() ? last.guestMove : last.hostMove),
                battle.isHost() ? last.guestHit : last.hostHit);
    } else {
        label(4, 37, "Pick your first move");
    }
    if (result) battleFooter("S Exit battle");
    else if (battle.locked || battle.match.finished()) battleFooter("Waiting for rival...");
    else {
        battleFooter("F/P");
        label(29, 56, moveName(localSpecies, battle.choice));
        label(103, 56, "B OK");
    }
}

void render() {
    if (!displayReady || !dirty) {
        return;
    }
    dirty = false;
    screen.clearDisplay();
    screen.setTextSize(1);
    screen.setTextColor(SSD1306_WHITE);
    screen.setTextWrap(false);
    if (battle.view != duel::View::Closed) {
        drawBattle();
        screen.display();
        return;
    }
    if (page == 0) {
        if (kLocalDex == 445) drawPet();
        else label(30, 25, localSpecies.name);
        screen.setCursor(1, 2);
        screen.print('#');
        screen.print(localSpecies.dex);
        screen.drawFastHLine(1, 12, 21, SSD1306_WHITE);
        label(1, 18, "WINS");
        screen.setCursor(1, 28);
        screen.print(pet.wins);
        label(1, 55, shortStatus());
        marginGauge(3, "FULL", pet.fullness);
        marginGauge(23, "MOOD", pet.mood);
        marginGauge(43, "NRG", pet.energy);
    } else if (page == 1) {
        pageTitle("CARE", "2/3");
        stat(16, "FULL", pet.fullness);
        stat(28, "MOOD", pet.mood);
        stat(40, "ENERGY", pet.energy);
    } else {
        pageTitle("DEVICE CHECK", "3/3");
        label(4, 16, motionReady ? "MOTION      OK" : "MOTION      MISSING");
        label(4, 28, audioReady ? "PIEZO       READY" : "PIEZO       FAILED");
        label(4, 40, irReady ? "IR BATTLE   READY" : "IR BATTLE   OFF");
    }
    if (page != 0) {
        screen.drawFastHLine(4, 51, 120, SSD1306_WHITE);
        label(4, 55, status);
    }
    screen.display();
}
}  // namespace

void setup() {
    digitalWrite(pins::irTransmit, LOW);
    pinMode(pins::irTransmit, OUTPUT);
    pinMode(pins::irReceive, INPUT);
    feedButton.begin();
    petButton.begin();
    battleButton.begin();
    selectButton.begin();

    Serial.begin(115200);
    Serial.println("DSH-DEX / ESP32-C3");
    audioReady = ledcSetup(kAudioChannel, 1000, 8) != 0;
    if (audioReady) {
        ledcAttachPin(pins::audio, kAudioChannel);
        ledcWriteTone(kAudioChannel, 0);
    }

    if (Wire.begin(pins::sda, pins::scl, 100000)) {
        const uint8_t oled = responds(0x3C) ? 0x3C : (responds(0x3D) ? 0x3D : 0);
        displayReady = oled != 0 && screen.begin(SSD1306_SWITCHCAPVCC, oled, false, false);
        motionReady = setupMotion();
        Serial.printf("OLED: %s (0x%02X), LIS3DH: %s\n",
                      displayReady ? "OK" : "missing", unsigned(oled), motionReady ? "OK" : "missing");
    } else {
        Serial.println("I2C initialization failed");
    }

    irReady = beginBattleIr();
    loadPet();
    careAt = millis();
    render();
}

void loop() {
    const uint32_t now = millis();
    tickCare(now);
    tickMotion(now);
    const duel::View before = battle.view;
    tickButtons(now);
    if (irReady && (battle.view == duel::View::Pairing ||
                   (battle.linked && battle.view == duel::View::Fight) ||
                   battle.finalizing(now))) {
        dirty |= battle.tick(now);
    } else if (irReady) {
        duel::Frame unused;
        while (receiveBattleIr(unused)) {}
    }
    if (before == duel::View::Pairing && battle.view == duel::View::Fight) payBattleCost();
    if (before != duel::View::Result && battle.view == duel::View::Result) battleResult(now);
    tickAudio(now);
    render();
    delay(2);
}
