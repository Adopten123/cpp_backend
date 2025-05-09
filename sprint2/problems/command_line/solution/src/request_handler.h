#pragma once
#include "http_server.h"
#include "model.h"

#include "api_handler.h"

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

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(data, "AdditionalData", boost::json::value)

namespace http_handler {

using namespace std::literals;
namespace beast = boost::beast;
namespace json = boost::json;
namespace http = beast::http;
namespace fs = std::filesystem;
namespace sys = boost::system;
namespace logging = boost::log;
namespace net = boost::asio;

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

std::vector<std::string_view> SplitRequest(std::string_view body);

json::object MapToJSON(const model::Map* map);
json::array RoadsToJson(const model::Map* map);
json::array OfficesToJson(const model::Map* map);
json::array BuildingsToJson(const model::Map* map);

struct ResponseData {
    http::status code;
    std::string_view content_type;
};

class ExtensionToConetntTypeMapper {
public:
    ExtensionToConetntTypeMapper();

    std::string_view operator()(std::string_view extension);

private:
    std::unordered_map<std::string_view, std::string_view> map_;
};

class Sender {
public:
    Sender() = delete;

    template<typename Send>
    static ResponseData SendBadRequest(Send&& send, bool is_head_method = false) {
        SendResponse(http::status::bad_request, HttpBodies::BAD_REQUEST, std::move(send), ContentType::APP_JSON, is_head_method);
        return { http::status::bad_request, ContentType::APP_JSON };
    }

    template<typename Send>
    static ResponseData SendFileResponseOr404(const fs::path root_path, std::string_view path, Send&& send, bool is_head_method = false) {
        http::response<http::file_body> res;
        res.version(11);
        res.result(http::status::ok);
        std::string full_path = root_path.string() + path.data();
        std::size_t ext_start = path.find_last_of('.', path.size());
        std::string_view type = ContentType::TEXT_HTML;
        if (ext_start != path.npos) {
            type = mapper_(path.substr(ext_start + 1, path.size() - ext_start + 1));
            res.insert(http::field::content_type, type);
        }
        else {
            full_path = root_path.string() + "/index.html";
            res.insert(http::field::content_type, ContentType::TEXT_HTML);
        }

        http::file_body::value_type file;

        if (sys::error_code ec; file.open(full_path.data(), beast::file_mode::read, ec), ec) {
            SendResponse(http::status::not_found, HttpBodies::FILE_NOT_FOUND, std::move(send), ContentType::TEXT_PLAIN, is_head_method);
            return { http::status::not_found , ContentType::TEXT_PLAIN };
        }

        if (!is_head_method) {
            res.body() = std::move(file);
        }
        res.prepare_payload();
        send(res);
        return { http::status::ok, type };
    }

    template<typename Send>
    static void SendResponse(http::status status, std::string_view body, Send&& send, std::string_view type, bool is_head_method = false) {
        http::response<http::string_body> response(status, 11);
        response.insert(http::field::cache_control, "no-cache");
        response.insert(http::field::content_type, type);
        if (!is_head_method) {
            response.body() = body;
        }
        send(response);
    }

    template<typename Send>
    static void SendAPIResponse(http::status status, std::string_view body, Send&& send, bool is_head_method = false) {
        http::response<http::string_body> response(status, 11);
        response.insert(http::field::content_type, ContentType::APP_JSON);
        response.insert(http::field::cache_control, "no-cache");
        if (!is_head_method) {
            response.body() = body;
        }
        response.prepare_payload();
        send(response);
    }

    template<typename Send>
    static ResponseData SendMethodNotAllowed(Send&& send, std::string_view allow) {
        http::response<http::string_body> response(http::status::method_not_allowed, 11);
        response.insert(http::field::content_type, ContentType::APP_JSON);
        response.insert(http::field::cache_control, "no-cache");
        response.insert(http::field::allow, allow);
        response.body() = HttpBodies::METHOD_NOT_ALLOWED;
        send(response);
        return { http::status::method_not_allowed, ContentType::APP_JSON };
    }

private:
    static ExtensionToConetntTypeMapper mapper_;
};

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    explicit RequestHandler(app::Application& app, const char* path_to_static, net::io_context& ioc, bool no_auto_tick);

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    auto& GetStrand() {
        return api_handler_->GetStrand();
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, std::function<void(ResponseData&&)> handle) {
        auto string_target = DecodeURL(req.target());
        std::string_view target(string_target);
        switch(CheckRequest(target)) {
        case RequestType::API:
        {
            net::dispatch(api_handler_->GetStrand(), [self = shared_from_this(), string_target_ = std::move(string_target)
                                                     , req_ = std::move(req), send_ = std::move(send), api_handler__ = api_handler_->shared_from_this()
                                                     , handle_ = std::move(handle)]() {
                    handle_(api_handler__->ProcessRequest(std::string_view(string_target_), std::move(send_), std::move(req_)));
                });
            return;
            break;
        }
        case RequestType::FILE:
            return handle(Sender::SendFileResponseOr404(root_path_, target, std::move(send)));
            break;
        case RequestType::BAD_REQUEST:
            return handle(Sender::SendBadRequest(std::move(send)));
            break;
        default:
            return handle(Sender::SendBadRequest(std::move(send)));
            break;
        }
    }

private:
    friend APIRequestHandler;

    const fs::path root_path_;
    std::shared_ptr<APIRequestHandler> api_handler_;

    enum RequestType {
        API,
        FILE,
        BAD_REQUEST
    };

    RequestType CheckRequest(std::string_view target) const;
    std::string DecodeURL(std::string_view url) const;
};

}  // namespace http_handler