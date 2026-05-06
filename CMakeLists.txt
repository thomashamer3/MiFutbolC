cmake_minimum_required(VERSION 3.15)
project(MiFutbolC C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(MIFUTBOLC_BUILD_TESTS "Build MiFutbolC_tests target" OFF)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(WARNING "In-source build detected. It's recommended to use an out-of-source build.")
endif()

# Collect all C sources under the project root (reconfigure on file changes)
file(GLOB_RECURSE ALL_C_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/*.c"
)

# Partition sources into: core (no main), main, and tests. This prevents
# accidental linking of multiple `main` definitions into the same target.
set(CORE_SOURCES "")
set(MAIN_SOURCE "")
set(TEST_SOURCES "")

foreach(_f ${ALL_C_SOURCES})
  # Normalize path for matching
  file(TO_CMAKE_PATH "${_f}" _f_norm)
  get_filename_component(_name "${_f_norm}" NAME)

  if(_f_norm MATCHES "(/|\\\\)tests(/|\\\\|$)" OR _name MATCHES "^test_.*")
    list(APPEND TEST_SOURCES "${_f_norm}")
  elseif(_name STREQUAL "main.c")
    set(MAIN_SOURCE "${_f_norm}")
  else()
    list(APPEND CORE_SOURCES "${_f_norm}")
  endif()
endforeach()

# Core static library (everything except main and tests)
add_library(mifutbolc_core STATIC ${CORE_SOURCES})
target_include_directories(mifutbolc_core PUBLIC ${CMAKE_SOURCE_DIR})
target_compile_definitions(mifutbolc_core PRIVATE SQLITE_THREADSAFE=0)

if(MSVC)
  target_compile_options(mifutbolc_core PRIVATE /W3)
else()
  target_compile_options(mifutbolc_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Ensure threads are available on non-Windows platforms before linking
if(NOT WIN32)
  find_package(Threads REQUIRED)
endif()

# Main executable: link the core library plus the single main.c
if(MAIN_SOURCE)
  add_executable(MiFutbolC ${MAIN_SOURCE})
  target_link_libraries(MiFutbolC PRIVATE mifutbolc_core)
  target_include_directories(MiFutbolC PRIVATE ${CMAKE_SOURCE_DIR})
  if(MSVC)
    target_compile_options(MiFutbolC PRIVATE /W3)
  else()
    target_compile_options(MiFutbolC PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  if(WIN32)
    target_link_libraries(MiFutbolC PRIVATE comdlg32 shell32 bcrypt)
  else()
    target_link_libraries(MiFutbolC PRIVATE Threads::Threads)
  endif()
else()
  message(FATAL_ERROR "No main.c found to build MiFutbolC executable.")
endif()

# Optional tests executable (built only if test sources are present)
if(TEST_SOURCES AND MIFUTBOLC_BUILD_TESTS)
  add_executable(MiFutbolC_tests ${TEST_SOURCES})
  target_link_libraries(MiFutbolC_tests PRIVATE mifutbolc_core)
  target_include_directories(MiFutbolC_tests PRIVATE ${CMAKE_SOURCE_DIR})
  if(MSVC)
    target_compile_options(MiFutbolC_tests PRIVATE /W3)
  else()
    target_compile_options(MiFutbolC_tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  if(WIN32)
    target_link_libraries(MiFutbolC_tests PRIVATE comdlg32 shell32 bcrypt)
  else()
    target_link_libraries(MiFutbolC_tests PRIVATE Threads::Threads)
  endif()
endif()

install(TARGETS MiFutbolC RUNTIME DESTINATION bin)
