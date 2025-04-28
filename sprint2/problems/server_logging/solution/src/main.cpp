#include "sdk.h"
//
#include <boost/asio/signal_set.hpp>
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <thread>
#include <filesystem>

#include "json_loader.h"
#include "request_handler.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace logging = boost::log;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value);

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

void InitLogger() {
    logging::add_console(
      std::cout,
      logging::keywords::format = &http_handler::LoggingRequestHandler::Formatter,
      logging::keywords::auto_flush = true
    );

    logging::add_common_attributes();
}

}  // namespace

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: game_server <game-config-json> <static-files-dir>"sv << std::endl;
        return EXIT_FAILURE;
    }
    try {
        InitLogger();

        // 1. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(argv[1]);
        const std::filesystem::path static_root(argv[2]);

        if (!std::filesystem::exists(static_root) || !std::filesystem::is_directory(static_root)) {
            throw std::runtime_error("Static directory not found or is not a directory");
        }

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code& ec, int signal_number) {
            if (!ec)
                ioc.stop();
        });

        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        http_handler::RequestHandler handler{game, static_root};

        // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&log_handler](auto&& req, auto&& send, const net::ip::address& address) {
            log_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send), address);
        });
        std::cout << "Server has started..."sv << std::endl;

        // Сообщение, что сервер запущен и готов обрабатывать запросы
        boost::json::value starting_data{ {"port"s, port}, {"address"s, address.to_string()} };
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, starting_data)
            << "server started"sv;

        // 6. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });

        boost::json::value exiting_data{ {"code"s, 0} };
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, exiting_data)
            << "server exited"sv;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}