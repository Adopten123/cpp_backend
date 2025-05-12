#pragma once

#include "handler_utils.h"

namespace http_handler {

using namespace std::literals;
namespace beast = boost::beast;
namespace json = boost::json;
namespace http = beast::http;
namespace fs = std::filesystem;
namespace sys = boost::system;
namespace logging = boost::log;
namespace net = boost::asio;

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
        SendResponse(http::status::bad_request, HttpBodies::BAD_REQUEST, std::move(send), MimeType::APP_JSON, is_head_method);
        return { http::status::bad_request, MimeType::APP_JSON };
    }

    template<typename Send>
    static ResponseData SendFileResponseOr404(const fs::path root_path, std::string_view path, Send&& send, bool is_head_method = false) {
        http::response<http::file_body> res;
        res.version(11);
        res.result(http::status::ok);
        std::string full_path = root_path.string() + path.data();
        std::size_t ext_start = path.find_last_of('.', path.size());
        std::string_view type = MimeType::TEXT_HTML;
        if (ext_start != path.npos) {
            type = utils::GetMimeType(path.substr(ext_start + 1, path.size() - ext_start + 1));
            res.insert(http::field::content_type, type);
        }
        else {
            full_path = root_path.string() + "/index.html";
            res.insert(http::field::content_type, MimeType::TEXT_HTML);
        }

        http::file_body::value_type file;

        if (sys::error_code ec; file.open(full_path.data(), beast::file_mode::read, ec), ec) {
            SendResponse(http::status::not_found, HttpBodies::FILE_NOT_FOUND, std::move(send), MimeType::TEXT_PLAIN, is_head_method);
            return { http::status::not_found , MimeType::TEXT_PLAIN };
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
        response.insert(http::field::content_type, MimeType::APP_JSON);
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
        response.insert(http::field::content_type, MimeType::APP_JSON);
        response.insert(http::field::cache_control, "no-cache");
        response.insert(http::field::allow, allow);
        response.body() = HttpBodies::METHOD_NOT_ALLOWED;
        send(response);
        return { http::status::method_not_allowed, MimeType::APP_JSON };
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
        auto splitted = utils::SplitRequest(unslashed);
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
                return { http::status::ok , MimeType::APP_JSON };
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
                    return { http::status::unauthorized, MimeType::APP_JSON };
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
                    return { http::status::unauthorized, MimeType::APP_JSON };
                }
                return StateRequest(std::move(token), std::move(send));
            }
            if (splitted[3] == RestApiLiterals::PLAYER) {
                if (splitted.size() == 4) {
                    Sender::SendAPIResponse(http::status::bad_request, HttpBodies::BAD_REQUEST, std::move(send));
                    return { http::status::bad_request, MimeType::APP_JSON };
                }
                if (splitted[4] == RestApiLiterals::ACTION) {
                    if (method != "POST") {
                        return Sender::SendMethodNotAllowed(std::move(send), "POST");
                    }
                    std::string token = "";
                    auto token_valid = ParseBearer(std::move(req.base()[http::field::authorization]), token);
                    if (!token_valid) {
                        Sender::SendAPIResponse(http::status::unauthorized, HttpBodies::INVALID_TOKEN, std::move(send));
                        return { http::status::unauthorized, MimeType::APP_JSON };
                    }
                    std::string_view content_type = req.base()[http::field::content_type];
                    if (content_type != MimeType::APP_JSON) {
                        Sender::SendAPIResponse(http::status::bad_request, HttpBodies::INVALID_CONTENT_TYPE, std::move(send));
                        return { http::status::bad_request, MimeType::APP_JSON };
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
            Sender::SendAPIResponse(http::status::ok, json::serialize(utils::MapToJson(map)), std::move(send));
            return { http::status::ok , MimeType::APP_JSON };
        }
        else {
            Sender::SendAPIResponse(http::status::not_found, HttpBodies::MAP_NOT_FOUND, std::move(send));
            return { http::status::not_found , MimeType::APP_JSON };
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
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        if (!json_body.contains("userName") || !json_body.at("userName").is_string()) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::JOIN_GAME_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        auto user_name = json_body.at("userName").get_string();
        if (user_name.size() == 0) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::INVALID_NAME, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        if (!json_body.contains("mapId") || !json_body.at("mapId").is_string()) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::JOIN_GAME_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        auto map_id = json_body.at("mapId").get_string();
        auto id = model::Map::Id(map_id.data());
        auto* session = app_.FindSession(id);
        if (!session) {
            Sender::SendAPIResponse(http::status::not_found, HttpBodies::MAP_NOT_FOUND, std::move(send));
            return { http::status::not_found, MimeType::APP_JSON };
        }
        model::Dog dog{ std::move(std::string(user_name.data())) };
        auto& player = app_.AddPlayer(std::move(dog), session);
        json::object result;
        app::Token token = player.GetToken();
        std::string tokenStr = *token;
        result["authToken"] = tokenStr;
        result["playerId"] = player.GetId();
        Sender::SendAPIResponse(http::status::ok, json::serialize(result), std::move(send));
        return { http::status::ok, MimeType::APP_JSON };
    }

    template<typename Send>
    ResponseData PlayersRequest(std::string&& token, Send&& send) {
        auto* player = app_.FindByToken(app::Token(token));
        if (!player) {
            Sender::SendAPIResponse(http::status::unauthorized, HttpBodies::TOKEN_UNKNOWN, std::move(send));
            return { http::status::unauthorized, MimeType::APP_JSON };
        }
        json::object result;
        for (const auto* dog : app_.GetDogs(player)) {
            result[std::to_string(dog->GetId())] = json::array{ "name", dog->GetName() };
        }
        Sender::SendAPIResponse(http::status::ok, json::serialize(result), std::move(send));
        return { http::status::ok, MimeType::APP_JSON };
    }

    template<typename Send>
    ResponseData StateRequest(std::string&& token, Send&& send) {
        auto* player = app_.FindByToken(app::Token(token));
        if (!player) {
            Sender::SendAPIResponse(http::status::unauthorized, HttpBodies::TOKEN_UNKNOWN, std::move(send));
            return { http::status::unauthorized, MimeType::APP_JSON };
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
        return { http::status::ok, MimeType::APP_JSON };
    }

    template<typename Send>
    ResponseData ActionRequest(std::string&& token, std::string_view body, Send&& send) {
        auto* player = app_.FindByToken(app::Token(token));
        if (!player) {
            Sender::SendAPIResponse(http::status::unauthorized, HttpBodies::TOKEN_UNKNOWN, std::move(send));
            return { http::status::unauthorized, MimeType::APP_JSON };
        }
        json::object json_body;
        try {
            json_body = json::parse(body.data()).as_object();
        }
        catch (std::exception& ex) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::ACTION_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        auto move = json_body.find("move");
        if (move == json_body.end() || !move->value().is_string()) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::ACTION_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        std::string_view move_value = move->value().get_string();
        if (move_value == "U") {
            app_.Move(player, model::Direction::NORTH);
            Sender::SendAPIResponse(http::status::ok, "{}"sv, std::move(send));
            return { http::status::ok, MimeType::APP_JSON };
        }
        if (move_value == "D") {
            app_.Move(player, model::Direction::SOUTH);
            Sender::SendAPIResponse(http::status::ok, "{}"sv, std::move(send));
            return { http::status::ok, MimeType::APP_JSON };
        }
        if (move_value == "L") {
            app_.Move(player, model::Direction::WEST);
            Sender::SendAPIResponse(http::status::ok, "{}"sv, std::move(send));
            return { http::status::ok, MimeType::APP_JSON };
        }
        if (move_value == "R") {
            app_.Move(player, model::Direction::EAST);
            Sender::SendAPIResponse(http::status::ok, "{}"sv, std::move(send));
            return { http::status::ok, MimeType::APP_JSON };
        }
        if (move_value == "") {
            app_.Stop(player);
            Sender::SendAPIResponse(http::status::ok, "{}"sv, std::move(send));
            return { http::status::ok, MimeType::APP_JSON };
        }
        Sender::SendAPIResponse(http::status::bad_request, HttpBodies::ACTION_PARSE_ERROR, std::move(send));
        return { http::status::bad_request, MimeType::APP_JSON };
    }

    template<typename Send>
    ResponseData TickRequest(std::string_view body, Send&& send) {
        json::object json_body;
        try {
            json_body = json::parse(body.data()).as_object();
        }
        catch (std::exception& ex) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::TICK_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        auto tick = json_body.find("timeDelta");
        if (tick == json_body.end() || !tick->value().is_int64()) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::TICK_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        int tick_val = tick->value().get_int64();
        if (tick_val < 1) {
            Sender::SendAPIResponse(http::status::bad_request, HttpBodies::TICK_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        app_.Tick(tick_val);
        Sender::SendAPIResponse(http::status::ok, "{}"sv, std::move(send));
        return { http::status::ok, MimeType::APP_JSON };
    }

    bool ParseBearer(const std::string_view auth_header, std::string& token_to_write) const;
};


}