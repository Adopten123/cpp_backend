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

//----------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------RequestHandler---------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

RequestHandler::RequestType RequestHandler::CheckRequest(std::string_view target) const {
    auto parts = SplitRequest(target.substr(1, target.length() - 1));
    if (parts.size() >= 3
        and parts[1] == RestApiLiterals::VERSION_1
        and parts[2] == RestApiLiterals::MAPS
        )
    {
            if (parts.size() == 3)
                return RequestHandler::RequestType::API_MAPS;
            else if (parts.size() == 4)
                return RequestHandler::RequestType::API_MAP;
            else
                return RequestHandler::RequestType::BAD_REQUEST;
    }

    if (parts[0] == RestApiLiterals::API)
        return RequestHandler::RequestType::BAD_REQUEST;

    try {
        auto temp_path = root_path_;
        temp_path += target;
        auto path = fs::weakly_canonical(temp_path);
        auto canonical_root = fs::weakly_canonical(root_path_);
        for (auto root_it = canonical_root.begin(), path_it = path.begin(); root_it != canonical_root.end(); ++root_it, ++path_it) {
            if (path_it == path.end() || *path_it != *root_it) {
                return RequestHandler::RequestType::BAD_REQUEST;
            }
        }
        return RequestHandler::RequestType::FILE;
    }
    catch (...) {
        return RequestType::BAD_REQUEST;
    }
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
//----------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------RequestHandler---------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------------------------
//--------------------------------------------HttpResponseFactory-------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

std::string_view HttpResponseFactory::GetMimeType(std::string_view extension) {
    static const std::unordered_map<std::string_view, std::string_view> mime_types = {
        {"html", MimeType::TEXT_HTML}, {"htm", MimeType::TEXT_HTML}, {"json", MimeType::APP_JSON},
        {"css", MimeType::TEXT_CSS}, {"txt", MimeType::TEXT_PLAIN}, {"js", MimeType::TEXT_JAVASCRIPT},
        {"xml", MimeType::APP_XML}, {"png", MimeType::PNG}, {"jpeg", MimeType::JPEG}, {"jpg", MimeType::JPEG},
        {"jpe", MimeType::JPEG}, {"gif", MimeType::GIF}, {"bmp", MimeType::BMP}, {"ico", MimeType::ICO},
        {"tiff", MimeType::TIFF}, {"tif", MimeType::TIFF}, {"svg", MimeType::SVG}, {"svgz", MimeType::SVG},
        {"mp3", MimeType::MP3},
    };

    if (mime_types.contains(extension)) {
        return mime_types.at(extension);
    }
    return MimeType::UNKNOWN;
}

//----------------------------------------------------------------------------------------------------------------------
//--------------------------------------------HttpResponseFactory-------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
//--------------------------------------------LoggingRequestHandler-----------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
void LoggingRequestHandler::LogResponse(const ResponseData& r,
                                        double response_time,
                                        const boost::beast::net::ip::address& address)
{
    json::object response_data;
    response_data["ip"] = address.to_string();
    response_data["response_time"] = (int)(response_time * 1000);
    response_data["code"] = (int)r.code;
    response_data["content_type"] = r.content_type.data();
    BOOST_LOG_TRIVIAL(info) << logging::add_value(data, response_data) << "response sent";
}
//----------------------------------------------------------------------------------------------------------------------
//--------------------------------------------LoggingRequestHandler-----------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
//--------------------------------------------PlayerSessionAPIHandler---------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
json::array PlayerSessionAPIHandler::ProcessMapsRequestBody() const {
    auto maps_body = json::array();
    for (const auto& map : game_.GetMaps()) {
        json::object body;
        body[std::string(model::ModelLiterals::ID)] = *map.GetId();
        body[std::string(model::ModelLiterals::NAME)] = map.GetName();
        maps_body.emplace_back(std::move(body));
    }
    return maps_body;
}

bool PlayerSessionAPIHandler::ExtractBearerTokenFromHeader(
    const std::string_view auth_header,
    std::string& token_to_write) const {

    if (!auth_header.starts_with("Bearer "))
        return false;

    std::string_view str = auth_header.substr(7);

    if (str.size() != 32)
        return false;
    token_to_write = str;
    return true;
}
//----------------------------------------------------------------------------------------------------------------------
//--------------------------------------------PlayerSessionAPIHandler---------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

}  // namespace http_handler