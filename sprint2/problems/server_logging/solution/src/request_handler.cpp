#include "request_handler.h"
#include <boost/beast/http.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace http_handler {

void RequestHandler::HandleMaps(const http::request<http::string_body>& req,
                            http::response<http::string_body>& res) {
    res.result(http::status::ok);
    res.body() = json::serialize(SerializeMaps());
    res.set(http::field::content_type, "application/json");
}

void RequestHandler::HandleMap(const std::string& id, http::response<http::string_body>& res) {
    const auto* map = game_.FindMap(model::Map::Id{id});
    if (!map) {
        HandleNotFound(res, "Map not found", true);
        return;
    }
    res.result(http::status::ok);
    res.body() = json::serialize(SerializeMap(*map));
    res.set(http::field::content_type, "application/json");
}

void RequestHandler::HandleBadRequest(http::response<http::string_body>& res, const std::string& message) {
    res.result(http::status::bad_request);
    res.set(http::field::content_type, "application/json");
    res.body() = json::serialize(json::value{
        {"code", "badRequest"},
        {"message", message}
    });
}

void RequestHandler::HandleNotFound(http::response<http::string_body>& res, const std::string& message,
                                    bool is_api_request) {
    res.result(http::status::not_found);
    if (is_api_request) {
        res.set(http::field::content_type, "application/json");
        res.body() = json::serialize(json::value{
            {"code", "mapNotFound"},
            {"message", message}
        });
    } else {
        res.set(http::field::content_type, "text/plain");
        res.body() = message;
    }
}

    json::value RequestHandler::SerializeMap(const model::Map& map) {
    json::object result;
    result["id"] = *map.GetId();
    result["name"] = map.GetName();


    json::array roads;
    for (const auto& road : map.GetRoads()) {
        json::object obj;
        if (road.IsHorizontal()) {
            obj.emplace("x0", road.GetStart().x);
            obj.emplace("y0", road.GetStart().y);
            obj.emplace("x1", road.GetEnd().x);
        } else {
            obj.emplace("x0", road.GetStart().x);
            obj.emplace("y0", road.GetStart().y);
            obj.emplace("y1", road.GetEnd().y);
        }
        roads.push_back(obj);
    }
    result["roads"] = roads;

    json::array buildings;
    for (const auto& building : map.GetBuildings()) {
        auto bounds = building.GetBounds();
        buildings.push_back(json::value{
            {"x", bounds.position.x},
            {"y", bounds.position.y},
            {"w", bounds.size.width},
            {"h", bounds.size.height}
        });
    }
    result["buildings"] = buildings;

    json::array offices;
    for (const auto& office : map.GetOffices()) {
        offices.push_back(json::value{
            {"id", *office.GetId()},
            {"x", office.GetPosition().x},
            {"y", office.GetPosition().y},
            {"offsetX", office.GetOffset().dx},
            {"offsetY", office.GetOffset().dy}
        });
    }
    result["offices"] = offices;

    return result;
}

json::value RequestHandler::SerializeMaps() {
    json::array maps_array;
    for (const auto& map : game_.GetMaps()) {
        maps_array.push_back(json::value{
            {"id", *map.GetId()},
            {"name", map.GetName()}
        });
    }
    return maps_array;
}

void RequestHandler::HandleStaticFile(const http::request<http::string_body>& req, http::response<http::string_body>& res) {
    std::string target = URLDecode(req.target());
    if (target.starts_with('/')) {
        target = target.substr(1);
    }
    std::filesystem::path file_path = static_root_ / target;

    std::error_code ec;
    bool is_directory = std::filesystem::is_directory(file_path, ec);
    if (is_directory) {
        file_path /= "index.html";
    }

    if (!std::filesystem::exists(file_path, ec) || ec) {
        HandleNotFound(res, "File not found", false);
        return;
    }

    std::filesystem::path canonical_path = std::filesystem::canonical(file_path, ec);
    std::filesystem::path canonical_root = std::filesystem::canonical(static_root_, ec);
    if (canonical_path.string().find(canonical_root.string()) != 0) {
        HandleBadRequest(res, "Invalid path");
        return;
    }

    std::string extension = file_path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    static const std::unordered_map<std::string, std::string> mime_types = {
        {".html", "text/html"},
        {".htm", "text/html"},
        {".css", "text/css"},
        {".txt", "text/plain"},
        {".js", "text/javascript"},
        {".json", "application/json"},
        {".xml", "application/xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpe", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".bmp", "image/bmp"},
        {".ico", "image/vnd.microsoft.icon"},
        {".tiff", "image/tiff"},
        {".tif", "image/tiff"},
        {".svg", "image/svg+xml"},
        {".svgz", "image/svg+xml"},
        {".mp3", "audio/mpeg"},
    };
    std::string content_type = "application/octet-stream";
    auto it = mime_types.find(extension);
    if (it != mime_types.end()) {
        content_type = it->second;
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        HandleNotFound(res, "File not found", false);
        return;
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    res.result(http::status::ok);
    res.set(http::field::content_type, content_type);
    res.body() = content;
    res.content_length(content.size());

    if (req.method() == http::verb::head) {
        res.body().clear();
    }
}

std::string RequestHandler::URLDecode(std::string_view url) {
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

}  // namespace http_handler