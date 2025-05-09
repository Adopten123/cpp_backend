#include "api_handler.h"

namespace http_handler {

APIRequestHandler::APIRequestHandler(app::Application& app, net::io_context& ioc, bool no_auto_tick)
    : app_{ app },
    strand_(net::make_strand(ioc)),
    auto_tick_(!no_auto_tick){
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

}