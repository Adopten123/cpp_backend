#pragma once

#include "request_handler.h"
#include "boost_json.cpp"
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/beast/http.hpp>
#include <chrono>

namespace http_handler {

namespace json = boost::json;
using namespace std::literals;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

class LoggingHandler {
public:
    explicit LoggingHandler(RequestHandler& handler)
        : handler_(handler) {
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        using namespace std::chrono;
        auto start_time = steady_clock::now();

        std::string client_ip = "unknown";
        if (auto remote_endpoint = handler_.GetStream().socket().remote_endpoint();
            remote_endpoint.address().is_v4()) {
            client_ip = remote_endpoint.address().to_string();
        }

        json::value request_data{
            {"ip", client_ip},
            {"URI", std::string(req.target())},
            {"method", std::string(req.method_string())}
        };

        BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data, request_data)
                                << "request received"sv;

        auto logged_send = [start_time, send = std::forward<Send>(send)](auto&& response) mutable {
            auto end_time = steady_clock::now();
            auto duration = duration_cast<milliseconds>(end_time - start_time).count();

            json::value response_data{
                {"response_time", duration},
                {"code", response.result_int()},
                {"content_type", std::string(response[http::field::content_type])}
            };

            BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data, response_data)
                                    << "response sent"sv;

            send(std::forward<decltype(response)>(response));
        };

        handler_(std::move(req), std::move(logged_send));
    }

private:
    RequestHandler& handler_;
};

} // namespace http_handler