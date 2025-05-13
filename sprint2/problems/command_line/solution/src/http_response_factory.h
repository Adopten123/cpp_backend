#pragma once

#include "handler_utils.h"

namespace http_handler {

namespace sys = boost::system;
namespace fs = std::filesystem;
namespace beast = boost::beast;
namespace http = beast::http;

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
        std::string_view type = MimeType::TEXT_HTML;

        std::size_t ext_pos = path.find_last_of('.');
        if (ext_pos != std::string_view::npos) {
            type = utils::GetMimeType(path.substr(ext_pos + 1));
        } else {
            full_path = root_path.string() + "/index.html";
        }

        res.set(http::field::content_type, type);

        http::file_body::value_type file;
        sys::error_code ec;
        file.open(full_path.c_str(), beast::file_mode::read, ec);

        if (ec) {
            HandleResponse(http::status::not_found, RequestHttpBody::FILE_NOT_FOUND, std::forward<Send>(send), MimeType::TEXT_PLAIN, is_head_method);
            return { http::status::not_found, MimeType::TEXT_PLAIN };
        }

        if (!is_head_method)
            res.body() = std::move(file);

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

}