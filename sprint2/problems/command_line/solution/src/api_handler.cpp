#include "api_handler.h"

namespace http_handler {

ExtensionToConetntTypeMapper Sender::mapper_;

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

json::object MapToJSON(const model::Map* map) {
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

APIRequestHandler::APIRequestHandler(app::Application& app, net::io_context& ioc, bool no_auto_tick)
    : app_{ app },
    strand_(net::make_strand(ioc)),
    auto_tick_(!no_auto_tick){
}

json::array APIRequestHandler::ProcessMapsRequestBody() const {
    auto maps_body = json::array();
    for (const auto& map : app_.GetMaps()) {
        json::object body;
        body[std::string(model::ModelLiterals::ID)] = *map.GetId();
        body[std::string(model::ModelLiterals::NAME)] = map.GetName();
        maps_body.emplace_back(std::move(body));
    }
    return maps_body;
}

ExtensionToConetntTypeMapper::ExtensionToConetntTypeMapper() {
    map_[FileExtensions::HTML] = ContentType::TEXT_HTML;
    map_[FileExtensions::HTM] = ContentType::TEXT_HTML;
    map_[FileExtensions::JSON] = ContentType::APP_JSON;
    map_[FileExtensions::CSS] = ContentType::TEXT_CSS;
    map_[FileExtensions::TXT] = ContentType::TEXT_PLAIN;
    map_[FileExtensions::JS] = ContentType::TEXT_JAVASCRIPT;
    map_[FileExtensions::XML] = ContentType::APP_XML;
    map_[FileExtensions::PNG] = ContentType::PNG;
    map_[FileExtensions::JPEG] = ContentType::JPEG;
    map_[FileExtensions::JPG] = ContentType::JPEG;
    map_[FileExtensions::JPE] = ContentType::JPEG;
    map_[FileExtensions::GIF] = ContentType::GIF;
    map_[FileExtensions::BMP] = ContentType::BMP;
    map_[FileExtensions::ICO] = ContentType::ICO;
    map_[FileExtensions::TIFF] = ContentType::TIFF;
    map_[FileExtensions::TIF] = ContentType::TIFF;
    map_[FileExtensions::SVG] = ContentType::SVG;
    map_[FileExtensions::SVGZ] = ContentType::SVG;
    map_[FileExtensions::MP3] = ContentType::MP3;
}

std::string_view ExtensionToConetntTypeMapper::operator()(std::string_view extension) {
    if (map_.contains(extension)) {
        return map_.at(extension);
    }
    return ContentType::UNKNOWN;
}

bool APIRequestHandler::ParseBearer(const std::string_view auth_header, std::string& token_to_write) const {
    if (!auth_header.starts_with("Bearer ")) {
        return false;
    }
    std::string_view str = auth_header.substr(7);
    if (str.size() != 32) {
        return false;
    }
    token_to_write = str;
    return true;
}

}