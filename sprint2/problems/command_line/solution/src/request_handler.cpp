#include "request_handler.h"

#include <ranges>

namespace http_handler {

RequestHandler::RequestHandler(app::Application& app, const char* path_to_static, net::io_context& ioc, bool no_auto_tick)
    : root_path_(path_to_static),
    api_handler_(std::make_shared<APIRequestHandler>(app, ioc, no_auto_tick)){
}


RequestHandler::RequestType RequestHandler::CheckRequest(std::string_view target) const {
    if (target.starts_with(RestApiLiterals::API_V1)) {
        return RequestHandler::RequestType::API;
    }
    if (target.starts_with("/api")) {
        return RequestHandler::RequestType::BAD_REQUEST;
    }
    auto request = SplitRequest(target.substr(1, target.length() - 1));
    auto temp_path = root_path_;
    temp_path += target;
    auto path = fs::weakly_canonical(temp_path);
    auto canonical_root = fs::weakly_canonical(root_path_);
    for (auto b = canonical_root.begin(), p = path.begin(); b != canonical_root.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return RequestHandler::RequestType::BAD_REQUEST;
        }
    }
    return RequestHandler::RequestType::FILE;
}

std::string RequestHandler::DecodeURL(std::string_view url) const {
    std::vector<char> text;
    text.reserve(url.length());
    for (size_t i = 0; i < url.length(); ++i) {
        if (url[i] == '%') {
            if (i + 2 < url.length()) {
                char hex1 = url[i + 1];
                char hex2 = url[i + 2];

                if (isxdigit(hex1) && isxdigit(hex2)) {
                    int code = std::stoi(std::string() + hex1 + hex2, nullptr, 16);
                    text.emplace_back(static_cast<char>(code));
                    i += 2;
                }
                else {
                    text.emplace_back('%');
                }
            }
            else {
                text.emplace_back('%');
            }
        }
        else if (url[i] == '+') {
            text.emplace_back(' ');
        }
        else {
            text.emplace_back(url[i]);
        }
    }
    return std::string(text.data(), text.size());
}

}  // namespace http_handler