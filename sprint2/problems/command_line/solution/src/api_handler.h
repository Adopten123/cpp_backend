#pragma once

#include "http_response_factory.h"

namespace http_handler {

using namespace std::literals;
namespace beast = boost::beast;
namespace json = boost::json;
namespace http = beast::http;
namespace fs = std::filesystem;
namespace sys = boost::system;
namespace logging = boost::log;
namespace net = boost::asio;


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
	)
    {
    	const auto path_segments = utils::SplitRequest(target.substr(1));
    	const std::string_view http_method = req.method_string();
    	const auto http_version = req.version();

    	if (path_segments.size() < 3)
        	return HttpResponseFactory::HandleBadRequest(std::forward<Send>(send));

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
        	if (path_segments.size() != 4)
            	return HttpResponseFactory::HandleBadRequest(std::forward<Send>(send));

        	return HandleMapRequest(
            	std::string(path_segments[3]),
           	 	std::forward<Send>(send)
        	);
    	}

    	if (resource == RestApiLiteral::GAME) {
        	if (path_segments.size() < 4)
            	return HttpResponseFactory::HandleBadRequest(std::forward<Send>(send));

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
            	if (auto_tick_)
                	return HttpResponseFactory::HandleBadRequest(std::forward<Send>(send));

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

    Strand& GetStrand() { return strand_; }
private:
    app::Application& app_;
    Strand strand_;
    bool auto_tick_;

    json::array ProcessMapsRequestBody() const;

    template<typename Send>
	ResponseData HandleMapRequest(std::string id, Send&& send) {
    	model::Map::Id map_id(std::move(id));
    	const auto* map = app_.FindMap(map_id);

    	if (map != nullptr) {
        	const auto response_body = json::serialize(utils::MapToJson(map));
        	HttpResponseFactory::HandleAPIResponse(
            	http::status::ok,
            	response_body,
            	std::forward<Send>(send)
        	);
        	return { http::status::ok, MimeType::APP_JSON };
    	}

    	HttpResponseFactory::HandleAPIResponse(
        	http::status::not_found,
        	RequestHttpBody::MAP_NOT_FOUND,
        	std::forward<Send>(send)
    	);
    	return { http::status::not_found, MimeType::APP_JSON };
	}


    template<typename Send>
    ResponseData HandleJoinRequest(std::string_view body, Send&& send) {
        json::object json_body;
        try {
            json_body = json::parse(body.data()).as_object();
        } catch (...) {
            HttpResponseFactory::HandleAPIResponse(
                http::status::bad_request,
                RequestHttpBody::JOIN_GAME_PARSE_ERROR,
                std::move(send)
            );
            return {http::status::bad_request, MimeType::APP_JSON};
        }

        const auto validate_field = [&json_body](boost::json::string_view field) {
            return !json_body.contains(field) || !json_body.at(field).is_string();
        };

        if (validate_field("userName")) {
            HttpResponseFactory::HandleAPIResponse(
                http::status::bad_request,
                RequestHttpBody::JOIN_GAME_PARSE_ERROR,
                std::move(send)
            );
            return {http::status::bad_request, MimeType::APP_JSON};
        }

        const auto user_name = json_body.at("userName").get_string();
        if (user_name.empty()) {
            HttpResponseFactory::HandleAPIResponse(
                http::status::bad_request,
                RequestHttpBody::INVALID_NAME,
                std::move(send)
            );
            return {http::status::bad_request, MimeType::APP_JSON};
        }

        if (validate_field("mapId")) {
            HttpResponseFactory::HandleAPIResponse(
                http::status::bad_request,
                RequestHttpBody::JOIN_GAME_PARSE_ERROR,
                std::move(send)
            );
            return {http::status::bad_request, MimeType::APP_JSON};
        }

        const auto map_id = json_body.at("mapId").get_string();
        const model::Map::Id id{map_id.data()};
        auto* session = app_.FindSession(id);

        if (!session) {
            HttpResponseFactory::HandleAPIResponse(
                http::status::not_found,
                RequestHttpBody::MAP_NOT_FOUND,
                std::move(send)
            );
            return {http::status::not_found, MimeType::APP_JSON};
        }

        model::Dog dog{std::string{user_name.data()}};
        auto& player = app_.AddPlayer(std::move(dog), session);

        json::object result;
        app::Token token = player.GetToken();
        result["authToken"] = std::string{*token};
        result["playerId"] = player.GetId();

        HttpResponseFactory::HandleAPIResponse(
            http::status::ok,
            json::serialize(result),
            std::move(send)
        );
        return {http::status::ok, MimeType::APP_JSON};
    }

	template<typename Send>
	ResponseData HandlePlayersRequest(std::string&& token, Send&& send) {
    	app::Token player_token(std::move(token));
    	auto* player = app_.FindByToken(player_token);

    	if (player == nullptr) {
        	HttpResponseFactory::HandleAPIResponse(
            	http::status::unauthorized,
            	RequestHttpBody::TOKEN_UNKNOWN,
            	std::forward<Send>(send)
        	);
        	return { http::status::unauthorized, MimeType::APP_JSON };
    	}

    	json::object result;

    	for (const auto* dog : app_.GetDogs(player))
        	result[std::to_string(dog->GetId())] = json::array{ "name", dog->GetName() };

    	HttpResponseFactory::HandleAPIResponse(
        	http::status::ok,
        	json::serialize(result),
        	std::forward<Send>(send)
    	);

    	return { http::status::ok, MimeType::APP_JSON };
	}

    template<typename Send>
	ResponseData HandleStateRequest(std::string&& token, Send&& send) {
    	app::Token player_token(std::move(token));
    	const auto* player = app_.FindByToken(player_token);

    	if (player == nullptr) {
        	HttpResponseFactory::HandleAPIResponse(
            	http::status::unauthorized,
            	RequestHttpBody::TOKEN_UNKNOWN,
            	std::forward<Send>(send)
        	);
        	return { http::status::unauthorized, MimeType::APP_JSON };
    	}

    	json::object result;
    	json::object players;

    	const auto& dogs = app_.GetDogs(player);

    	for (const auto* dog : dogs) {
        	json::object dog_data;

        	const auto pos = dog->GetPosition();
        	dog_data["pos"] = json::array{ pos.x, pos.y };

        	const auto speed = dog->GetSpeed();
        	dog_data["speed"] = json::array{ speed.vx, speed.vy };

        	switch (dog->GetDirection()) {
            	case model::Direction::NORTH: dog_data["dir"] = "U"; break;
            	case model::Direction::SOUTH: dog_data["dir"] = "D"; break;
                case model::Direction::EAST:  dog_data["dir"] = "R"; break;
				case model::Direction::WEST:  dog_data["dir"] = "L"; break;
        	}
        	players[std::to_string(dog->GetId())] = std::move(dog_data);
    	}

    	result["players"] = std::move(players);

    	HttpResponseFactory::HandleAPIResponse(
        	http::status::ok,
        	json::serialize(result),
        	std::forward<Send>(send)
    	);

    	return { http::status::ok, MimeType::APP_JSON };
	}


	template<typename Send>
	ResponseData HandleActionRequest(std::string&& token, std::string_view body, Send&& send) {
    	auto* player = app_.FindByToken(app::Token(std::move(token)));
    	if (!player) {
        	HttpResponseFactory::HandleAPIResponse(
            	http::status::unauthorized,
            	RequestHttpBody::TOKEN_UNKNOWN,
            	std::move(send)
        	);
        	return {http::status::unauthorized, MimeType::APP_JSON};
    	}

    	json::object json_body;
    	try {
        	json_body = json::parse(body.data()).as_object();
    	} catch (...) {
        	HttpResponseFactory::HandleAPIResponse(
            	http::status::bad_request,
            	RequestHttpBody::ACTION_PARSE_ERROR,
            	std::move(send)
        	);
        	return {http::status::bad_request, MimeType::APP_JSON};
    	}

    	const auto move_it = json_body.find("move");
    	if (move_it == json_body.end() || !move_it->value().is_string()) {
        	HttpResponseFactory::HandleAPIResponse(
            	http::status::bad_request,
            	RequestHttpBody::ACTION_PARSE_ERROR,
            	std::move(send)
        	);
        	return {http::status::bad_request, MimeType::APP_JSON};
    	}

    	const std::string_view move_value = move_it->value().get_string().c_str();

    	if (move_value == "U") { app_.Move(player, model::Direction::NORTH); }
        else if (move_value == "D") { app_.Move(player, model::Direction::SOUTH); }
        else if (move_value == "L") { app_.Move(player, model::Direction::WEST); }
        else if (move_value == "R") { app_.Move(player, model::Direction::EAST); }
        else if (move_value.empty()) { app_.Stop(player); }
        else {
        	HttpResponseFactory::HandleAPIResponse(
            	http::status::bad_request,
            	RequestHttpBody::ACTION_PARSE_ERROR,
            	std::move(send)
        	);
        	return {http::status::bad_request, MimeType::APP_JSON};
    	}

    	HttpResponseFactory::HandleAPIResponse(
        	http::status::ok,
        	"{}"sv,
        	std::move(send)
    	);
    	return {http::status::ok, MimeType::APP_JSON};
	}

    template<typename Send>
	ResponseData HandleTickRequest(std::string_view body, Send&& send) {
    	json::object json_body;

    	try {
        	json_body = json::parse(body.data()).as_object();
    	} catch (...) {
        	HttpResponseFactory::HandleAPIResponse(
            	http::status::bad_request,
            	RequestHttpBody::TICK_PARSE_ERROR,
            	std::forward<Send>(send)
        	);
        	return { http::status::bad_request, MimeType::APP_JSON };
    	}

    	auto tick = json_body.find("timeDelta");

    	if (tick == json_body.end() || !tick->value().is_int64()) {
        	HttpResponseFactory::HandleAPIResponse(
            	http::status::bad_request,
            	RequestHttpBody::TICK_PARSE_ERROR,
            	std::forward<Send>(send)
        	);
        	return { http::status::bad_request, MimeType::APP_JSON };
    	}

    	int tick_val = static_cast<int>(tick->value().get_int64());

    	if (tick_val < 1) {
        	HttpResponseFactory::HandleAPIResponse(
            	http::status::bad_request,
            	RequestHttpBody::TICK_PARSE_ERROR,
            	std::forward<Send>(send)
        	);
        	return { http::status::bad_request, MimeType::APP_JSON };
    	}

    	app_.Tick(tick_val);

    	HttpResponseFactory::HandleAPIResponse(
        	http::status::ok,
        	"{}"sv,
        	std::forward<Send>(send)
    	);

    	return { http::status::ok, MimeType::APP_JSON };
	}

    bool ParseBearer(const std::string_view auth_header, std::string& token_to_write) const;
};


}