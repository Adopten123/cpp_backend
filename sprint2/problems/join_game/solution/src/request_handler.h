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

struct MimeType {
    MimeType() = delete;
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

struct RestApiLiterals {
    RestApiLiterals() = delete;
    constexpr static std::string_view API = "api"sv;
    constexpr static std::string_view VERSION_1 = "v1"sv;
    constexpr static std::string_view MAPS = "maps"sv;
};

struct RequestHttpBody {
    RequestHttpBody() = delete;
    constexpr static std::string_view BAD_REQUEST = R"({ "code": "badRequest", "message": "Bad request" })"sv;
    constexpr static std::string_view MAP_NOT_FOUND = R"({ "code": "mapNotFound", "message": "Map not found" })"sv;
    constexpr static std::string_view FILE_NOT_FOUND = R"({ "code": "fileNotFound", "message": "File not found" })"sv;
    constexpr static std::string_view INVALID_NAME = R"({ "code": "invalidArgument", "message": "Invalid name" })"sv;
    constexpr static std::string_view JOIN_GAME_PARSE_ERROR = R"({ "code": "invalidArgument", "message": "Join game request parse error" })"sv;
    constexpr static std::string_view METHOD_NOT_ALLOWED = R"({ "code": "invalidMethod", "message": "Another method expected" })"sv;
    constexpr static std::string_view INVALID_TOKEN_ = R"({ "code": "invalidToken", "message": "Authorization header is missing" })"sv;
    constexpr static std::string_view TOKEN_UNKNOWN = R"({ "code": "unknownToken", "message": "Player token has not been found" })"sv;
};

std::vector<std::string_view> SplitRequest(std::string_view body);

json::object SerializeMap(const model::Map* map);
json::array SerializeRoads(const model::Map* map);
json::array SerializeOffices(const model::Map* map);
json::array SerializeBuildings(const model::Map* map);

struct ResponseData {
    http::status code;
    std::string_view content_type;
};

class HttpResponseFactory {
public:
    HttpResponseFactory() = delete;

    template<typename Send>
    ResponseData HandleBadRequest(Send&& send, unsigned http_version, bool is_head_request = false) const {
        HandleResponse(
            http::status::bad_request,
            RequestHttpBody::BAD_REQUEST,
            http_version,
            std::move(send),
            MimeType::APP_JSON,
            is_head_request
        );

        return {
            http::status::bad_request,
            MimeType::APP_JSON
        };
    }

    template<typename Send>
	ResponseData HandleStaticFilesOr404(std::string_view path, Send&& send, unsigned http_version, bool is_head_request = false) const {
    	using namespace http;

    	response<file_body> response;
    	response.version(http_version);
    	response.result(status::ok);

    	const auto resolve_resource = [this](std::string_view uri_path) {
        	const auto extension_pos = uri_path.find_last_of('.');
        	std::string full_path = root_path_.string() + uri_path.data();
        	std::string_view mime_type = MimeType::TEXT_HTML;

        	if (extension_pos != uri_path.npos) {
            	auto file_extension = uri_path.substr(extension_pos + 1);
            	mime_type = GetMimeType(file_extension);
        	} else {
            	full_path = root_path_.string() + "/index.html";
        	}

        	return std::make_tuple(full_path, mime_type);
    	};

    	auto [file_path, content_type] = resolve_resource(path);
    	response.set(field::content_type, content_type);

    	file_body::value_type file_stream;
    	sys::error_code file_error;

    	file_stream.open(file_path.c_str(), beast::file_mode::read, file_error);

    	if (file_error) {
        	HandleResponse(
            	status::not_found,
            	RequestHttpBody::FILE_NOT_FOUND,
            	http_version,
            	std::forward<Send>(send),
            	MimeType::TEXT_PLAIN,
                is_head_request
        	);
        	return {status::not_found, MimeType::TEXT_PLAIN};
    	}
        if(!is_head_request)
            response.body() = std::move(file_stream);

    	response.prepare_payload();
    	send(std::move(response));

    	return {status::ok, content_type};
	}

