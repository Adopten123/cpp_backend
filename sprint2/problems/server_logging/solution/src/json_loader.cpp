#include "json_loader.h"
#include <fstream>
#include "boost_json.cpp"

namespace json_loader {

namespace json = boost::json;

model::Road ParseRoad(const json::value& road_json){
    int x0 = road_json.at("x0").as_int64();
    int y0 = road_json.at("y0").as_int64();

	if (road_json.as_object().contains("x1")) {
    	int x1 = road_json.at("x1").as_int64();
    	return model::Road(model::Road::HORIZONTAL, {x0,y0}, x1);
    } else {
   		int y1 = road_json.at("y1").as_int64();
        return model::Road(model::Road::VERTICAL, {x0,y0}, y1);
    }

}
model::Building ParseBuilding(const json::value& building_json) {
	int x = building_json.at("x").as_int64();;;
    int y = building_json.at("y").as_int64();;;
    int w = building_json.at("w").as_int64();;;
    int h = building_json.at("h").as_int64();;;

    return model::Building{model::Rectangle{{x, y}, {w, h}}};
}
model::Office ParseOffice(const json::value& office_json) {
	std::string id = office_json.at("id").as_string().c_str();
    int x = office_json.at("x").as_int64();;;
    int y = office_json.at("y").as_int64();;;
    int offsetX = office_json.at("offsetX").as_int64();;;
    int offsetY = office_json.at("offsetY").as_int64();;;

    return model::Office(model::Office::Id{id}, {x, y}, {offsetX, offsetY});
}

model::Game LoadGame(const std::filesystem::path& json_path) {
    // Загрузить содержимое файла json_path, например, в виде строки
    // Распарсить строку как JSON, используя boost::json::parse
    // Загрузить модель игры из файла
    std::ifstream file(json_path);
    if (!file.is_open())
      throw std::runtime_error("Config file not found");

    std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    json::value json_obj = json::parse(json_str);

    model::Game game;

    for (const auto& map_json : json_obj.at("maps").as_array()) {
        auto& map_obj = map_json.as_object();
    	std::string id = map_obj.at("id").as_string().c_str();
    	std::string name = map_obj.at("name").as_string().c_str();
    	model::Map map(model::Map::Id{id}, name);

        for (const auto& road_json : map_obj.at("roads").as_array()) {
          	model::Road road = ParseRoad(road_json);
        	map.AddRoad(road);
        }

        for (const auto& building_json : map_obj.at("buildings").as_array()) {
            model::Building building = ParseBuilding(building_json);
          	map.AddBuilding(building);
        }

        for (const auto& office_json : map_obj.at("offices").as_array()) {
          	model::Office office = ParseOffice(office_json);
          	map.AddOffice(office);
        }

        game.AddMap(std::move(map));
    }

    return game;
}

}  // namespace json_loader