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

class APIRequestHandler : public std::enable_shared_from_this<APIRequestHandler> {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    explicit APIRequestHandler(app::Application& app, net::io_context& ioc, bool no_auto_tick);

    APIRequestHandler(const APIRequestHandler&) = delete;
    APIRequestHandler& operator=(const APIRequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    ResponseData ProcessRequest(std::string_view target, Send&& send, const http::request<Body, http::basic_fields<Allocator>>&& req) {
        auto unslashed = target.substr(1, target.length() - 1);
        auto splitted = SplitRequest(unslashed);
        std::string_view method = std::string_view(req.method_string().data());
        unsigned http_version = req.version();
        if (splitted.size() < 3) {
            return Sender::SendBadRequest(std::move(send));
        }
        if (splitted[2] == RestApiLiterals::MAPS) {
            if (splitted.size() == 4) {
                return MapRequest(std::string(splitted[3].data(), splitted[3].size()), std::move(send));
            }
            if (splitted.size() == 3) {
                Sender::SendAPIResponse(http::status::ok, json::serialize(ProcessMapsRequestBody()), std::move(send));
                return { http::status::ok , ContentType::APP_JSON };
            }
        }
        if (splitted[2] == RestApiLiterals::MAP) {
            if (splitted.size() != 4) {
                return Sender::SendBadRequest(std::move(send));
            }
            return MapRequest(std::string(splitted[3].data(), splitted[3].size()), std::move(send));
        }
        if (splitted[2] == RestApiLiterals::GAME) {
            if (splitted.size() < 4) {
                return Sender::SendBadRequest(std::move(send));
            }
            if (splitted[3] == RestApiLiterals::JOIN) {
                if (method != "POST") {
                    return Sender::SendMethodNotAllowed(std::move(send), "POST");
                }
                return JoinRequest(req.body(), std::move(send));
            }
            if (splitted[3] == RestApiLiterals::PLAYERS) {
                if (method != "GET" && method != "HEAD") {
                    return Sender::SendMethodNotAllowed(std::move(send), "GET, HEAD");
                }
                std::string token = "";
                auto token_valid = ParseBearer(std::move(req.base()[http::field::authorization]), token);
                if (!token_valid) {
                    Sender::SendAPIResponse(http::status::unauthorized, HttpBodies::INVALID_TOKEN, std::move(send));
                    return { http::status::unauthorized, ContentType::APP_JSON };
                }
                return PlayersRequest(std::move(token), std::move(send));
            }
            if (splitted[3] == RestApiLiterals::STATE) {
                if (method != "GET" && method != "HEAD") {
                    return Sender::SendMethodNotAllowed(std::move(send), "GET, HEAD");
                }
                std::string token = "";
                auto token_valid = ParseBearer(std::move(req.base()[http::field::authorization]), token);
                if (!token_valid) {
                    Sender::SendAPIResponse(http::status::unauthorized, HttpBodies::INVALID_TOKEN, std::move(send));
                    return { http::status::unauthorized, ContentType::APP_JSON };
                }
                return StateRequest(std::move(token), std::move(send));
            }
            if (splitted[3] == RestApiLiterals::PLAYER) {
                if (splitted.size() == 4) {
                    Sender::SendAPIResponse(http::status::bad_request, HttpBodies::BAD_REQUEST, std::move(send));
                    return { http::status::bad_request, ContentType::APP_JSON };
                }
                if (splitted[4] == RestApiLiterals::ACTION) {
                    if (method != "POST") {
                        return Sender::SendMethodNotAllowed(std::move(send), "POST");
                    }
                    std::string token = "";
                    auto token_valid = ParseBearer(std::move(req.base()[http::field::authorization]), token);
                    if (!token_valid) {
                        Sender::SendAPIResponse(http::status::unauthorized, HttpBodies::INVALID_TOKEN, std::move(send));
                        return { http::status::unauthorized, ContentType::APP_JSON };
                    }
                    std::string_view content_type = req.base()[http::field::content_type];
                    if (content_type != ContentType::APP_JSON) {
                        Sender::SendAPIResponse(http::status::bad_request, HttpBodies::INVALID_CONTENT_TYPE, std::move(send));
                        return { http::status::bad_request, ContentType::APP_JSON };
                    }
                    return ActionRequest(std::move(token), req.body(), std::move(send));
                }
            }
            if (splitted[3] == RestApiLiterals::TICK) {
                if (auto_tick_) {
                    return Sender::SendBadRequest(std::move(send));
                }
                if (method != "POST") {
                    return Sender::SendMethodNotAllowed(std::move(send), "POST");
                }
                return TickRequest(req.body(), std::move(send));
            }
        }
        return Sender::SendBadRequest(std::move(send));
    }

    Strand& GetStrand() {
        return strand_;
    }

private:
    app::Application& app_;
    Strand strand_;
    bool auto_tick_;

    json::array ProcessMapsRequestBody() const;

    template<typename Send>
    ResponseData MapRequest(std::string id, Send&& send) {
        model::Map::Id map_id(id);
        const auto* map = app_.FindMap(map_id);
        if (map) {
            Sender::SendAPIResponse(http::status::ok, json::serialize(MapToJSON(map)), std::move(send));
            return { http::status::ok , ContentType::APP_JSON };
        }
        else {
            Sender::SendAPIResponse(http::status::not_found, HttpBodies::MAP_NOT_FOUND, std::move(send));
            return { http::status::not_found , ContentType::APP_JSON };
        }
    }

    template<typename Send>
    ResponseData JoinRequest(std::string_view body, Send&& send) {
        json::object json_body;
        try {
            json_body = json::parse(body.data()).as_object();
        }
        catch (std::exception& ex) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::JOIN_GAME_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, ContentType::APP_JSON };
        }
        if (!json_body.contains("userName") || !json_body.at("userName").is_string()) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::JOIN_GAME_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, ContentType::APP_JSON };
        }
        auto user_name = json_body.at("userName").get_string();
        if (user_name.size() == 0) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::INVALID_NAME, std::move(send));
            return { http::status::bad_request, ContentType::APP_JSON };
        }
        if (!json_body.contains("mapId") || !json_body.at("mapId").is_string()) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::JOIN_GAME_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, ContentType::APP_JSON };
        }
        auto map_id = json_body.at("mapId").get_string();
        auto id = model::Map::Id(map_id.data());
        auto* session = app_.FindSession(id);
        if (!session) {
            Sender::SendAPIResponse(http::status::not_found, HttpBodies::MAP_NOT_FOUND, std::move(send));
            return { http::status::not_found, ContentType::APP_JSON };
        }
        model::Dog dog{ std::move(std::string(user_name.data())) };
        auto& player = app_.AddPlayer(std::move(dog), session);
        json::object result;
        app::Token token = player.GetToken();
        std::string tokenStr = *token;
        result["authToken"] = tokenStr;
        result["playerId"] = player.GetId();
        Sender::SendAPIResponse(http::status::ok, json::serialize(result), std::move(send));
        return { http::status::ok, ContentType::APP_JSON };
    }

    template<typename Send>
    ResponseData PlayersRequest(std::string&& token, Send&& send) {
        auto* player = app_.FindByToken(app::Token(token));
        if (!player) {
            Sender::SendAPIResponse(http::status::unauthorized, HttpBodies::TOKEN_UNKNOWN, std::move(send));
            return { http::status::unauthorized, ContentType::APP_JSON };
        }
        json::object result;
        for (const auto* dog : app_.GetDogs(player)) {
            result[std::to_string(dog->GetId())] = json::array{ "name", dog->GetName() };
        }
        Sender::SendAPIResponse(http::status::ok, json::serialize(result), std::move(send));
        return { http::status::ok, ContentType::APP_JSON };
    }

    template<typename Send>
    ResponseData StateRequest(std::string&& token, Send&& send) {
        auto* player = app_.FindByToken(app::Token(token));
        if (!player) {
            Sender::SendAPIResponse(http::status::unauthorized, HttpBodies::TOKEN_UNKNOWN, std::move(send));
            return { http::status::unauthorized, ContentType::APP_JSON };
        }
        json::object result;
        json::object players;
        for (const auto& dog : app_.GetDogs(player)) {
            json::object data;
            auto pos = dog->GetPosition();
            auto speed = dog->GetSpeed();
            data["pos"] = { pos.x, pos.y };
            data["speed"] = { speed.vx, speed.vy };
            switch (dog->GetDirection()) {
            case model::Direction::NORTH:
                data["dir"] = "U";
                break;
            case model::Direction::SOUTH:
                data["dir"] = "D";
                break;
            case model::Direction::EAST:
                data["dir"] = "R";
                break;
            case model::Direction::WEST:
                data["dir"] = "L";
                break;
            }
            players[std::to_string(dog->GetId())] = data;
        }
        result["players"] = players;
        Sender::SendAPIResponse(http::status::ok, json::serialize(result), std::move(send));
        return { http::status::ok, ContentType::APP_JSON };
    }

    template<typename Send>
    ResponseData ActionRequest(std::string&& token, std::string_view body, Send&& send) {
        auto* player = app_.FindByToken(app::Token(token));
        if (!player) {
            Sender::SendAPIResponse(http::status::unauthorized, HttpBodies::TOKEN_UNKNOWN, std::move(send));
            return { http::status::unauthorized, ContentType::APP_JSON };
        }
        json::object json_body;
        try {
            json_body = json::parse(body.data()).as_object();
        }
        catch (std::exception& ex) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::ACTION_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, ContentType::APP_JSON };
        }
        auto move = json_body.find("move");
        if (move == json_body.end() || !move->value().is_string()) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::ACTION_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, ContentType::APP_JSON };
        }
        std::string_view move_value = move->value().get_string();
        if (move_value == "U") {
            app_.Move(player, model::Direction::NORTH);
            Sender::SendAPIResponse(http::status::ok, "{}"sv, std::move(send));
            return { http::status::ok, ContentType::APP_JSON };
        }
        if (move_value == "D") {
            app_.Move(player, model::Direction::SOUTH);
            Sender::SendAPIResponse(http::status::ok, "{}"sv, std::move(send));
            return { http::status::ok, ContentType::APP_JSON };
        }
        if (move_value == "L") {
            app_.Move(player, model::Direction::WEST);
            Sender::SendAPIResponse(http::status::ok, "{}"sv, std::move(send));
            return { http::status::ok, ContentType::APP_JSON };
        }
        if (move_value == "R") {
            app_.Move(player, model::Direction::EAST);
            Sender::SendAPIResponse(http::status::ok, "{}"sv, std::move(send));
            return { http::status::ok, ContentType::APP_JSON };
        }
        if (move_value == "") {
            app_.Stop(player);
            Sender::SendAPIResponse(http::status::ok, "{}"sv, std::move(send));
            return { http::status::ok, ContentType::APP_JSON };
        }
        Sender::SendAPIResponse(http::status::bad_request, HttpBodies::ACTION_PARSE_ERROR, std::move(send));
        return { http::status::bad_request, ContentType::APP_JSON };
    }

    template<typename Send>
    ResponseData TickRequest(std::string_view body, Send&& send) {
        json::object json_body;
        try {
            json_body = json::parse(body.data()).as_object();
        }
        catch (std::exception& ex) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::TICK_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, ContentType::APP_JSON };
        }
        auto tick = json_body.find("timeDelta");
        if (tick == json_body.end() || !tick->value().is_int64()) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::TICK_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, ContentType::APP_JSON };
        }
        int tick_val = tick->value().get_int64();
        if (tick_val < 1) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::TICK_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, ContentType::APP_JSON };
        }
        app_.Tick(tick_val);
        Sender::SendAPIResponse(http::status::ok, "{}"sv, std::move(send));
        return { http::status::ok, ContentType::APP_JSON };
    }

    bool ParseBearer(const std::string_view auth_header, std::string& token_to_write) const;
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

