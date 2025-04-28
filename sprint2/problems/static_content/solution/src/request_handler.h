#pragma once

#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <filesystem>
#include <unordered_map>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, const std::filesystem::path& static_root)
        : game_{game}, static_root_{static_root} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    void HandleMaps(const http::request<http::string_body>& req, http::response<http::string_body>& res);
    void HandleMap(const std::string& id, http::response<http::string_body>& res);
    void HandleStaticFile(const http::request<http::string_body>& req, http::response<http::string_body>& res);

    void HandleBadRequest(http::response<http::string_body>& res, const std::string& message);
    void HandleNotFound(http::response<http::string_body>& res, const std::string& message, bool is_api_request);

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string path(req.target().data(), req.target().size());

        http::response<http::string_body> res;
        res.set(http::field::content_type, "application/json");

        if (path.starts_with("/api/")) {
            if (req.method() != http::verb::get) {
                HandleBadRequest(res, "Invalid Method");
                return send(std::move(res));
            }

            if (path.starts_with("/api/v1/maps")) {
                if (path == "/api/v1/maps") {
                    HandleMaps(req, res);
                } else if (auto pos = path.find("/api/v1/maps/"); pos != std::string::npos) {
                    std::string id = path.substr(pos + strlen("/api/v1/maps/"));
                    HandleMap(id, res);
                } else {
                    HandleBadRequest(res, "Invalid map ID format");
                }
            } else {
                HandleBadRequest(res, "Invalid API endpoint");
            }
        } else {
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                HandleBadRequest(res, "Invalid Method");
                return send(std::move(res));
            }
            HandleStaticFile(req, res);
        }

        send(std::move(res));
    }

private:
    json::value SerializeMap(const model::Map& map);
    json::value SerializeMaps();
    static std::string URLDecode(std::string_view url);
    static int HexToInt(char c);

    model::Game& game_;
    std::filesystem::path static_root_;
};

}  // namespace http_handler