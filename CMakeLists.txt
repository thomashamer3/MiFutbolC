cmake_minimum_required(VERSION 3.15)
project(MiFutbolC C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD_REQUIRED ON)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(WARNING "In-source build detected. It's recommended to use an out-of-source build.")
endif()

# Collect all C sources under the project root (reconfigure on file changes)
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/*.c"
)

add_executable(MiFutbolC ${SOURCE_FILES})

target_include_directories(MiFutbolC PRIVATE ${CMAKE_SOURCE_DIR})

if(MSVC)
  target_compile_options(MiFutbolC PRIVATE /W3)
else()
  target_compile_options(MiFutbolC PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
  target_link_libraries(MiFutbolC PRIVATE comdlg32)
else()
  find_package(Threads REQUIRED)
  target_link_libraries(MiFutbolC PRIVATE Threads::Threads)
endif()

install(TARGETS MiFutbolC RUNTIME DESTINATION bin)
