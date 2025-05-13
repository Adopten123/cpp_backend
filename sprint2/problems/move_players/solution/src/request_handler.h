#pragma once

#include "api_handler.h"

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

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    explicit RequestHandler(app::Application& app, const char* path_to_static, net::io_context& ioc, bool no_auto_tick);

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    auto& GetStrand() {
        return api_handler_->GetStrand();
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, std::function<void(ResponseData&&)> handle) {
        auto string_target = URLDecode(req.target());
        std::string_view target(string_target);
        switch(CheckRequest(target)) {
        case RequestType::API:
        {
            net::dispatch(api_handler_->GetStrand(), [self = shared_from_this(), string_target_ = std::move(string_target)
                                                     , req_ = std::move(req), send_ = std::move(send), api_handler__ = api_handler_->shared_from_this()
                                                     , handle_ = std::move(handle)]() {
                    handle_(api_handler__->ProcessRequest(std::string_view(string_target_), std::move(send_), std::move(req_)));
                });
            return;
            break;
        }
        case RequestType::FILE:
            return handle(HttpResponseFactory::HandleFileResponseOr404(root_path_, target, std::move(send)));
            break;
        case RequestType::BAD_REQUEST:
            return handle(HttpResponseFactory::HandleBadRequest(std::move(send)));
            break;
        default:
            return handle(HttpResponseFactory::HandleBadRequest(std::move(send)));
            break;
        }
    }

private:
    friend APIHandler;

    const fs::path root_path_;
    std::shared_ptr<APIHandler> api_handler_;

    enum RequestType {
        API,
        FILE,
        BAD_REQUEST
    };

    RequestType CheckRequest(std::string_view target) const;
    std::string URLDecode(std::string_view url) const;
    int HexToInt(char c) const;
};

}  // namespace http_handler