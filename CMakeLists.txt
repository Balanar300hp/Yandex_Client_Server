cmake_minimum_required(VERSION 2.8.7)

project(Yandex_Client_Server)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

option(BUILD_SHARED_LIBS "Build shared instead of static library" OFF)
option(BUILD_TESTS "Build tests for webdavclient" ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/ tests/)
add_library(Yandex_Client_Server STATIC ${CMAKE_CURRENT_SOURCE_DIR}/tests/main.cpp)

set(SOURCES include/client.hpp)
find_package(OpenSSL REQUIRED)
find_package(Boost COMPONENTS filesystem REQUIRED)
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIR})
if(UNIX)
    set(DEPENDS_LIBRARIES ${OPENSSL_LIBRARIES} ${CURL_LIBRARY})
elseif(MSVC)
    set(DEPENDS_LIBRARIES ${OPENSSL_LIBRARIES} ${CURL_LIBRARY} ws2_32)
endif()

add_executable(Examples ${CMAKE_CURRENT_SOURCE_DIR} tests/init.cpp)
target_link_libraries(Examples ${PROJECT_NAME} ${DEPENDS_LIBRARIES} ${Boost_LIBRARIES})

enable_testing(true)
add_subdirectory(tests)
