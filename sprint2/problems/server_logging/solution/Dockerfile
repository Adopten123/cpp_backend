FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan

#Запуск conan
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. -s compiler.libcxx=libstdc++11 -s build_type=Debug --build=missing

COPY ./src /app/src
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . \

# Второй контейнер в том же докерфайле
FROM ubuntu:22.04 as run

# Создадёт пользователя www
RUN groupadd -r www && useradd -r -g www www
USER www

# Копирует приложение со сборочного контейнера в директорию /app.
COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

# Запускает игровой сервер
ENTRYPOINT ["/app/game_server", "/app/data/config.json", "/app/static"]