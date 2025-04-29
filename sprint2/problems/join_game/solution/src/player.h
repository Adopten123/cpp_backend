#pragma once

#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "model.h"

namespace detail {
    struct TokenTag {};
}  // namespace detail

namespace app {
using Token = util::Tagged<std::string, detail::TokenTag>;

class PlayerToken {
public:
    Token GetToken();
private:
    std::random_device random_device_;
    std::mt19937_64 generator1_{ [this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }() };
    std::mt19937_64 generator2_{ [this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }() };
}

class Player {
public:
    explicit Player(const Token& token, model::GameSession* session, model::Dog* dog)
        : id_(GetNextId())
        , token_(token)
        , session_(session)
        , dog_(dog) {}

    Token GetToken() const { return token_; }
    int GetId() const noexcept { return id_; }

    const model::GameSession* GetSession() const noexcept { return session_; }
    const model::Dog* GetDog() const noexcept { return dog_; }

private:
    static int id_counter_;
    static int GetNextId() noexcept {
        return id_counter_++;
    }

    int id_;
    model::GameSession* session_;
    model::Dog* dog_;
    Token token_;
}

class Players {
public:
    Player& AddPlayer(model::Dog&& dog, model::GameSession* session);
    const Player* FindByToken(const Token& token) const;
private:
    using TokenHasher = util::TaggedHasher<Token>;
    using TokensByPlayers = std::unordered_map<Token, size_t, TokenHasher>;

    std::vector<Player> players_;
    TokensByPlayers tokens_by_players_;
    PlayerToken token_;

}
} // namespace app