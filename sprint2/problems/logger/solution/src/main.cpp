#include "sdk.h"
#include <boost/program_options.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <optional>
#include <thread>

#include "json_loader.h"

#include "api_handler.h"
#include "request_handler.h"
#include "logging_handler.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace logging = boost::log;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value);

namespace {

struct Args {
    std::string config_path;
    std::string static_path;
    int tick_time;
    bool randomize_spawn = false;
    bool no_auto_tick = true;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    po::options_description desc{ "All options"s };

    Args args;
    desc.add_options()
        // Добавляем опцию --help и её короткую версию -h
        ("help,h", "produce help message")
        ("tick-period,t", po::value(&args.tick_time)->value_name("milliseconds"s), "set tick period")
        ("config-file,c", po::value(&args.config_path)->value_name("file"s), "set config file path")
        ("www-root,w", po::value(&args.static_path)->value_name("path"s), "set static files root")
        ("randomize-spawn-points", "spawn dogs at random positions");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        // Если был указан параметр --help, то выводим справку и возвращаем nullopt
        std::cout << desc;
        return std::nullopt;
    }

    // Проверяем наличие опций
    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("No path to config"s);
    }
    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("No path to static"s);
    }
    if (vm.contains("tick-period"s)) {
        args.no_auto_tick = false;
    }
    if (vm.contains("randomize-spawn-points")) {
        args.randomize_spawn = true;
    }

    // С опциями программы всё в порядке, возвращаем структуру args
    return args;
}

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    // Функция handler будет вызываться внутри strand с интервалом period
    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{ strand }
        , period_{ period }
        , handler_{ std::move(handler) } {
    }

    void Start() {
        net::dispatch(strand_, [self = shared_from_this(), last_tick = &last_tick_] {
            *last_tick = Clock::now();
            self->ScheduleTick();
            });
    }

private:
    void ScheduleTick() {
        assert(strand_.running_in_this_thread());
        timer_.expires_after(period_);
        timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnTick(ec);
            });
    }

    void OnTick(sys::error_code ec) {
        using namespace std::chrono;
        assert(strand_.running_in_this_thread());

        if (!ec) {
            auto this_tick = Clock::now();
            auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
            last_tick_ = this_tick;
            try {
                handler_(delta);
            }
            catch (...) {
            }
            ScheduleTick();
        }
    }

    using Clock = std::chrono::steady_clock;

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_{ strand_ };
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
};

// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

void InitLogger() {
    logging::add_console_log(
        std::cout,
        //tru additional data & add message data
        logging::keywords::format = &http_handler::LoggingRequestHandler::Formatter, //R"({"timestamp":"%TimeStamp%", "data":"%AdditionalData%", "message":"%Message%"})",
        logging::keywords::auto_flush = true
    );

    logging::add_common_attributes();
}

}  // namespace

int main(int argc, const char* argv[]) {
    try {
        auto args = ParseCommandLine(argc, argv);
        if (!args) {
            return EXIT_SUCCESS;
        }

        InitLogger();

        // 1. Загружаем карту из файла и построить модель игры
        app::Application app{std::move(json_loader::LoadGame(args->config_path)), args->randomize_spawn};

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                std::cout << "Signal "sv << signal_number << " received"sv << std::endl;
                ioc.stop();
            }
            });

        // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
        auto handler = std::make_shared<http_handler::RequestHandler>(app, args->static_path.data(), ioc, args->no_auto_tick);
        http_handler::LoggingRequestHandler log_handler{handler};

        if (!args->no_auto_tick) {
            if (args->tick_time < 1) {
                throw std::runtime_error("Wrong tick time");
            }
            auto ticker = std::make_shared<Ticker>(handler->GetStrand(), std::chrono::milliseconds(args->tick_time),
                [&app](std::chrono::milliseconds delta) { app.Tick(delta.count()); }
            );
            ticker->Start();
        }

        // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&log_handler](auto&& req, auto&& send, const net::ip::address& address) {
            log_handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send), address);
        });

        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
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