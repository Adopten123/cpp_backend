#include "model.h"

#include <cmath>
#include <stdexcept>

namespace model {
using namespace std::literals;

static const double MAX_DELTA = 0.4;

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (std::exception& ex) {
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
        } catch (std::exception& ex) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

std::vector<const Dog*> GameSession::GetDogs() const {
    std::vector<const Dog*> result;
    for (const auto& dog : dogs_) {
        result.emplace_back(&dog);
    }
    return result;
}

Dog* GameSession::AddDog(Dog&& dog) {
    auto roads = map_->GetRoads();
    if (randomize_spawn_) {
        std::default_random_engine generator(roads.size());
        std::uniform_int_distribution rand_road{ 0, static_cast<int>(roads.size()) - 1 };
        auto& road = roads[rand_road(generator)];
        if (road.IsHorizontal()) {
            std::uniform_real_distribution rand_pos{ static_cast<double>(road.GetStart().x), static_cast<double>(road.GetEnd().x) };
            dog.SetPosition({rand_pos(generator), static_cast<double>(road.GetStart().y) });
        }
        else {
            std::uniform_real_distribution rand_pos{ static_cast<double>(road.GetStart().y), static_cast<double>(road.GetEnd().y) };
            dog.SetPosition({ static_cast<double>(road.GetStart().x), rand_pos(generator) });
        }
    }
    else {
        const auto& road_start = roads[0].GetStart();
        dog.SetPosition({ static_cast<double>(road_start.x), static_cast<double>(road_start.y) });
    }
    dog.ResetDirection();
    dog.Stop();
    dogs_.push_front(std::move(dog));
    return &dogs_.front();
}

GameSession::GameSession(Map* map, bool randomize_spawn)
        : map_(map),
        randomize_spawn_(randomize_spawn)
{
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
            min_x = static_cast<double>(start_point.x) - MAX_DELTA;
            max_x = static_cast<double>(end_point.x) + MAX_DELTA;
        }
        else {
            min_x = static_cast<double>(end_point.x) - MAX_DELTA;
            max_x = static_cast<double>(start_point.x) + MAX_DELTA;
        }
        min_y = static_cast<double>(start_point.y) - MAX_DELTA;
        max_y = static_cast<double>(start_point.y) + MAX_DELTA;
    }
    else {
        if (start_point.y < end_point.y) {
            min_y = static_cast<double>(start_point.y) - MAX_DELTA;
            max_y = static_cast<double>(end_point.y) + MAX_DELTA;
        }
        else {
            min_y = static_cast<double>(end_point.y) - MAX_DELTA;
            max_y = static_cast<double>(start_point.y) + MAX_DELTA;
        }
        min_x = static_cast<double>(start_point.x) - MAX_DELTA;
        max_x = static_cast<double>(start_point.x) + MAX_DELTA;
    }
    if (pos.x <= max_x && pos.x >= min_x && pos.y <= max_y && pos.y >= min_y) {
        return true;
    }
    return false;
}

double GetMaxPossible(const Road* road, Direction dir) {
    switch (dir) {
    case Direction::NORTH:
        return static_cast<double>(std::min(road->GetEnd().y, road->GetStart().y)) - MAX_DELTA;
    case Direction::SOUTH:
        return static_cast<double>(std::max(road->GetEnd().y, road->GetStart().y)) + MAX_DELTA;
    case Direction::EAST:
        return static_cast<double>(std::max(road->GetEnd().x, road->GetStart().x)) + MAX_DELTA;
    case Direction::WEST:
        return static_cast<double>(std::min(road->GetEnd().x, road->GetStart().x)) - MAX_DELTA;
    }
    return 0.;
}

std::pair<bool, Position> GameSession::CalculateMove(Position pos, Speed speed, unsigned delta) const {
    double in_seconds = static_cast<double>(delta) / 1000;
    Position end_pos = {pos.x + speed.vx * in_seconds, pos.y + speed.vy * in_seconds };
    Point rounded = { static_cast<int>(std::round(pos.x)), static_cast<int>(std::round(pos.y)) };
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
        end_pos = {pos.x, speed.vy > 0 ? std::round(pos.y) + MAX_DELTA : std::round(pos.y) - MAX_DELTA };
        return {true, end_pos};
    }
    end_pos = { speed.vx > 0 ? std::round(pos.x) + MAX_DELTA : std::round(pos.x) - MAX_DELTA, pos.y };
    return { true, end_pos };
}

}  // namespace model