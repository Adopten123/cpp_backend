#pragma once
#include <compare>
#include <forward_list>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "tagged.h"

namespace model {

using namespace std::literals;
using Dimension = int;
using Coord = Dimension;

struct ModelLiterals {
    ModelLiterals() = delete;
    constexpr static std::string_view DEFAULT_DOG_SPEED = "defaultDogSpeed"sv;
    constexpr static std::string_view DOG_SPEED = "dogSpeed"sv;
    constexpr static std::string_view MAPS = "maps"sv;
    constexpr static std::string_view ID = "id"sv;
    constexpr static std::string_view NAME = "name"sv;
    constexpr static std::string_view START_X = "x0"sv;
    constexpr static std::string_view START_Y = "y0"sv;
    constexpr static std::string_view END_X = "x1"sv;
    constexpr static std::string_view END_Y = "y1"sv;
    constexpr static std::string_view POSITION_X = "x"sv;
    constexpr static std::string_view POSITION_Y = "y"sv;
    constexpr static std::string_view OFFSET_X = "offsetX"sv;
    constexpr static std::string_view OFFSET_Y = "offsetY"sv;
    constexpr static std::string_view MODEL_SIZE_WIDTH = "w"sv;
    constexpr static std::string_view MODEL_SIZE_HEIGHT = "h"sv;
    constexpr static std::string_view ROADS = "roads"sv;
    constexpr static std::string_view OFFICES = "offices"sv;
    constexpr static std::string_view BUILDINGS = "buildings"sv;
};

struct Position {
    double x, y;
    auto operator<=>(const Position&) const = default;
};

struct Speed {
    double vx, vy;
    auto operator<=>(const Speed&) const = default;
};

enum Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};

struct Point {
    Coord x, y;
    bool operator==(const Point& other) const = default;
};

struct PointHash {
    size_t operator()(const Point& p) const {
        return std::hash<int>{}(p.x) ^ (std::hash<int>{}(p.y) << 1);
    }
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

class Road {
    struct HorizontalTag {
        HorizontalTag() = default;
    };

    struct VerticalTag {
        VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept { return start_.y == end_.y; }
    bool IsVertical() const noexcept { return start_.x == end_.x; }

    Point GetStart() const noexcept { return start_; }
    Point GetEnd() const noexcept { return end_; }
private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    //--------------------getters--------------------------------------
    const Rectangle& GetBounds() const noexcept { return bounds_; }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    //--------------------getters--------------------------------------
    const Id& GetId() const noexcept { return id_; }
    Point GetPosition() const noexcept { return position_; }
    Offset GetOffset() const noexcept { return offset_; }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    //--------------------getters--------------------------------------
    const Id& GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return name_; }
    const Buildings& GetBuildings() const noexcept { return buildings_; }
    const Roads& GetRoads() const noexcept { return roads_; }
    const Offices& GetOffices() const noexcept { return offices_; }

    void SetSpeed(double speed) { speed_ = speed; }
    double GetSpeed() { return speed_; }

    void AddRoad(const Road& road) { roads_.emplace_back(road); }
    void AddBuilding(const Building& building) { buildings_.emplace_back(building); }
    void AddOffice(Office office);

private:
    using OfficeIdByIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    double speed_;

    OfficeIdByIndex warehouse_id_by_index_;
    Offices offices_;
};

class Dog {
public:
    explicit Dog(std::string&& name)
      : id_(GetNextId())
      , name_(std::move(name)) {
    }

    //--------------------getters--------------------------------------
    int GetId() const { return id_; }
    const std::string& GetName() const { return name_; }

    Position GetPosition() const { return position_; }
    Direction GetDirection() const { return direction_; }
    Speed GetSpeed() const { return speed_; }

    void SetPosition(Position pos) { position_ = pos; }

    void Move(Direction dir, double speed) {
        direction_ = dir;
        switch (dir) {
        case Direction::NORTH:
            speed_ = {0., -speed};
            return;
        case Direction::SOUTH:
            speed_ = {0., speed};
            return;
        case Direction::WEST:
            speed_ = {-speed, 0.};
            return;
        case Direction::EAST:
            speed_ = {speed, 0.};
            return;
        }
    }

    void Stop() {
        speed_ = {};
    }

    void ResetDirection() {
        direction_ = Direction::NORTH;
    }

private:
    inline static int start_id_ = 0;

    static int GetNextId() {
        return start_id_++;
    }

    const int id_;
    std::string name_;
    Position position_ = {0., 0.};
    Speed speed_ = {0., 0.};
    Direction direction_ = Direction::NORTH;
};

class GameSession {
public:
    explicit GameSession(Map* map, bool randomize_spawn);

    std::vector<const Dog*> GetDogs() const;
    Dog* AddDog(Dog&& dog);

    double GetSpeed() const { return map_->GetSpeed(); }

    void Tick(unsigned delta) {
        for (auto& dog : dogs_) {
            if (dog.GetSpeed() != Speed{}) {
                auto [stop, new_pos] = CalculateMove(dog.GetPosition(), dog.GetSpeed(), delta);
                dog.SetPosition(new_pos);
                if (stop)
                    dog.Stop();
            }
        }
    }

private:
    std::forward_list<Dog> dogs_;
    Map* map_;
    std::unordered_map<Point, std::vector<const Road*>, PointHash> roads_graph_;
    bool randomize_spawn_;

    std::pair<bool, Position> CalculateMove(Position pos, Speed speed, unsigned delta) const;
};

class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept { return maps_; }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_by_index_.find(id); it != map_id_by_index_.end())
            return &maps_.at(it->second);
        return nullptr;
    }

    GameSession* FindSession(const Map::Id& id) {
        if (auto it = map_id_by_index_.find(id); it != map_id_by_index_.end())
            return &sessions_.at(it->second);
        return nullptr;
    }

    void StartSessions(bool randomize_spawn) {
        sessions_.clear();
        sessions_.reserve(maps_.size());
        for (auto& map : maps_)
            sessions_.push_back(GameSession{ &map, randomize_spawn });
    }

    void Tick(unsigned delta) {
        for (auto& session : sessions_)
            session.Tick(delta);
    }

private:
    using Hasher = util::TaggedHasher<Map::Id>;
    using MapIdByIndex = std::unordered_map<Map::Id, size_t, Hasher>;

    std::vector<Map> maps_;
    MapIdByIndex map_id_by_index_;
    std::vector<GameSession> sessions_;
};

}  // namespace model