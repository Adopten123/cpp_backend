#pragma once
#include "http_server.h"
#include "model.h"

#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/date_time.hpp>
#include <boost/chrono.hpp>
#include <filesystem>

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

std::vector<std::string_view> SplitRequest(std::string_view body);

json::object SerializeMap(const model::Map* map);
json::array SerializeRoads(const model::Map* map);
json::array SerializeOffices(const model::Map* map);
json::array SerializeBuildings(const model::Map* map);

class RequestHandler {
public:

    explicit RequestHandler(model::Game& game, const char* path_to_static)
        : game_{ game }
        , root_path_(path_to_static) {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    struct ResponseData {
        http::status code;
        std::string_view content_type;
    };

    template <typename Body, typename Allocator, typename Send>
    ResponseData operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        auto string_path = URLDecode(req.target());
        std::string_view path(string_path);

        switch(CheckRequest(path)) {
        case RequestType::API_MAPS:
            HandleResponse(
                http::status::ok,
                json::serialize(ProcessMapsRequestBody()),
                req.version(),
                std::move(send),
                MimeType::APP_JSON
            );
            return {
                http::status::ok ,
                MimeType::APP_JSON
            };
            break;
        case RequestType::API_MAP:
        {
            auto request = SplitRequest(path.substr(1, path.length() - 1));
            std::string id_text(request[3].data(), request[3].size());
            model::Map::Id id(id_text);
            const auto* map = game_.FindMap(id);
            if (map) {
                HandleResponse(
                    http::status::ok,
                    json::serialize(SerializeMap(map)),
                    req.version(),
                    std::move(send),
                    MimeType::APP_JSON
                );
                return {
                    http::status::ok ,
                    MimeType::APP_JSON
                };
            }
            else {
                HandleResponse(
                    http::status::not_found,
                    RequestHttpBody::MAP_NOT_FOUND_HTTP_BODY,
                    req.version(),
                    std::move(send),
                    MimeType::APP_JSON
                );
                return {
                    http::status::not_found,
                    MimeType::APP_JSON
                };
            }
            break;
        }
        case RequestType::FILE:
            return HandleStaticFilesOr404(path, std::move(send), req.version());
            break;
        case RequestType::BAD_REQUEST:
            return HandleBadRequest(std::move(send), req.version());
            break;
        default:
            return HandleBadRequest(std::move(send), req.version());
            break;
        }
    }

private:
    enum RequestType {
        API_MAP,
        API_MAPS,
        FILE,
        BAD_REQUEST
    };

    struct RequestHttpBody {
    	RequestHttpBody() = delete;
        constexpr static std::string_view BAD_REQUEST_HTTP_BODY = R"({ "code": "badRequest", "message": "Bad request" })"sv;
    	constexpr static std::string_view MAP_NOT_FOUND_HTTP_BODY = R"({ "code": "mapNotFound", "message": "Map not found" })"sv;
    	constexpr static std::string_view FILE_NOT_FOUND_HTTP_BODY = R"({ "code": "fileNotFound", "message": "File not found" })"sv;
    };

    model::Game& game_;
    const fs::path root_path_;

    template<typename Send>
    ResponseData HandleBadRequest(Send&& send, unsigned http_version) const {
        HandleResponse(http::status::bad_request, RequestHttpBody::BAD_REQUEST_HTTP_BODY, http_version, std::move(send), MimeType::APP_JSON);

        return {
            http::status::bad_request,
            MimeType::APP_JSON
        };
    }

    template<typename Send>
    ResponseData HandleStaticFilesOr404(std::string_view path, Send&& send, unsigned http_version) const {
        namespace fs = std::filesystem;

    	// Формирование полного пути
    	fs::path full_path = root_path_;
    	full_path /= request_path.empty() ? "index.html" : request_path;

    	// Определение MIME-типа
    	std::string_view content_type = MimeType::TEXT_HTML;
    	if (const auto ext = full_path.extension(); !ext.empty()) {
        	content_type = GetMimeType(ext.string().substr(1));
    	}

    	// Подготовка ответа
    	http::response<http::file_body> response;
    	response.version(http_version);
    	response.set(http::field::content_type, content_type);

    	// Попытка открыть файл
    	beast::error_code ec;
    	response.body().open(full_path.c_str(), beast::file_mode::read, ec);

    	if (ec) {
        	// Fallback для index.html если путь был директорией
        	if (request_path.empty() || request_path.back() == '/') {
            	full_path = root_path_ / "index.html";
            	response.body().open(full_path.c_str(), beast::file_mode::read, ec);
        	}

        if (ec) {
            	SendErrorResponse(http::status::not_found,
                             HttpBody::FILE_NOT_FOUND,
                             http_version,
                             std::forward<Send>(send));
            	return {http::status::not_found, MimeType::TEXT_PLAIN};
        	}
    	}

    	// Отправка успешного ответа
    	response.result(response.body().is_open() ? http::status::ok
                                            : http::status::not_found);
    	response.prepare_payload();
    	send(std::move(response));

    	return {http::status::ok, content_type};
    }

    template<typename Send>
    void HandleResponse(http::status status, std::string_view body, unsigned http_version, Send&& send, std::string_view type) const {
        http::response<http::string_body> response(status, http_version);
        response.insert(http::field::content_type, type);
        response.body() = body;
        send(response);
    }

    RequestType CheckRequest(std::string_view target) const;
    json::array ProcessMapsRequestBody() const;
    std::string URLDecode(std::string_view url) const;
    static int HexToInt(char c);
    static std::string_view GetMapperType(std::string_view extension);
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