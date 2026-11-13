#pragma once

#include <stdint.h>

namespace duel {

enum class Move : uint8_t { Quick, Power, Guard };
enum class Type : uint8_t { Dragon, Ground, Electric, Normal, Fire, Flying };
enum class Message : uint8_t { Invite, Join, HostMove, Reply, Finish, Cancel };
enum class View : uint8_t { Closed, Menu, Practice, Pairing, Fight, Result, Notice };

struct Attack {
    const char *name;
    Type type;
};

struct Species {
    uint16_t dex;
    const char *name;
    Type first;
    Type second;
    Attack quick;
    Attack power;
};

constexpr Species roster[] = {
    {445, "Garchomp", Type::Dragon, Type::Ground,
     {"Dragon Claw", Type::Dragon}, {"Earthquake", Type::Ground}},
    {25, "Pikachu", Type::Electric, Type::Electric,
     {"Quick Attack", Type::Normal}, {"Thunderbolt", Type::Electric}},
    {6, "Charizard", Type::Fire, Type::Flying,
     {"Air Slash", Type::Flying}, {"Flamethrower", Type::Fire}},
};

constexpr uint8_t maxHp = 60;
constexpr uint8_t maxTurns = 24;
constexpr uint8_t packetBits = 40;
constexpr uint32_t retryMs = 460;
constexpr uint32_t pairTimeoutMs = 15000;
constexpr uint32_t turnTimeoutMs = 45000;
constexpr uint32_t finalRetryMs = 3500;

struct Frame {
    Message kind;
    uint16_t dex;
    uint16_t tag;
    uint8_t turn;
    Move move;
};

constexpr unsigned speciesCount = sizeof(roster) / sizeof(roster[0]);

constexpr const Species *species(uint16_t dex, unsigned index = 0) {
    return index == speciesCount ? nullptr :
           roster[index].dex == dex ? &roster[index] : species(dex, index + 1);
}

inline uint8_t checksum(uint32_t data) {
    uint8_t crc = 0xd5;
    for (uint8_t byte = 0; byte < 4; ++byte) {
        crc ^= static_cast<uint8_t>(data >> (byte * 8));
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = static_cast<uint8_t>((crc << 1) ^ ((crc & 0x80) ? 0x07 : 0));
        }
    }
    return crc;
}

inline uint64_t encode(Frame frame) {
    const uint32_t data = 0x80000000u | uint32_t(frame.dex) |
        (uint32_t(frame.tag) << 11) | (uint32_t(frame.kind) << 21) |
        (uint32_t(frame.turn) << 24) | (uint32_t(frame.move) << 29);
    return uint64_t(data) | (uint64_t(checksum(data)) << 32);
}

inline bool decode(uint64_t packet, Frame &frame) {
    const uint32_t data = static_cast<uint32_t>(packet);
    if (!(data & 0x80000000u) || packet >> 32 != checksum(data)) return false;

    frame = {static_cast<Message>((data >> 21) & 7), uint16_t(data & 0x7ff),
             uint16_t((data >> 11) & 0x3ff), uint8_t((data >> 24) & 31),
             static_cast<Move>((data >> 29) & 3)};
    if (!species(frame.dex) || !frame.tag || frame.kind > Message::Cancel ||
        frame.move > Move::Guard) return false;
    if (frame.kind == Message::Invite || frame.kind == Message::Join || frame.kind == Message::Cancel) {
        return frame.turn == 0 && frame.move == Move::Quick;
    }
    return frame.turn > 0 && frame.turn <= maxTurns;
}

struct Hit {
    uint8_t damage;
    bool missed;
    bool immune;
};

inline int effectiveness(Type attack, Type target) {
    if ((attack == Type::Electric && target == Type::Ground) ||
        (attack == Type::Ground && target == Type::Flying)) return 0;
    if ((attack == Type::Dragon && target == Type::Dragon) ||
        (attack == Type::Electric && target == Type::Flying) ||
        (attack == Type::Ground && (target == Type::Electric || target == Type::Fire))) return 4;
    if ((attack == Type::Fire && (target == Type::Dragon || target == Type::Fire)) ||
        (attack == Type::Electric && (target == Type::Electric || target == Type::Dragon))) return 1;
    return 2;
}

inline uint32_t mix(uint32_t seed) {
    seed ^= seed >> 16;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    seed *= 0x846ca68bu;
    return seed ^ (seed >> 16);
}

inline Hit attack(const Species &attacker, const Species &defender, Move move,
                  Move defense, uint32_t seed) {
    if (move == Move::Guard) {
        // A guard counters an attacker, giving defensive play a way to win.
        return {static_cast<uint8_t>(defense == Move::Guard ? 0 : 6), false, false};
    }

    const bool power = move == Move::Power;
    const Type type = (power ? attacker.power : attacker.quick).type;
    int multiplier = effectiveness(type, defender.first);
    if (defender.second != defender.first) {
        multiplier = multiplier * effectiveness(type, defender.second) / 2;
    }
    if (multiplier == 0) return {0, false, true};
    const uint32_t roll = mix(seed);
    if (roll % 100 >= (power ? 75u : 95u)) return {0, true, false};

    int damage = ((power ? 25 : 16) + int((roll >> 8) % 5)) * multiplier / 2;
    if (defense == Move::Guard) damage = (damage + 3) / 4;
    return {static_cast<uint8_t>(damage), false, false};
}

struct Round {
    Move hostMove;
    Move guestMove;
    Hit hostHit;
    Hit guestHit;
};

struct Match {
    const Species *host = nullptr;
    const Species *guest = nullptr;
    uint16_t tag = 0;
    uint8_t turn = 1;
    uint8_t hostHp = maxHp;
    uint8_t guestHp = maxHp;
    Round last{};

    bool finished() const {
        return hostHp == 0 || guestHp == 0 || turn > maxTurns;
    }

    void resolve(Move hostMove, Move guestMove) {
        const uint32_t seed = (uint32_t(tag) << 22) ^ (uint32_t(host->dex) << 11) ^
                              guest->dex ^ (uint32_t(turn) * 0x9e3779b9u);
        last = {hostMove, guestMove, attack(*host, *guest, hostMove, guestMove, seed),
                attack(*guest, *host, guestMove, hostMove, seed ^ 0xa5b47c19u)};
        hostHp = last.guestHit.damage >= hostHp ? 0 : hostHp - last.guestHit.damage;
        guestHp = last.hostHit.damage >= guestHp ? 0 : guestHp - last.hostHit.damage;
        ++turn;
    }
};

}  // namespace duel
