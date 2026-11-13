#pragma once

#include "battle_ir.h"

namespace duel {

struct Session {
    explicit Session(const Species &ownSpecies) : local(ownSpecies) {}

    const Species &local;
    View view = View::Closed;
    Match match;
    bool linked = false;
    bool hosting = false;
    bool locked = false;
    bool hostMoveReceived = false;
    bool pendingReply = false;
    bool finalPending = false;
    uint32_t finishedAt = 0;
    Move choice = Move::Quick;
    Move hostMove = Move::Quick;
    Frame reply{};
    const char *notice = nullptr;
    uint32_t stateAt = 0;
    uint32_t sendAt = 0;

    bool isHost() const { return !linked || hosting; }
    uint8_t ownHp() const { return isHost() ? match.hostHp : match.guestHp; }
    uint8_t opponentHp() const { return isHost() ? match.guestHp : match.hostHp; }
    const Species &opponent() const { return *(isHost() ? match.guest : match.host); }

    void enter(View next, uint32_t now) {
        view = next;
        stateAt = now;
    }

    void reset() {
        match = Match{};
        choice = Move::Quick;
        locked = false;
        hostMoveReceived = false;
        pendingReply = false;
        finalPending = false;
    }

    void practice(const Species &rival, uint32_t now) {
        reset();
        linked = false;
        match.host = &local;
        match.guest = &rival;
        match.tag = newTag();
        enter(View::Fight, now);
    }

    void pair(bool host, uint32_t now) {
        reset();
        linked = true;
        hosting = host;
        if (host) {
            match.host = &local;
            match.tag = newTag();
        } else {
            match.guest = &local;
        }
        sendAt = now;
        enter(View::Pairing, now);
    }

    void chooseNext(bool forward) {
        choice = static_cast<Move>((static_cast<uint8_t>(choice) + (forward ? 1 : 2)) % 3);
    }

    void cancel(uint32_t now) {
        if (linked && match.host && match.guest) {
            sendBattleIr({Message::Cancel, local.dex, match.tag, 0, Move::Quick});
        }
        enter(View::Closed, now);
    }

    void lock(uint32_t now) {
        locked = true;
        sendAt = now;
        if (!linked) {
            Move cpu = static_cast<Move>(esp_random() % 3);
            if (cpu == Move::Power && attack(opponent(), local, cpu, Move::Quick, 0).immune) {
                cpu = Move::Quick;
            }
            finishRound(choice, cpu, now);
        }
    }

    bool finalizing(uint32_t now) const {
        return finalPending && now - finishedAt < finalRetryMs;
    }

    bool tick(uint32_t now) {
        const bool retry = int32_t(now - sendAt) >= 0;
        if (finalizing(now) && retry) {
            send(Message::Finish, match.turn - 1, match.last.hostMove, now);
        }
        if (view != View::Pairing && view != View::Fight) return false;
        if (view == View::Pairing && now - stateAt >= pairTimeoutMs) {
            return showNotice("No device found", now);
        }
        if (view == View::Fight && now - stateAt >= turnTimeoutMs) {
            return showNotice("Link timed out", now);
        }

        Frame frame;
        while (receiveBattleIr(frame)) {
            if (accept(frame, now)) return true;
        }
        if (view == View::Pairing) {
            if (hosting && retry) send(Message::Invite, 0, Move::Quick, now);
        } else if (hosting && locked && retry) {
            send(Message::HostMove, match.turn, choice, now);
        } else if (!hosting) {
            if (locked && hostMoveReceived) {
                reply = {Message::Reply, local.dex, match.tag, match.turn, choice};
                pendingReply = true;
                hostMoveReceived = false;
                const Move selected = choice;
                // The guest computes the same HP as the host and waits for Finish on the final turn.
                finishRound(hostMove, selected, now);
                send(Message::Reply, reply.turn, reply.move, now);
                return true;
            }
            if (pendingReply && retry) send(Message::Reply, reply.turn, reply.move, now);
        }
        return false;
    }

private:
    static uint16_t newTag() { return 1 + esp_random() % 1023; }

    bool showNotice(const char *message, uint32_t now) {
        notice = message;
        enter(View::Notice, now);
        return true;
    }

    void send(Message kind, uint8_t turn, Move move, uint32_t now) {
        sendAt = now + retryMs + esp_random() % 155;
        sendBattleIr({kind, local.dex, match.tag, turn, move});
    }

    void finishRound(Move host, Move guest, uint32_t now) {
        match.resolve(host, guest);
        choice = Move::Quick;
        locked = false;
        stateAt = now;
        if (match.finished() && isHost()) {
            enter(View::Result, now);
            sendAt = now;
            if (linked) {
                finalPending = true;
                finishedAt = now;
                send(Message::Finish, match.turn - 1, host, now);
            }
        }
    }

    bool accept(Frame frame, uint32_t now) {
        if (view == View::Pairing) {
            if (!hosting && frame.kind == Message::Invite) {
                match.host = species(frame.dex);
                match.tag = frame.tag;
                enter(View::Fight, now);
                send(Message::Join, 0, Move::Quick, now);
                return true;
            }
            if (hosting && frame.kind == Message::Join && frame.tag == match.tag) {
                match.guest = species(frame.dex);
                enter(View::Fight, now);
                return true;
            }
            return false;
        }
        if (frame.tag != match.tag || frame.dex != opponent().dex) return false;
        if (frame.kind == Message::Cancel) return showNotice("Opponent left", now);
        if (hosting && frame.kind == Message::Reply && locked && frame.turn == match.turn) {
            finishRound(choice, frame.move, now);
            return true;
        }
        if (!hosting && frame.kind == Message::Invite && match.turn == 1 && !hostMoveReceived) {
            send(Message::Join, 0, Move::Quick, now);
        }
        if (!hosting && frame.kind == Message::HostMove && frame.turn == match.turn &&
            !match.finished() && !hostMoveReceived) {
            hostMove = frame.move;
            hostMoveReceived = true;
            pendingReply = false;
            stateAt = now;
            return true;
        }
        if (!hosting && frame.kind == Message::Finish && match.finished() &&
            frame.turn == match.turn - 1 && frame.move == match.last.hostMove) {
            enter(View::Result, now);
            return true;
        }
        return false;
    }
};

}  // namespace duel
