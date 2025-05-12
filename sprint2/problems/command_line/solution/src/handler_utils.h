#pragma once

#include "http_server.h"
#include "model.h"

#include <boost/asio/io_context.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/date_time.hpp>
#include <boost/chrono.hpp>
#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>

namespace http_handler {

using namespace std::literals;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

struct MimeType {
    MimeType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
    constexpr static std::string_view APP_JSON = "application/json"sv;
    constexpr static std::string_view TEXT_CSS = "text/css"sv;
    constexpr static std::string_view TEXT_PLAIN = "text/plain"sv;
    constexpr static std::string_view TEXT_JAVASCRIPT = "text/javascript"sv;
    constexpr static std::string_view APP_XML = "application/xml"sv;
    constexpr static std::string_view PNG = "image/png"sv;
    constexpr static std::string_view JPEG = "image/jpeg"sv;
    constexpr static std::string_view GIF = "image/gif"sv;
    constexpr static std::string_view BMP = "image/bmp"sv;
    constexpr static std::string_view ICO = "image/vnd.microsoft.icon"sv;
    constexpr static std::string_view TIFF = "image/tiff"sv;
    constexpr static std::string_view SVG = "image/svg+xml"sv;
    constexpr static std::string_view MP3 = "audio/mpeg"sv;
    constexpr static std::string_view UNKNOWN = "application/octet-stream"sv;
};

struct RestApiLiteral {
    RestApiLiteral() = delete;
    constexpr static std::string_view API_V1 = "/api/v1/"sv;
    constexpr static std::string_view MAPS = "maps"sv;
    constexpr static std::string_view MAP = "map"sv;
    constexpr static std::string_view GAME = "game"sv;
    constexpr static std::string_view JOIN = "join"sv;
    constexpr static std::string_view PLAYERS = "players"sv;
    constexpr static std::string_view STATE = "state"sv;
    constexpr static std::string_view PLAYER = "player"sv;
    constexpr static std::string_view ACTION = "action"sv;
    constexpr static std::string_view TICK = "tick"sv;
};

struct RequestHttpBody {
    RequestHttpBody() = delete;
    constexpr static std::string_view BAD_REQUEST = R"({ "code": "badRequest", "message": "Bad request" })"sv;
    constexpr static std::string_view MAP_NOT_FOUND = R"({ "code": "mapNotFound", "message": "Map not found" })"sv;
    constexpr static std::string_view FILE_NOT_FOUND = R"({ "code": "fileNotFound", "message": "File not found" })"sv;
    constexpr static std::string_view INVALID_NAME = R"({ "code": "invalidArgument", "message": "Invalid name" })"sv;
    constexpr static std::string_view JOIN_GAME_PARSE_ERROR = R"({ "code": "invalidArgument", "message": "Join game request parse error" })"sv;
    constexpr static std::string_view ACTION_PARSE_ERROR = R"({ "code": "invalidArgument", "message": "Failed to parse action" })"sv;
    constexpr static std::string_view TICK_PARSE_ERROR = R"({ "code": "invalidArgument", "message": "Failed to parse tick request JSON" })"sv;
    constexpr static std::string_view METHOD_NOT_ALLOWED = R"({ "code": "invalidMethod", "message": "Another method expected" })"sv;
    constexpr static std::string_view INVALID_TOKEN = R"({ "code": "invalidToken", "message": "Authorization header is missing" })"sv;
    constexpr static std::string_view TOKEN_UNKNOWN = R"({ "code": "unknownToken", "message": "Player token has not been found" })"sv;
    constexpr static std::string_view INVALID_CONTENT_TYPE = R"({"code": "invalidArgument", "message": "Invalid content type"} )"sv;
};

namespace utils {
	std::vector<std::string_view> SplitRequest(std::string_view body);
	json::object MapToJson(const model::Map* map);
	json::array RoadsToJson(const model::Map* map);
	json::array OfficesToJson(const model::Map* map);
	json::array BuildingsToJson(const model::Map* map);

    std::string_view GetMimeType(std::string_view extension);
}

struct ResponseData {
    http::status code;
    std::string_view content_type;
};

}