    template<typename Send>
    void HandleResponse(http::status status, std::string_view body, unsigned http_version, Send&& send, std::string_view type, bool is_head_request = false) const {
        http::response<http::string_body> response(status, http_version);
        response.insert(http::field::content_type, type);

        if(!is_head_request)
            response.body() = body;

        send(response);
    }

    template<typename Send>
    static void HandleAPIResponse(http::status status, std::string_view body, unsigned http_version, Send&& send, bool is_head_method = false) {
        http::response<http::string_body> response(status, http_version);
        response.insert(http::field::content_type, ContentType::APP_JSON);
        response.insert(http::field::cache_control, "no-cache");

        if (!is_head_method)
            response.body() = body;

        response.prepare_payload();
        send(response);
    }

    template<typename Send>
    static ResponseData SendMethodNotAllowed(unsigned http_version, Send&& send, std::string_view allow) {
        http::response<http::string_body> response(http::status::method_not_allowed, http_version);
        response.insert(http::field::content_type, ContentType::APP_JSON);
        response.insert(http::field::cache_control, "no-cache");
        response.insert(http::field::allow, allow);
        response.body() = RequestHttpBody::METHOD_NOT_ALLOWED;
        send(response);

        return {
            http::status::method_not_allowed,
            ContentType::APP_JSON
        };
    }
private:
    static std::string_view GetMimeType(std::string_view extension);
}

class PlayerSessionAPIHandler : public std::enable_shared_from_this<APIRequestHandler> {
public:
    using Strand = net::strand<net::io_context::executor_type>;

