#pragma once

#include <filesystem>

#include "boost_json.cpp"
#include "model.h"


namespace json_loader {

    namespace json = boost::json;

    model::Road ParseRoad(const json::value& road_json);
    model::Building ParseBuilding(const json::value& building_json);
    model::Office ParseOffice(const json::value& office_json);

    model::Game LoadGame(const std::filesystem::path& json_path);

}  // namespace json_loader