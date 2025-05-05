#include "model.h"

#include <stdexcept>

namespace model {
using namespace std::literals;

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

std::vector<const Dog*> GameSession::GetDogs() const {
    std::vector<const Dog*> result;
    result.reserve(dogs_.size()-1);
    for (const auto& dog : dogs_) {
        result.emplace_back(&dog);
    }
    return result;
}

Dog* GameSession::AddDog(Dog&& dog) {
    auto roads = map_->GetRoads();
    std::default_random_engine generator(roads.size());
    std::uniform_int_distribution rand_road{0, (int)roads.size()-1};
    auto& road = roads[rand_road(generator)];
    if (road.IsHorizontal()) {
        std::uniform_real_distribution rand_pos{ (double)road.GetStart().x, (double)road.GetEnd().x };
        dog.SetPosition({rand_pos(generator), (double)road.GetStart().y });
    }
    else {
        std::uniform_real_distribution rand_pos{ (double)road.GetStart().y, (double)road.GetEnd().y };
        dog.SetPosition({ (double)road.GetStart().x, rand_pos(generator) });
    }
    dog.SetDirection(Direction::NORTH);
    dog.SetSpeed({});
    dogs_.emplace_back(std::move(dog));
    return &dogs_.back();
}

int Dog::start_id_ = 0;

}  // namespace model

namespace app {

Token TokensGen::GetToken() {
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << generator1_() << std::setw(16) << generator2_();
    return Token(ss.str());
}

Player::Player(Token token, model::GameSession* session, model::Dog* dog)
    : id_(GetNextId()), session_(session), dog_(dog), token_(std::move(token)) {
}

//Player::Player(int id, Token&& token, model::GameSession* session, model::Dog* dog)
//    : id_(id), session_(session), dog_(dog), token_(std::move(token)) {
//}

Player& Players::AddPlayer(model::Dog&& dog, model::GameSession* session) {
    auto* doggy = session->AddDog(std::move(dog));
    auto token = token_gen_.GetToken();
    while (tokens_to_players_.contains(token)) {
        token = token_gen_.GetToken();
    }
    Player player(token, session, doggy);
    players_.push_back(std::move(player));
    //players_.push_back(factory_.Create(std::move(token), session, doggy));
    tokens_to_players_.emplace(token, players_.size() - 1);
    return players_.back();
}

const Player* Players::FindByToken(const Token& token) const {
    if (auto player = tokens_to_players_.find(token); player != tokens_to_players_.end()) {
        return &players_.at(player->second);
    }
    return nullptr;
}

int Player::start_id_ = 0;

}  // namespace app