class LoggingRequestHandler {
    template <typename Body, typename Allocator>
    static void LogRequest(const http::request<Body, http::basic_fields<Allocator>>& r, const boost::beast::net::ip::address& address) {
        json::object request_data;
        request_data["ip"] = address.to_string();
        request_data["URI"] = std::string(r.target());
        request_data["method"] = r.method_string().data();
        BOOST_LOG_TRIVIAL(info) << logging::add_value(data, request_data) << "request received";
    }
    static void LogResponse(const ResponseData& r, boost::chrono::system_clock::time_point start_time, const boost::beast::net::ip::address&& address);
public:
    explicit LoggingRequestHandler(std::shared_ptr<RequestHandler> handler) : decorated_(handler) {
    }

    static void Formatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
        auto ts = *logging::extract<boost::posix_time::ptime>("TimeStamp", rec);

        strm << "{\"timestamp\":\"" << boost::posix_time::to_iso_extended_string(ts) << "\", ";
        strm << "\"data\":" << json::serialize(*logging::extract<boost::json::value>("AdditionalData", rec)) << ", ";
        strm << "\"message\":\"" << rec[logging::expressions::smessage] << "\"}";
    }

    template <typename Body, typename Allocator, typename Send>
    void operator () (http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, const boost::beast::net::ip::address& address) {
        LogRequest(req, address);
        boost::chrono::system_clock::time_point start = boost::chrono::system_clock::now();
        auto handle { [address, start](ResponseData&& resp_data) {
                LogResponse(std::move(resp_data), start, std::move(address));
            }};
        decorated_->operator()(std::move(req), std::move(send), handle);
    }

private:
    std::shared_ptr<RequestHandler> decorated_;
};

}  // namespace http_handler