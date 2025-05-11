#pragma once

#include <string_view>

namespace http_handler {

using namespace std::literals;

struct ContentType {
    ContentType() = delete;
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

struct FileExtensions {
    FileExtensions() = delete;
    constexpr static std::string_view HTML = "html"sv;
    constexpr static std::string_view HTM = "htm"sv;
    constexpr static std::string_view JSON = "json"sv;
    constexpr static std::string_view CSS = "css"sv;
    constexpr static std::string_view TXT = "txt"sv;
    constexpr static std::string_view JS = "js"sv;
    constexpr static std::string_view XML = "xml"sv;
    constexpr static std::string_view PNG = "png"sv;
    constexpr static std::string_view JPEG = "jpeg"sv;
    constexpr static std::string_view JPG = "jpg"sv;
    constexpr static std::string_view JPE = "jpe"sv;
    constexpr static std::string_view GIF = "gif"sv;
    constexpr static std::string_view BMP = "bmp"sv;
    constexpr static std::string_view ICO = "ico"sv;
    constexpr static std::string_view TIFF = "tiff"sv;
    constexpr static std::string_view TIF = "tif"sv;
    constexpr static std::string_view SVG = "svg"sv;
    constexpr static std::string_view SVGZ = "svgz"sv;
    constexpr static std::string_view MP3 = "mp3"sv;
};

struct RestApiLiterals {
    RestApiLiterals() = delete;
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

struct HttpBodies {
    HttpBodies() = delete;
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

}