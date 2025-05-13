#include "request_handler.h"

#include <ranges>

namespace http_handler {

RequestHandler::RequestHandler(app::Application& app, const char* path_to_static, net::io_context& ioc, bool no_auto_tick)
    : root_path_(path_to_static),
    api_handler_(std::make_shared<APIHandler>(app, ioc, no_auto_tick)){
}


RequestHandler::RequestType RequestHandler::CheckRequest(std::string_view target) const {
    if (target.starts_with(RestApiLiteral::API_V1)) {
        return RequestHandler::RequestType::API;
    }
    if (target.starts_with("/api")) {
        return RequestHandler::RequestType::BAD_REQUEST;
    }
    auto request = utils::SplitRequest(target.substr(1, target.length() - 1));
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

std::string RequestHandler::URLDecode(std::string_view url) const {
    std::string result;
    result.reserve(url.size());
    for (size_t i = 0; i < url.size(); ++i) {
        if (url[i] == '%') {
            if (i + 2 < url.size()) {
                int hi = HexToInt(url[i+1]);
                int lo = HexToInt(url[i+2]);
                if (hi == -1 || lo == -1) {
                    result += url[i];
                } else {
                    result += static_cast<char>((hi << 4) | lo);
                    i += 2;
                }
            } else {
                result += url[i];
            }
        } else if (url[i] == '+') {
            result += ' ';
        } else {
            result += url[i];
        }
    }
    return result;
}

int RequestHandler::HexToInt(char c) const {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    return -1;
}

}  // namespace http_handler