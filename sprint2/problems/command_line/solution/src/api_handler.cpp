#include "api_handler.h"

namespace http_handler {

APIRequestHandler::APIRequestHandler(app::Application& app, net::io_context& ioc, bool no_auto_tick)
    : app_{ app },
    strand_(net::make_strand(ioc)),
    auto_tick_(!no_auto_tick) {
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

template <typename Body, typename Allocator, typename Send>
ResponseData APIRequestHandler::ProcessRequest(std::string_view target, Send&& send, const http::request<Body, http::basic_fields<Allocator>>&& req) {
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

}  // namespace http_handler