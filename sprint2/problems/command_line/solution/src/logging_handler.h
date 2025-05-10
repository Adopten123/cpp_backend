#pragma once
#include "request_handler.h"

namespace http_handler {

class LoggingRequestHandler {
    template <typename Body, typename Allocator>
    static void LogRequest(const http::request<Body, http::basic_fields<Allocator>>& r, const boost::beast::net::ip::address& address) {
        json::object request_data;
        request_data["ip"] = address.to_string();
        request_data["URI"] = std::string(r.target());
        request_data["method"] = r.method_string().data();
        BOOST_LOG_TRIVIAL(info) << logging::add_value(data, request_data) << "request received";
    }
    static void LogResponse(const ResponseData& r, boost::chrono::system_clock::time_point start_time, const boost::beast::net::ip::address&& address);
public:
    explicit LoggingRequestHandler(std::shared_ptr<RequestHandler> handler) : decorated_(handler) {
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
        auto handle { [address, start](ResponseData&& resp_data) {
                LogResponse(std::move(resp_data), start, std::move(address));
            }};
        decorated_->operator()(std::move(req), std::move(send), handle);
    }

private:
    std::shared_ptr<RequestHandler> decorated_;
};

}  // namespace http_handler