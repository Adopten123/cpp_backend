#include "request_handler.h"

#include <ranges>

namespace http_handler {

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

json::object SerializeMap(const model::Map* map) {
    json::object obj;
    obj[std::string(model::ModelLiterals::ID)] = *map->GetId();
    obj[std::string(model::ModelLiterals::NAME)] = map->GetName();
    obj[std::string(model::ModelLiterals::ROADS)] = SerializeRoads(map);
    obj[std::string(model::ModelLiterals::BUILDINGS)] = SerializeBuildings(map);
    obj[std::string(model::ModelLiterals::OFFICES)] = SerializeOffices(map);
    return obj;
}

json::array SerializeRoads(const model::Map* map) {
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

json::array SerializeOffices(const model::Map* map) {
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

json::array SerializeBuildings(const model::Map* map) {
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

json::array RequestHandler::ProcessMapsRequestBody() const {
    auto maps_body = json::array();
    for (const auto& map : game_.GetMaps()) {
        json::object body;
        body[std::string(model::ModelLiterals::ID)] = *map.GetId();
        body[std::string(model::ModelLiterals::NAME)] = map.GetName();
        maps_body.emplace_back(std::move(body));
    }
    return maps_body;
}

RequestHandler::RequestType RequestHandler::CheckRequest(std::string_view target) const {
    auto request = SplitRequest(target.substr(1, target.length() - 1));

    if (request.size() > 2 and request[0] == RestApiLiterals::API
        and request[1] == RestApiLiterals::VERSION_1
        and request[2] == RestApiLiterals::MAPS) {

        if (request.size() == 3)
            return RequestHandler::RequestType::API_MAPS;
        else if (request.size() == 4)
            return RequestHandler::RequestType::API_MAP;
        else
            return RequestHandler::RequestType::BAD_REQUEST;
    }
    if (request[0] == RestApiLiterals::API) {
        return RequestHandler::RequestType::BAD_REQUEST;
    }
    auto temp_path = root_path_;
    temp_path += target;
    auto path = fs::weakly_canonical(temp_path);
    auto canonical_root = fs::weakly_canonical(root_path_);
    for (auto b = canonical_root.begin(), p = path.begin(); b != canonical_root.end(); ++b, ++p) {
        if (p == path.end() or *p != *b) {
            return RequestHandler::RequestType::BAD_REQUEST;
        }
    }
    return RequestHandler::RequestType::FILE;
}

std::string RequestHandler::URLDecode(std::string_view url) const {
    std::string result;
    result.reserve(url.size());
    for (size_t i = 0; i < url.size(); ++i) {
        if (url[i] == '%') {
            if (i + 2 < url.size()) {
                int hi = HexToInt(url[i+1]);
                int lo = HexToInt(url[i+2]);
                if (hi == -1 || lo == -1) {
                    result += url[i];
                } else {
                    result += static_cast<char>((hi << 4) | lo);
                    i += 2;
                }
            } else {
                result += url[i];
            }
        } else if (url[i] == '+') {
            result += ' ';
        } else {
            result += url[i];
        }
    }
    return result;
}

int RequestHandler::HexToInt(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    return -1;
}

std::string_view GetMapperType(std::string_view extension) {
    static const std::unordered_map<std::string_view, std::string_view> mime_types = {
        {"html", ContentType::TEXT_HTML}, {"htm", ContentType::TEXT_HTML}, {"json", ContentType::APP_JSON},
        {"css", ContentType::TEXT_CSS}, {"txt", ContentType::TEXT_PLAIN}, {"js", ContentType::TEXT_JAVASCRIPT},
        {"xml", ContentType::APP_XML}, {"png", ContentType::PNG}, {"jpeg", ContentType::JPEG}, {"jpg", ContentType::JPEG},
        {"jpe", ContentType::JPEG}, {"gif", ContentType::GIF}, {"bmp", ContentType::BMP}, {"ico", ContentType::ICO},
        {"tiff", ContentType::TIFF}, {"tif", ContentType::TIFF}, {"svg", ContentType::SVG}, {"svgz", ContentType::SVG},
        {"mp3", ContentType::MP3},
    };

    if (mime_types.contains(extension)) {
        return mime_types.at(extension);
    }
    return ContentType::UNKNOWN;
}

void LoggingRequestHandler::LogResponse(const RequestHandler::ResponseData& r, double response_time, const boost::beast::net::ip::address& address) {
    json::object response_data;
    response_data["ip"] = address.to_string();
    response_data["response_time"] = (int)(response_time * 1000);
    response_data["code"] = (int)r.code;
    response_data["content_type"] = r.content_type.data();
    BOOST_LOG_TRIVIAL(info) << logging::add_value(data, response_data) << "response sent";
}

}  // namespace http_handler