    explicit PlayerSessionAPIHandler(model::Game& game, net::io_context& ioc)
        : game_{ game }
        , strand_(net::make_strand(ioc) {
    }

    PlayerSessionAPIHandler(const PlayerSessionAPIHandler&) = delete;
    PlayerSessionAPIHandler& operator=(const PlayerSessionAPIHandler&) = delete;

    template <typename Send>
    ResponseData ProcessRequest(std::string_view target, unsigned http_version,
                 std::string_view method, Send&& send, const json::object& body, const std::string_view bearer) {
        auto unslashed = target.substr(1, target.length() - 1);
        auto splitted = SplitRequest(unslashed);

        if (splitted.size() < 3)
            return HttpResponseFactory::HandleBadRequest(std::move(send), http_version);

        if (splitted[2] == RestApiLiterals::MAPS) {
            if (splitted.size() == 4) {
                std::string id_text(splitted[3].data(), splitted[3].size());
                model::Map::Id id(id_text);
                const auto* map = game_.FindMap(id);
                if (map) {
                    HttpResponseFactory::HandleAPIResponse(http::status::ok, json::serialize(SerializeMap(map)), http_version, std::move(send));
                    return {
                        http::status::ok,
                        ContentType::APP_JSON
                    };
                }
                else {
                    HttpResponseFactory::HandleAPIResponse(http::status::not_found, RequestHttpBody::MAP_NOT_FOUND, http_version, std::move(send));
                    return {
                        http::status::not_found,
                        ContentType::APP_JSON
                    };
                }
            }
            if (splitted.size() == 3) {
                HttpResponseFactory::HandleAPIResponse(http::status::ok, json::serialize(ProcessMapsRequestBody()), http_version, std::move(send));
                return { http::status::ok , ContentType::APP_JSON };
            }
            if (splitted[2] == RestApiLiterals::MAP) {
                if (splitted.size() != 4)
                    return HttpResponseFactory::HandleBadRequest(std::move(send), http_version);
                std::string id_text(splitted[3].data(), splitted[3].size());
                model::Map::Id id(id_text);
                const auto* map = game_.FindMap(id);
                if (map) {
                    HttpResponseFactory::HandleAPIResponse(http::status::ok, json::serialize(SerializeMap(map)), http_version, std::move(send));
                    return {
                        http::status::ok ,
                        ContentType::APP_JSON
                    };
                }
                else {
                    HttpResponseFactory::HandleAPIResponse(http::status::not_found, RequestHttpBody::MAP_NOT_FOUND, http_version, std::move(send));
                    return {
                        http::status::not_found ,
                        ContentType::APP_JSON
                    };
                }
            }
            if (splitted[2] == RestApiLiterals::GAME) {
                if (splitted.size() < 4)
                    return HttpResponseFactory::HandleBadRequest(std::move(send), http_version);

                if (splitted[3] == RestApiLiterals::JOIN) {
                    if (method != "POST")
                        return HttpResponseFactory::HandleMethodNotAllowed(http_version, std::move(send), "POST");
                    if (!body.contains("userName") || !body.at("userName").is_string()) {
                        HttpResponseFactory::HandleAPIResponse(http::status::bad_request, RequestHttpBody::JOIN_GAME_PARSE_ERROR, http_version, std::move(send));
                        return {
                            http::status::bad_request,
                            ContentType::APP_JSON
                        };
                    }
                    auto user_name = body.at("userName").get_string();
                    if (user_name.size() == 0) {
                        HttpResponseFactory::HandleAPIResponse(http::status::bad_request, RequestHttpBody::INVALID_NAME, http_version, std::move(send));
                        return {
                            http::status::bad_request,
                            ContentType::APP_JSON
                        };
                    }
                    if (!body.contains("mapId") || !body.at("mapId").is_string()) {
                        HttpResponseFactory::HandleAPIResponse(http::status::bad_request, RequestHttpBody::JOIN_GAME_PARSE_ERROR, http_version, std::move(send));
                        return {
                            http::status::bad_request,
                            ContentType::APP_JSON
                        };
                    }
                    auto map_id = body.at("mapId").get_string();
                    auto id = model::Map::Id(map_id.data());
                    auto* session = game_.FindSession(id);
                    if (!session) {
                        HttpResponseFactory::HandleAPIResponse(http::status::not_found, RequestHttpBody::MAP_NOT_FOUND, http_version, std::move(send));
                        return {
                            http::status::not_found,
                            ContentType::APP_JSON
                        };
                    }
                    model::Dog dog{ std::move(std::string(user_name.data())) };
                    auto& player = players_.AddPlayer(std::move(dog), session);
                    json::object result;
                    player.GetDog();
                    player.GetSession();
                    app::Token token = player.GetToken();
                    std::string tokenStr = *token;
                    result["authToken"] = tokenStr;
                    result["playerId"] = player.GetId();
                    HttpResponseFactory::HandleAPIResponse(http::status::ok, json::serialize(result), http_version, std::move(send));
                    return {
                        http::status::ok,
                        ContentType::APP_JSON
                    };
                }
                if (splitted[3] == RestApiLiterals::PLAYERS) {
                    if (method != "GET" && method != "HEAD")
                        return HttpResponseFactory::HandleMethodNotAllowed(http_version, std::move(send), "GET, HEAD");
                    std::string token = "";
                    auto token_valid = ParseBearer(std::move(bearer), token);
                    if (!token_valid) {
                        HttpResponseFactory::HandleAPIResponse(http::status::unauthorized, RequestHttpBody::INVALID_TOKEN, http_version, std::move(send));
                        return {
                            http::status::unauthorized,
                            ContentType::APP_JSON
                        };
                    }
                    auto* player = players_.FindByToken(app::Token(token.data()));
                    if (!player) {
                        HttpResponseFactory::HandleAPIResponse(http::status::unauthorized, RequestHttpBody::TOKEN_UNKNOWN, http_version, std::move(send));
                        return {
                            http::status::unauthorized,
                            ContentType::APP_JSON
                        };
                    }
                    auto dogs = player->GetSession()->GetDogs();
                    json::object result;
                    for (const auto* dog : dogs)
                        result[std::to_string(dog->GetId())] = json::array{ "name", dog->GetName()};
                    HttpResponseFactory::HandleAPIResponse(http::status::ok, json::serialize(result), http_version, std::move(send));
                    return {
                        http::status::ok,
                        ContentType::APP_JSON
                    };
                }
            }
        }
        return HttpResponseFactory::HandleBadRequest(std::move(send), http_version);
    }

    Strand& GetStrand() { return strand_; }
private:
    model::Game& game_;
    Strand strand_;
    app::Players players_;

    json::array ProcessMapsRequestBody() const;
    bool ExtractBearerTokenFromHeader(const std::string_view auth_header, std::string& token_to_write) const;
}

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    explicit RequestHandler(model::Game& game, const char* path_to_static, net::io_context& ioc)
        : game_{ game }
        , root_path_(path_to_static)
        , {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    ResponseData operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        auto string_path = URLDecode(req.target());
        std::string_view path(string_path);

        switch(CheckRequest(path)) {
        case RequestType::API:
            json::object body;
            if (req.body().size() != 0) {
                try {
                    body = json::parse(req.body()).as_object();
                }
                catch (...) {
                    HttpResponseFactory::HandleAPIResponse(http::status::bad_request, RequestHttpBody::JOIN_GAME_PARSE_ERROR, req.version(), std::move(send));
                    return handle({ http::status::bad_request, ContentType::APP_JSON });
                }
            }
            net::dispatch(api_handler_->GetStrand(), [self = shared_from_this(), string_target_ = std::move(string_target)
                                                     , req_ = std::move(req), send_ = std::move(send), api_handler__ = api_handler_->shared_from_this()
                                                     , handle, body_ = std::move(body)]() {
                    handle(api_handler_->ProcessRequest(std::string_view(string_target_), (unsigned)(req_.version()), std::string_view(req_.method_string().data())
                                        , std::move(send_), body_, req_.base()[http::field::authorization]));
                });
            return;
            break;
        case RequestType::FILE:
            return handle(HttpResponseFactory::HandleStaticFilesOr404(path, std::move(send), req.version()));
            break;
        case RequestType::BAD_REQUEST:
            return handle(HttpResponseFactory::HandleBadRequest(std::move(send), req.version()));
            break;
        default:
            return handle(HttpResponseFactory::HandleBadRequest(std::move(send), req.version()));
            break;
        }
    }

private:
    friend PlayerSessionAPIHandler;
    enum RequestType {
        API,
        API,
        FILE,
        BAD_REQUEST
    };

    model::Game& game_;
    const fs::path root_path_;
    std::shared_ptr<PlayerSessionAPIHandler> api_handler_;

    RequestType CheckRequest(std::string_view target) const;
    std::string URLDecode(std::string_view url) const;
    static int HexToInt(char c);
};

class LoggingRequestHandler {
public:
    LoggingRequestHandler(RequestHandler& handler)
        : decorated_(handler) {
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
        RequestHandler::ResponseData resp_data = decorated_(std::move(req), std::move(send));
        boost::chrono::duration<double> response_time = boost::chrono::system_clock::now() - start;
        LogResponse(resp_data, response_time.count(), address);
    }

private:
    RequestHandler& decorated_;

    template <typename Body, typename Allocator>
    static void LogRequest(const http::request<Body, http::basic_fields<Allocator>>& r, const boost::beast::net::ip::address& address) {
        json::object request_data;
        request_data["ip"] = address.to_string();
        request_data["URI"] = std::string(r.target());
        request_data["method"] = r.method_string().data();
        BOOST_LOG_TRIVIAL(info) << logging::add_value(data, request_data) << "request received";
    }
    static void LogResponse(const RequestHandler::ResponseData& r, double response_time, const boost::beast::net::ip::address& address);
};

}  // namespace http_handler