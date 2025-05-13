#include "http_server.h"

#include <iostream>

#include <boost/json.hpp>
//#include <boost/log/attributes.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

BOOST_LOG_ATTRIBUTE_KEYWORD(error_data, "AdditionalData", boost::json::value);

namespace http_server {

void ReportError(beast::error_code ec, std::string_view where) {
    boost::json::value data{ {"code", ec.value()}, {"text", ec.message()}, {"where", where}};
    BOOST_LOG_TRIVIAL(error) << boost::log::add_value(error_data, data) << "error";
}

void SessionBase::Run() {
    // Вызываем метод Read, используя executor объекта stream_.
    // Таким образом вся работа со stream_ будет выполняться, используя его executor
    net::dispatch(stream_.get_executor(),
        beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

}  // namespace http_server