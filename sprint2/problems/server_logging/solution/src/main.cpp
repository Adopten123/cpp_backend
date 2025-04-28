#include "sdk.h"
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <thread>
#include <filesystem>
#include <chrono>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "json_loader.h"
#include "request_handler.h"
#include "logging_handler.h"

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value)

using namespace std::literals;
namespace net = boost::asio;
namespace logging = boost::log;
namespace json = boost::json;

namespace {

void InitLogger() {
    logging::add_common_attributes();

    auto fmtTimeStamp = logging::expressions::format_date_time<boost::posix_time::ptime>(
        "TimeStamp", "%Y-%m-%dT%H:%M:%S.%f");
    auto fmtAdditionalData = logging::expressions::attr<json::value>("AdditionalData");

    auto jsonFormatter = [fmtTimeStamp, fmtAdditionalData](logging::record_view const& rec, logging::formatting_ostream& strm) {
        json::object logEntry;

        if (auto ts = logging::extract<boost::posix_time::ptime>("TimeStamp", rec))
            logEntry["timestamp"] = to_iso_extended_string(ts.get());

        if (auto msg = logging::extract<std::string>("Message", rec))
            logEntry["message"] = msg.get();

        if (auto data = logging::extract<json::value>("AdditionalData", rec))
            logEntry["data"] = data.get();

        strm << json::serialize(logEntry);
    };

    auto consoleSink = logging::add_console_log(std::cout);
    consoleSink->set_formatter(jsonFormatter);
    consoleSink->set_filter(logging::trivial::severity >= logging::trivial::info);
}

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: game_server <game-config-json> <static-files-dir>"sv << std::endl;
        return EXIT_FAILURE;
    }

    try {
        InitLogger();
        model::Game game = json_loader::LoadGame(argv[1]);
        const std::filesystem::path static_root(argv[2]);
        if (!std::filesystem::exists(static_root) || !std::filesystem::is_directory(static_root)) {
            throw std::runtime_error("Static directory not found or is not a directory");
        }

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code& ec, int signal_number) {
            if (!ec) {
                BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, json::value{{"code", 0}})
                                         << "server exited"sv;
                ioc.stop();
            }
        });

        http_handler::RequestHandler base_handler{game, static_root};
        http_handler::LoggingHandler logging_handler{base_handler};

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;

        json::value start_data{
            {"port", port},
            {"address", address.to_string()}
        };
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, start_data)
                                << "server started"sv;

        http_server::ServeHttp(ioc, {address, port}, [&logging_handler](auto&& req, auto&& send) {
            logging_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    } catch (...) {
        json::value exit_data{
            {"code", EXIT_FAILURE},
            {"exception", "unknown error"}
        };
        BOOST_LOG_TRIVIAL(error) << logging::add_value(additional_data, exit_data)
                                 << "server exited"sv;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}