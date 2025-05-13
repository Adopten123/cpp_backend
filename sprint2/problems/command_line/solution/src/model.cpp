#include "model.h"

#include <cmath>
#include <stdexcept>

namespace model {
using namespace std::literals;

static const double MAX_DELTA = 0.4;

void Map::AddOffice(Office office) {
    if (warehouse_id_by_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_by_index_.emplace(o.GetId(), index);
    } catch (std::exception& ex) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_by_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (std::exception& ex) {
            map_id_by_index_.erase(it);
            throw;
        }
    }
}

std::vector<const Dog*> GameSession::GetDogs() const {
    std::vector<const Dog*> result;

    for (const auto& dog : dogs_)
        result.emplace_back(&dog);
    return result;
}

Dog* GameSession::AddDog(Dog&& dog) {
    auto roads = map_->GetRoads();

    if (randomize_spawn_) {
        std::default_random_engine generator(roads.size());
        std::uniform_int_distribution rand_road{ 0, static_cast<int>(roads.size()) - 1 };

        auto& road = roads[rand_road(generator)];

        auto set_position = [&](const model::Road& road) {
            if (road.IsHorizontal()) {
                std::uniform_real_distribution rand_pos {
                    static_cast<double>(road.GetStart().x),
                    static_cast<double>(road.GetEnd().x)
                };
                dog.SetPosition({ rand_pos(generator), static_cast<double>(road.GetStart().y) });
            } else {
                std::uniform_real_distribution rand_pos {
                    static_cast<double>(road.GetStart().y),
                    static_cast<double>(road.GetEnd().y)
                };
                dog.SetPosition({ static_cast<double>(road.GetStart().x), rand_pos(generator) });
            }
        };

        set_position(road);
    } else {
        const auto& road_start = roads[0].GetStart();
        dog.SetPosition({ static_cast<double>(road_start.x), static_cast<double>(road_start.y) });
    }

    dog.ResetDirection();
    dog.Stop();
    dogs_.push_front(std::move(dog));
    return &dogs_.front();
}

GameSession::GameSession(Map* map, bool randomize_spawn)
    : map_(map)
    , randomize_spawn_(randomize_spawn)
{
    for (const auto& road : map_->GetRoads()) {
        auto start = road.GetStart();
        auto end = road.GetEnd();

        if (road.IsHorizontal()) {
            int start_x = std::min(start.x, end.x);
            int end_x = std::max(start.x, end.x);

            for (int x = start_x; x <= end_x; ++x)
                roads_graph_[{x, start.y}].push_back(&road);
        } else {
            int start_y = std::min(start.y, end.y);
            int end_y = std::max(start.y, end.y);

            for (int y = start_y; y <= end_y; ++y)
                roads_graph_[{start.x, y}].push_back(&road);
        }
    }
}

bool IsPositionNearRoad(const Road* road, Position pos) {
    auto start_point = road->GetStart();
    auto end_point = road->GetEnd();

    double min_x, max_x;
    double min_y, max_y;

    if (road->IsHorizontal()) {
        min_x = std::min(start_point.x, end_point.x) - MAX_DELTA;
        max_x = std::max(start_point.x, end_point.x) + MAX_DELTA;
        min_y = max_y = static_cast<double>(start_point.y);
        min_y -= MAX_DELTA;
        max_y += MAX_DELTA;
    } else {
        min_y = std::min(start_point.y, end_point.y) - MAX_DELTA;
        max_y = std::max(start_point.y, end_point.y) + MAX_DELTA;
        min_x = max_x = static_cast<double>(start_point.x);
        min_x -= MAX_DELTA;
        max_x += MAX_DELTA;
    }

    return pos.x >= min_x && pos.x <= max_x &&
           pos.y >= min_y && pos.y <= max_y;
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
    double in_seconds = static_cast<double>(delta) / 1000.0;

    Position end_pos = {
        pos.x + speed.vx * in_seconds,
        pos.y + speed.vy * in_seconds
    };

    Point rounded = {
        static_cast<int>(std::round(pos.x)),
        static_cast<int>(std::round(pos.y))
    };

    const auto& roads = roads_graph_.at(rounded);

    for (const auto* road : roads) {
        if (IsPositionNearRoad(road, end_pos))
            return {false, end_pos};

        if (road->IsHorizontal() && speed.vy == 0.0) {
            double max_x = (speed.vx > 0)
                ? GetMaxPossible(road, Direction::EAST)
                : GetMaxPossible(road, Direction::WEST);
            return {true, {max_x, pos.y}};
        }

        if (road->IsVertical() && speed.vx == 0.0) {
            double max_y = (speed.vy > 0)
                ? GetMaxPossible(road, Direction::SOUTH)
                : GetMaxPossible(road, Direction::NORTH);
            return {true, {pos.x, max_y}};
        }
    }

    if (speed.vx == 0.0) {
        double y_limit = std::round(pos.y) + (speed.vy > 0 ? MAX_DELTA : -MAX_DELTA);
        return {true, {pos.x, y_limit}};
    } else {
        double x_limit = std::round(pos.x) + (speed.vx > 0 ? MAX_DELTA : -MAX_DELTA);
        return {true, {x_limit, pos.y}};
    }
}

}  // namespace model