#include "sdk.h"
//
#include <boost/system/error_code.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <thread>
#include <filesystem>

#include "json_loader.h"
#include "request_handler.h"

using namespace std::literals;
namespace net = boost::asio;

namespace {

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
        model::Game game = json_loader::LoadGame(argv[1]);
        const std::filesystem::path static_root(argv[2]);
        if (!std::filesystem::exists(static_root) || !std::filesystem::is_directory(static_root)) {
            throw std::runtime_error("Static directory not found or is not a directory");
        }

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code& ec, int signal_number) {
            if (!ec)
                ioc.stop();
        });

        http_handler::RequestHandler handler{game, static_root};

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            using RequestType = std::decay_t<decltype(req)>;
            handler(std::forward<RequestType>(req), std::forward<decltype(send)>(send));
        });
        std::cout << "Server has started..."sv << std::endl;

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}