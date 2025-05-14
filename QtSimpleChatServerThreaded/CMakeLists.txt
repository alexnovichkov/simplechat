project(chatserver LANGUAGES CXX)
find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} 5.7 COMPONENTS Core Network REQUIRED)
add_executable(chatserver
    chatserver.cpp
    servermain.cpp
    serverworker.cpp
    server.cpp
    chatserver.h
    serverworker.h
    server.h
)
target_link_libraries(chatserver PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network)
target_include_directories(chatserver PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_compile_definitions(chatserver PRIVATE QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
set_target_properties(chatserver PROPERTIES
	AUTOMOC ON
	AUTOUIC ON
	CXX_STANDARD 11
	CXX_STANDARD_REQUIRED ON
	VERSION "1.0.0"
)

