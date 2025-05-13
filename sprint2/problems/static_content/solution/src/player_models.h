#pragma once

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
};

class Player {
public:
    explicit Player(Token token, model::GameSession* session, model::Dog* dog)
        : id_(GetNextId())
        , session_(session)
        , dog_(dog)
        , token_(std::move(token)) {
    }

    Token GetToken() const {
        return token_;
    }

    int GetId() const noexcept {
        return id_;
    }

    const model::GameSession* GetSession() const noexcept {
        return session_;
    }

    model::Dog* GetDog() noexcept {
        return dog_;
    }

private:
    inline static int start_id_ = 0;

    static int GetNextId() {
        return start_id_++;
    }

    int id_;
    model::GameSession* session_;
    model::Dog* dog_;
    Token token_;

};

class Players {
public:
    Player& AddPlayer(model::Dog&& dog, model::GameSession* session);

    Player* FindByToken(const Token& token);

private:
    using TokenHasher = util::TaggedHasher<Token>;
    using TokensByPlayers = std::unordered_map<Token, size_t, TokenHasher>;

    std::vector<Player> players_;
    TokensByPlayers tokens_by_players_;
    PlayerToken token_gen_;
};

class Application {
public:
    explicit Application(model::Game&& game, bool randomize_spawn) :game_(std::move(game)) {
        game_.StartSessions(randomize_spawn);
    }

    const model::Map* FindMap(model::Map::Id id) const {
        return game_.FindMap(id);
    }

    const model::Game::Maps& GetMaps() const {
        return game_.GetMaps();
    }

    model::GameSession* FindSession(const model::Map::Id& id) {
        return game_.FindSession(id);
    }

    Player& AddPlayer(model::Dog&& dog, model::GameSession* session) {
        return players_.AddPlayer(std::move(dog), session);
    }

    Player* FindByToken(const Token& token) {
        return players_.FindByToken(token);
    }

    std::vector<const model::Dog*> GetDogs(const Player* player) const {
        return player->GetSession()->GetDogs();
    }

    void Move(Player* player, model::Direction dir) {
        player->GetDog()->Move(dir, player->GetSession()->GetSpeed());
    }

    void Stop(Player* player) {
        player->GetDog()->Stop();
    }

    void Tick(unsigned millisec) {
        game_.Tick(millisec);
    }

private:
    model::Game game_;
    Players players_;
};

}  // namespace app