#include "player_models.h"

namespace app {

    Player& Players::AddPlayer(model::Dog&& dog, model::GameSession* session) {
        auto* dog_ptr = session->AddDog(std::move(dog));

        Token token = token_gen_.GetToken();
        while (tokens_by_players_.contains(token))
            token = token_gen_.GetToken();

        players_.emplace_back(std::move(token), session, dog_ptr);
        tokens_by_players_.emplace(players_.back().GetToken(), players_.size() - 1);

        return players_.back();
    }

    Player* Players::FindByToken(const Token& token) {
        if (const auto it = tokens_by_players_.find(token);
            it != tokens_by_players_.end()) {
            return &players_.at(it->second);
        }
        return nullptr;
    }

}  // namespace app