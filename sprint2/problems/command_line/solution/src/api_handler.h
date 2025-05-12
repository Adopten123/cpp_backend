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

class HttpResponseFactory {
public:
    HttpResponseFactory() = delete;

    template<typename Send>
    static ResponseData HandleBadRequest(Send&& send, bool is_head_method = false) {
        HandleResponse(http::status::bad_request, RequestHttpBody::BAD_REQUEST, std::move(send), MimeType::APP_JSON, is_head_method);
        return { http::status::bad_request, MimeType::APP_JSON };
    }

    template<typename Send>
    static ResponseData HandleFileResponseOr404(const fs::path root_path, std::string_view path, Send&& send, bool is_head_method = false) {
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
            HandleResponse(http::status::not_found, RequestHttpBody::FILE_NOT_FOUND, std::move(send), MimeType::TEXT_PLAIN, is_head_method);
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
    static void HandleResponse(http::status status, std::string_view body, Send&& send, std::string_view type, bool is_head_method = false) {
        http::response<http::string_body> response(status, 11);
        response.insert(http::field::cache_control, "no-cache");
        response.insert(http::field::content_type, type);
        if (!is_head_method) {
            response.body() = body;
        }
        send(response);
    }

    template<typename Send>
    static void HandleAPIResponse(http::status status, std::string_view body, Send&& send, bool is_head_method = false) {
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
    static ResponseData HandleMethodNotAllowed(Send&& send, std::string_view allow) {
        http::response<http::string_body> response(http::status::method_not_allowed, 11);
        response.insert(http::field::content_type, MimeType::APP_JSON);
        response.insert(http::field::cache_control, "no-cache");
        response.insert(http::field::allow, allow);
        response.body() = RequestHttpBody::METHOD_NOT_ALLOWED;
        send(response);
        return { http::status::method_not_allowed, MimeType::APP_JSON };
    }
};


class APIHandler : public std::enable_shared_from_this<APIHandler> {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    explicit APIHandler(app::Application& app, net::io_context& ioc, bool no_auto_tick);

    APIHandler(const APIHandler&) = delete;
    APIHandler& operator=(const APIHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
	ResponseData ProcessRequest(
    	std::string_view target,
    	Send&& send,
    	const http::request<Body, http::basic_fields<Allocator>>&& req
	) {
    	const auto path_segments = utils::SplitRequest(target.substr(1));
    	const std::string_view http_method = req.method_string();
    	const auto http_version = req.version();

    	if (path_segments.size() < 3) {
        	return HttpResponseFactory::HandleBadRequest(std::forward<Send>(send));
    	}

    	const auto& resource = path_segments[2];

    	if (resource == RestApiLiteral::MAPS) {
        	if (path_segments.size() == 4) {
            	return HandleMapRequest(
                	std::string(path_segments[3]),
                	std::forward<Send>(send)
            	);
        	}
        	if (path_segments.size() == 3) {
            	const auto response_body = json::serialize(ProcessMapsRequestBody());
            	HttpResponseFactory::HandleAPIResponse(
                	http::status::ok,
                	response_body,
                	std::forward<Send>(send)
            	);
            	return {http::status::ok, MimeType::APP_JSON};
        	}
    	}

    	if (resource == RestApiLiteral::MAP) {
        	if (path_segments.size() != 4) {
            	return HttpResponseFactory::HandleBadRequest(std::forward<Send>(send));
        	}
        	return HandleMapRequest(
            	std::string(path_segments[3]),
           	 	std::forward<Send>(send)
        	);
    	}

    	if (resource == RestApiLiteral::GAME) {
        	if (path_segments.size() < 4) {
            	return HttpResponseFactory::HandleBadRequest(std::forward<Send>(send));
        	}

        	const auto& action = path_segments[3];

        	if (action == RestApiLiteral::JOIN) {
            	if (http_method != "POST") {
                	return HttpResponseFactory::HandleMethodNotAllowed(
                    	std::forward<Send>(send),
                    	"POST"
                	);
            	}
            	return HandleJoinRequest(req.body(), std::forward<Send>(send));
        	}

        	if (action == RestApiLiteral::PLAYERS) {
            	if (http_method != "GET" && http_method != "HEAD") {
                	return HttpResponseFactory::HandleMethodNotAllowed(
                    	std::forward<Send>(send),
                    	"GET, HEAD"
                	);
            	}

            	std::string auth_token;
            	const bool valid_token = ParseBearer(
                	req.base()[http::field::authorization],
                	auth_token
            	);

            	if (!valid_token) {
                	HttpResponseFactory::HandleAPIResponse(
                    	http::status::unauthorized,
                    	RequestHttpBody::INVALID_TOKEN,
                    	std::forward<Send>(send)
                	);
                	return {http::status::unauthorized, MimeType::APP_JSON};
            	}

            	return HandlePlayersRequest(std::move(auth_token), std::forward<Send>(send));
        	}

        	if (action == RestApiLiteral::STATE) {
            	if (http_method != "GET" && http_method != "HEAD") {
                	return HttpResponseFactory::HandleMethodNotAllowed(
                    	std::forward<Send>(send),
                    	"GET, HEAD"
                	);
            	}

            	std::string auth_token;
            	const bool valid_token = ParseBearer(
                	req.base()[http::field::authorization],
                	auth_token
            	);

            	if (!valid_token) {
                	HttpResponseFactory::HandleAPIResponse(
                    	http::status::unauthorized,
                    	RequestHttpBody::INVALID_TOKEN,
                    	std::forward<Send>(send)
                	);
                	return {http::status::unauthorized, MimeType::APP_JSON};
            	}

            	return HandleStateRequest(std::move(auth_token), std::forward<Send>(send));
        	}

        	if (action == RestApiLiteral::PLAYER) {
            	if (path_segments.size() == 4) {
                	HttpResponseFactory::HandleAPIResponse(
                    	http::status::bad_request,
                    	RequestHttpBody::BAD_REQUEST,
                    	std::forward<Send>(send)
                	);
                	return {http::status::bad_request, MimeType::APP_JSON};
            	}

            	if (path_segments[4] == RestApiLiteral::ACTION) {
                	if (http_method != "POST") {
                    	return HttpResponseFactory::HandleMethodNotAllowed(
                        	std::forward<Send>(send),
                        	"POST"
                    	);
                	}

                	std::string auth_token;
                	const bool valid_token = ParseBearer(
                    	req.base()[http::field::authorization],
                    	auth_token
                	);

                	if (!valid_token) {
                    	HttpResponseFactory::HandleAPIResponse(
                        	http::status::unauthorized,
                        	RequestHttpBody::INVALID_TOKEN,
                        	std::forward<Send>(send)
                    	);
                    	return {http::status::unauthorized, MimeType::APP_JSON};
                	}

                	if (req.base()[http::field::content_type] != MimeType::APP_JSON) {
                    	HttpResponseFactory::HandleAPIResponse(
                        	http::status::bad_request,
                        	RequestHttpBody::INVALID_CONTENT_TYPE,
                        	std::forward<Send>(send)
                    	);
                    	return {http::status::bad_request, MimeType::APP_JSON};
                	}

                	return HandleActionRequest(
                    	std::move(auth_token),
                    	req.body(),
                    	std::forward<Send>(send)
                	);
            	}
        	}

        	if (action == RestApiLiteral::TICK) {
            	if (auto_tick_) {
                	return HttpResponseFactory::HandleBadRequest(std::forward<Send>(send));
            	}

            	if (http_method != "POST") {
                	return HttpResponseFactory::HandleMethodNotAllowed(
                    	std::forward<Send>(send),
                    	"POST"
                	);
            	}

            	return HandleTickRequest(req.body(), std::forward<Send>(send));
        	}
    	}

    	return HttpResponseFactory::HandleBadRequest(std::forward<Send>(send));
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
    ResponseData HandleMapRequest(std::string id, Send&& send) {
        model::Map::Id map_id(id);
        const auto* map = app_.FindMap(map_id);
        if (map) {
            HttpResponseFactory::HandleAPIResponse(http::status::ok, json::serialize(utils::MapToJson(map)), std::move(send));
            return { http::status::ok , MimeType::APP_JSON };
        }
        else {
            HttpResponseFactory::HandleAPIResponse(http::status::not_found, RequestHttpBody::MAP_NOT_FOUND, std::move(send));
            return { http::status::not_found , MimeType::APP_JSON };
        }
    }

    template<typename Send>
    ResponseData HandleJoinRequest(std::string_view body, Send&& send) {
        json::object json_body;
        try {
            json_body = json::parse(body.data()).as_object();
        }
        catch (std::exception& ex) {
            HttpResponseFactory::HandleAPIResponse(http::status::bad_request, RequestHttpBody::JOIN_GAME_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        if (!json_body.contains("userName") || !json_body.at("userName").is_string()) {
            HttpResponseFactory::HandleAPIResponse(http::status::bad_request, RequestHttpBody::JOIN_GAME_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        auto user_name = json_body.at("userName").get_string();
        if (user_name.size() == 0) {
            HttpResponseFactory::HandleAPIResponse(http::status::bad_request, RequestHttpBody::INVALID_NAME, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        if (!json_body.contains("mapId") || !json_body.at("mapId").is_string()) {
            HttpResponseFactory::HandleAPIResponse(http::status::bad_request, RequestHttpBody::JOIN_GAME_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        auto map_id = json_body.at("mapId").get_string();
        auto id = model::Map::Id(map_id.data());
        auto* session = app_.FindSession(id);
        if (!session) {
            HttpResponseFactory::HandleAPIResponse(http::status::not_found, RequestHttpBody::MAP_NOT_FOUND, std::move(send));
            return { http::status::not_found, MimeType::APP_JSON };
        }
        model::Dog dog{ std::move(std::string(user_name.data())) };
        auto& player = app_.AddPlayer(std::move(dog), session);
        json::object result;
        app::Token token = player.GetToken();
        std::string tokenStr = *token;
        result["authToken"] = tokenStr;
        result["playerId"] = player.GetId();
        HttpResponseFactory::HandleAPIResponse(http::status::ok, json::serialize(result), std::move(send));
        return { http::status::ok, MimeType::APP_JSON };
    }

    template<typename Send>
    ResponseData HandlePlayersRequest(std::string&& token, Send&& send) {
        auto* player = app_.FindByToken(app::Token(token));
        if (!player) {
            HttpResponseFactory::HandleAPIResponse(http::status::unauthorized, RequestHttpBody::TOKEN_UNKNOWN, std::move(send));
            return { http::status::unauthorized, MimeType::APP_JSON };
        }
        json::object result;
        for (const auto* dog : app_.GetDogs(player)) {
            result[std::to_string(dog->GetId())] = json::array{ "name", dog->GetName() };
        }
        HttpResponseFactory::HandleAPIResponse(http::status::ok, json::serialize(result), std::move(send));
        return { http::status::ok, MimeType::APP_JSON };
    }

    template<typename Send>
    ResponseData HandleStateRequest(std::string&& token, Send&& send) {
        auto* player = app_.FindByToken(app::Token(token));
        if (!player) {
            HttpResponseFactory::HandleAPIResponse(http::status::unauthorized, RequestHttpBody::TOKEN_UNKNOWN, std::move(send));
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
        HttpResponseFactory::HandleAPIResponse(http::status::ok, json::serialize(result), std::move(send));
        return { http::status::ok, MimeType::APP_JSON };
    }

    template<typename Send>
    ResponseData HandleActionRequest(std::string&& token, std::string_view body, Send&& send) {
        auto* player = app_.FindByToken(app::Token(token));
        if (!player) {
            HttpResponseFactory::HandleAPIResponse(http::status::unauthorized, RequestHttpBody::TOKEN_UNKNOWN, std::move(send));
            return { http::status::unauthorized, MimeType::APP_JSON };
        }
        json::object json_body;
        try {
            json_body = json::parse(body.data()).as_object();
        }
        catch (std::exception& ex) {
            HttpResponseFactory::HandleAPIResponse(http::status::bad_request, RequestHttpBody::ACTION_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        auto move = json_body.find("move");
        if (move == json_body.end() || !move->value().is_string()) {
            HttpResponseFactory::HandleAPIResponse(http::status::bad_request, RequestHttpBody::ACTION_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        std::string_view move_value = move->value().get_string();
        if (move_value == "U") {
            app_.Move(player, model::Direction::NORTH);
            HttpResponseFactory::HandleAPIResponse(http::status::ok, "{}"sv, std::move(send));
            return { http::status::ok, MimeType::APP_JSON };
        }
        if (move_value == "D") {
            app_.Move(player, model::Direction::SOUTH);
            HttpResponseFactory::HandleAPIResponse(http::status::ok, "{}"sv, std::move(send));
            return { http::status::ok, MimeType::APP_JSON };
        }
        if (move_value == "L") {
            app_.Move(player, model::Direction::WEST);
            HttpResponseFactory::HandleAPIResponse(http::status::ok, "{}"sv, std::move(send));
            return { http::status::ok, MimeType::APP_JSON };
        }
        if (move_value == "R") {
            app_.Move(player, model::Direction::EAST);
            HttpResponseFactory::HandleAPIResponse(http::status::ok, "{}"sv, std::move(send));
            return { http::status::ok, MimeType::APP_JSON };
        }
        if (move_value == "") {
            app_.Stop(player);
            HttpResponseFactory::HandleAPIResponse(http::status::ok, "{}"sv, std::move(send));
            return { http::status::ok, MimeType::APP_JSON };
        }
        HttpResponseFactory::HandleAPIResponse(http::status::bad_request, RequestHttpBody::ACTION_PARSE_ERROR, std::move(send));
        return { http::status::bad_request, MimeType::APP_JSON };
    }

    template<typename Send>
    ResponseData HandleTickRequest(std::string_view body, Send&& send) {
        json::object json_body;
        try {
            json_body = json::parse(body.data()).as_object();
        }
        catch (std::exception& ex) {
            HttpResponseFactory::HandleAPIResponse(http::status::bad_request, RequestHttpBody::TICK_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        auto tick = json_body.find("timeDelta");
        if (tick == json_body.end() || !tick->value().is_int64()) {
            HttpResponseFactory::HandleAPIResponse(http::status::bad_request, RequestHttpBody::TICK_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        int tick_val = tick->value().get_int64();
        if (tick_val < 1) {
            HttpResponseFactory::HandleAPIResponse(http::status::bad_request, RequestHttpBody::TICK_PARSE_ERROR, std::move(send));
            return { http::status::bad_request, MimeType::APP_JSON };
        }
        app_.Tick(tick_val);
        HttpResponseFactory::HandleAPIResponse(http::status::ok, "{}"sv, std::move(send));
        return { http::status::ok, MimeType::APP_JSON };
    }

    bool ParseBearer(const std::string_view auth_header, std::string& token_to_write) const;
};


}