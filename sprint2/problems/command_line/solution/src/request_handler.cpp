#include "request_handler.h"

#include <ranges>

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

RequestHandler::RequestHandler(app::Application& app, const char* path_to_static, net::io_context& ioc, bool no_auto_tick)
    : root_path_(path_to_static),
    api_handler_(std::make_shared<APIRequestHandler>(app, ioc, no_auto_tick)){
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

RequestHandler::RequestType RequestHandler::CheckRequest(std::string_view target) const {
    if (target.starts_with(RestApiLiterals::API_V1)) {
        return RequestHandler::RequestType::API;
    }
    if (target.starts_with("/api")) {
        return RequestHandler::RequestType::BAD_REQUEST;
    }
    auto request = SplitRequest(target.substr(1, target.length() - 1));
    auto temp_path = root_path_;
    temp_path += target;
    auto path = fs::weakly_canonical(temp_path);
    auto canonical_root = fs::weakly_canonical(root_path_);
    for (auto b = canonical_root.begin(), p = path.begin(); b != canonical_root.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return RequestHandler::RequestType::BAD_REQUEST;
        }
    }
    return RequestHandler::RequestType::FILE;
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

std::string RequestHandler::DecodeURL(std::string_view url) const {
    std::vector<char> text;
    text.reserve(url.length());
    for (size_t i = 0; i < url.length(); ++i) {
        if (url[i] == '%') {
            if (i + 2 < url.length()) {
                char hex1 = url[i + 1];
                char hex2 = url[i + 2];

                if (isxdigit(hex1) && isxdigit(hex2)) {
                    int code = std::stoi(std::string() + hex1 + hex2, nullptr, 16);
                    text.emplace_back(static_cast<char>(code));
                    i += 2;
                }
                else {
                    text.emplace_back('%');
                }
            }
            else {
                text.emplace_back('%');
            }
        }
        else if (url[i] == '+') {
            text.emplace_back(' ');
        }
        else {
            text.emplace_back(url[i]);
        }
    }
    return std::string(text.data(), text.size());
}

void LoggingRequestHandler::LogResponse(const ResponseData& r, boost::chrono::system_clock::time_point start_time, const boost::beast::net::ip::address&& address) {
    boost::chrono::duration<double> response_time = boost::chrono::system_clock::now() - start_time;
    json::object response_data;
    response_data["ip"] = address.to_string();
    response_data["response_time"] = (int)(response_time.count() * 1000);
    response_data["code"] = (int)r.code;
    response_data["content_type"] = r.content_type.data();
    BOOST_LOG_TRIVIAL(info) << logging::add_value(data, response_data) << "response sent";
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

}  // namespace http_handler