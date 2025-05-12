#include "handler_utils.h"

namespace http_handler {

namespace beast = boost::beast;
namespace json = boost::json;

namespace utils {

std::vector<std::string_view> SplitRequest(std::string_view body) {
    std::vector<std::string_view> result;
    size_t start = 0;
    size_t end = body.find("/");
    while (end != std::string_view::npos) {
        result.push_back(body.substr(start, end - start));
        start = end + 1;
        end = body.find("/", start);
    }
    result.push_back(body.substr(start, body.length() - start));
    return result;
}

json::object MapToJson(const model::Map* map) {
    json::object obj;
    obj[std::string(model::ModelLiterals::ID)] = *map->GetId();
    obj[std::string(model::ModelLiterals::NAME)] = map->GetName();
    obj[std::string(model::ModelLiterals::ROADS)] = RoadsToJson(map);
    obj[std::string(model::ModelLiterals::BUILDINGS)] = BuildingsToJson(map);
    obj[std::string(model::ModelLiterals::OFFICES)] = OfficesToJson(map);
    return obj;
}

json::array RoadsToJson(const model::Map* map) {
    json::array roads;
    for (const auto& road : map->GetRoads()) {
        json::object json_road;
        json_road[std::string(model::ModelLiterals::START_X)] = road.GetStart().x;
        json_road[std::string(model::ModelLiterals::START_Y)] = road.GetStart().y;
        road.IsHorizontal() ? json_road[std::string(model::ModelLiterals::END_X)] = road.GetEnd().x
            : json_road[std::string(model::ModelLiterals::END_Y)] = road.GetEnd().y;
        roads.emplace_back(json_road);
    }
    return roads;
}

json::array OfficesToJson(const model::Map* map) {
    json::array offices;
    for (const auto& office : map->GetOffices()) {
        json::object json_office;
        json_office[std::string(model::ModelLiterals::ID)] = *office.GetId();
        json_office[std::string(model::ModelLiterals::POSITION_X)] = office.GetPosition().x;
        json_office[std::string(model::ModelLiterals::POSITION_Y)] = office.GetPosition().y;
        json_office[std::string(model::ModelLiterals::OFFSET_X)] = office.GetOffset().dx;
        json_office[std::string(model::ModelLiterals::OFFSET_Y)] = office.GetOffset().dy;
        offices.emplace_back(json_office);
    }
    return offices;
}

json::array BuildingsToJson(const model::Map* map) {
    json::array buildings;
    for (const auto& building : map->GetBuildings()) {
        json::object json_building;
        auto& bounds = building.GetBounds();
        json_building[std::string(model::ModelLiterals::POSITION_X)] = bounds.position.x;
        json_building[std::string(model::ModelLiterals::POSITION_Y)] = bounds.position.y;
        json_building[std::string(model::ModelLiterals::MODEL_SIZE_WIDTH)] = bounds.size.width;
        json_building[std::string(model::ModelLiterals::MODEL_SIZE_HEIGHT)] = bounds.size.height;
        buildings.emplace_back(json_building);
    }
    return buildings;
}

std::string_view GetMimeType(std::string_view extension) {

    static const std::unordered_map<std::string_view, std::string_view> mime_types = {
        {"html"sv, MimeType::TEXT_HTML},
        {"htm"sv, MimeType::TEXT_HTML},
        {"json"sv, MimeType::APP_JSON},
        {"css"sv, MimeType::TEXT_CSS},
        {"txt"sv, MimeType::TEXT_PLAIN},
        {"js"sv, MimeType::TEXT_JAVASCRIPT},
        {"xml"sv, MimeType::APP_XML},
        {"png"sv, MimeType::PNG},
        {"jpeg"sv, MimeType::JPEG},
        {"jpg"sv, MimeType::JPEG},
        {"jpe"sv, MimeType::JPEG},
        {"gif"sv, MimeType::GIF},
        {"bmp"sv, MimeType::BMP},
        {"ico"sv, MimeType::ICO},
        {"tiff"sv, MimeType::TIFF},
        {"tif"sv, MimeType::TIFF},
        {"svg"sv, MimeType::SVG},
        {"svgz"sv, MimeType::SVG},
        {"mp3"sv, MimeType::MP3},
    };

    if (mime_types.contains(extension)) {
        return mime_types.at(extension);
    }
    return MimeType::UNKNOWN;
}

}

}