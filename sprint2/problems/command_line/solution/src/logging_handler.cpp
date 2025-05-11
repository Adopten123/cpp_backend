#include "logging_handler.h"

namespace http_handler {

void LoggingRequestHandler::LogResponse(const ResponseData& r, boost::chrono::system_clock::time_point start_time, const boost::beast::net::ip::address&& address) {
    boost::chrono::duration<double> response_time = boost::chrono::system_clock::now() - start_time;
    json::object response_data;
    response_data["ip"] = address.to_string();
    response_data["response_time"] = (int)(response_time.count() * 1000);
    response_data["code"] = (int)r.code;
    response_data["content_type"] = r.content_type.data();
    BOOST_LOG_TRIVIAL(info) << logging::add_value(data, response_data) << "response sent";
}


}