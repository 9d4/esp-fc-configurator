cmake_minimum_required(VERSION 3.21)

project(esp-fc-configurator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets SerialPort Network OpenGLWidgets)

qt_add_executable(esp-fc-configurator
    src/main.cpp
    src/model/AppState.h
    src/model/FcStatus.h
    src/model/FcConfigStore.cpp
    src/model/FcConfigStore.h
    src/model/FirmwareInfo.h
    src/model/RxConfig.h
    src/model/SensorData.h
    src/protocol/CliSession.cpp
    src/protocol/CliSession.h
    src/protocol/MspClient.cpp
    src/protocol/MspClient.h
    src/protocol/MspCodec.cpp
    src/protocol/MspCodec.h
    src/transport/ITransport.h
    src/transport/SerialTransport.cpp
    src/transport/SerialTransport.h
    src/transport/TcpTransport.cpp
    src/transport/TcpTransport.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/AttitudeView.cpp
    src/ui/AttitudeView.h
)

target_link_libraries(esp-fc-configurator PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::Network
    Qt6::OpenGLWidgets
)

target_include_directories(esp-fc-configurator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(esp-fc-configurator PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

install(TARGETS esp-fc-configurator RUNTIME DESTINATION bin)
