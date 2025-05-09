#pragma once

#include "request_handler.h"

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

namespace http_handler {

using namespace std::literals;
namespace beast = boost::beast;
namespace json = boost::json;
namespace http = beast::http;
namespace fs = std::filesystem;
namespace sys = boost::system;
namespace logging = boost::log;
namespace net = boost::asio;

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

}