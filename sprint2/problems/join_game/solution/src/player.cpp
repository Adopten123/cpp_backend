#include "player.h"

namespace app {

Player& Players::AddPlayer(model::Dog&& dog, model::GameSession* session) {
    auto* dog = session->AddDog(std::move(dog));
    auto token = token_.GetToken();

    while (tokens_by_players_.contains(token))
        token = token_.GetToken();

    Player player(token, session, dog);
    players_.emplace_back(std::move(player));
    tokens_by_players_.emplace(token,  players_.size() - 1);
    return players_.back();
}

const Player* Players::FindByToken(const Token& token) const {
    if (auto player = tokens_by_players_.find(token);
        player != tokens_by_players_.end()
    )
        return &players_.at(player->second);

    return nullptr;
}

}