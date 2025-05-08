#include "model.h"

#include <cmath>
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
    //result.reserve(dogs_.size()-1);
    for (const auto& dog : dogs_) {
        result.emplace_back(&dog);
    }
    return result;
}

Dog* GameSession::AddDog(Dog&& dog) {
    auto roads = map_->GetRoads();
    /*std::default_random_engine generator(roads.size());
    std::uniform_int_distribution rand_road{0, (int)roads.size()-1};
    auto& road = roads[rand_road(generator)];
    if (road.IsHorizontal()) {
        std::uniform_real_distribution rand_pos{ (double)road.GetStart().x, (double)road.GetEnd().x };
        dog.SetPosition({rand_pos(generator), (double)road.GetStart().y });
    }
    else {
        std::uniform_real_distribution rand_pos{ (double)road.GetStart().y, (double)road.GetEnd().y };
        dog.SetPosition({ (double)road.GetStart().x, rand_pos(generator) });
    }*/
    //temp
    const auto& road_start = roads[0].GetStart();
    dog.SetPosition({(double)road_start.x, (double)road_start.y});
    //temp
    dog.ResetDirection();
    dog.Stop();
    dogs_.push_front(std::move(dog));
    return &dogs_.front();
}

int Dog::start_id_ = 0;

GameSession::GameSession(Map* map) : map_(map) {
    for (const auto& road : map_->GetRoads()) {
        auto start = road.GetStart();
        auto end = road.GetEnd();
        if (road.IsHorizontal()) {
            int start_x, end_x;
            if (start.x < end.x) {
                start_x = start.x;
                end_x = end.x;
            }
            else {
                start_x = end.x;
                end_x = start.x;
            }
            for (; start_x <= end_x; ++start_x) {
                roads_graph_[{start_x, start.y}].push_back(&road);
            }
        }
        else {
            int start_y, end_y;
            if (start.y < end.y) {
                start_y = start.y;
                end_y = end.y;
            }
            else {
                start_y = end.y;
                end_y = start.y;
            }
            for (; start_y <= end_y; ++start_y) {
                roads_graph_[{start.x, start_y}].push_back(&road);
            }
        }
    }
}

bool InBounds(const Road* road, Position pos) {
    auto start_point = road->GetStart();
    auto end_point = road->GetEnd();
    double max_x;
    double min_x;
    double max_y;
    double min_y;
    if (road->IsHorizontal()) {
        if (start_point.x < end_point.x) {
            min_x = (double)start_point.x - 0.4;
            max_x = (double)end_point.x + 0.4;
        }
        else {
            min_x = (double)end_point.x - 0.4;
            max_x = (double)start_point.x + 0.4;
        }
        min_y = (double)start_point.y - 0.4;
        max_y = (double)start_point.y + 0.4;
    }
    else {
        if (start_point.y < end_point.y) {
            min_y = (double)start_point.y - 0.4;
            max_y = (double)end_point.y + 0.4;
        }
        else {
            min_y = (double)end_point.y - 0.4;
            max_y = (double)start_point.y + 0.4;
        }
        min_x = (double)start_point.x - 0.4;
        max_x = (double)start_point.x + 0.4;
    }
    if (pos.x <= max_x && pos.x >= min_x && pos.y <= max_y && pos.y >= min_y) {
        return true;
    }
    return false;
}

double GetMaxPossible(const Road* road, Direction dir) {
    switch (dir) {
    case Direction::NORTH:
        return (double)std::min(road->GetEnd().y, road->GetStart().y) - 0.4;
    case Direction::SOUTH:
        return (double)std::max(road->GetEnd().y, road->GetStart().y) + 0.4;
    case Direction::EAST:
        return (double)std::max(road->GetEnd().x, road->GetStart().x) + 0.4;
    case Direction::WEST:
        return (double)std::min(road->GetEnd().x, road->GetStart().x) - 0.4;
    }
    return 0.;
}

std::pair<bool, Position> GameSession::CalculateMove(Position pos, Speed speed, int delta) const {
    double in_seconds = (double)delta / 1000;
    Position end_pos = {pos.x + speed.vx * in_seconds, pos.y + speed.vy * in_seconds };
    Point rounded = {(int)std::round(pos.x), (int)std::round(pos.y)};
    for (const auto& road : roads_graph_.at(rounded)) {
        if (InBounds(road, end_pos)) {
            return {false, end_pos};
        }
        if (road->IsHorizontal() && speed.vy == 0.) {
            if (speed.vx > 0) {
                return {true, {GetMaxPossible(road, Direction::EAST), pos.y}};
            }
            return {true, {GetMaxPossible(road, Direction::WEST), pos.y}};
        }
        if (road->IsVertical() && speed.vx == 0.) {
            if (speed.vy > 0) {
                return {true, {pos.x, GetMaxPossible(road, Direction::SOUTH)}};
            }
            return {true, {pos.x, GetMaxPossible(road, Direction::NORTH)}};
        }
    }
    if (speed.vx == 0) {
        end_pos = {pos.x, speed.vy > 0 ? std::round(pos.y) + 0.4 : std::round(pos.y) - 0.4 };
        return {true, end_pos};
    }
    end_pos = { speed.vx > 0 ? std::round(pos.x) + 0.4 : std::round(pos.x) - 0.4, pos.y };
    return { true, end_pos };
}

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

Player& Players::AddPlayer(model::Dog&& dog, model::GameSession* session) {
    auto* doggy = session->AddDog(std::move(dog));
    auto token = token_gen_.GetToken();
    while (tokens_to_players_.contains(token)) {
        token = token_gen_.GetToken();
    }
    Player player(token, session, doggy);
    players_.push_back(std::move(player));
    tokens_to_players_.emplace(token, players_.size() - 1);
    return players_.back();
}

Player* Players::FindByToken(const Token& token) {
    if (auto player = tokens_to_players_.find(token); player != tokens_to_players_.end()) {
        return &players_.at(player->second);
    }
    return nullptr;
}

int Player::start_id_ = 0;

}  // namespace app