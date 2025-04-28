#pragma once

#include "request_handler.h"

namespace http_handler {

using namespace std::literals;
namespace json = boost::json;
namespace http = beast::http;
namespace logging = boost::log;


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
    static void LogResponse(const RequestHandler::ResponseData& r, double response_time, const boost::beast::net::ip::address& address) {
        json::object response_data;
        response_data["ip"] = address.to_string();
        response_data["response_time"] = (int)(response_time * 1000);
        response_data["code"] = (int)r.code;
        response_data["content_type"] = r.content_type.data();
        BOOST_LOG_TRIVIAL(info) << logging::add_value(data, response_data) << "response sent";
    }
};

} // namespace http_handler
