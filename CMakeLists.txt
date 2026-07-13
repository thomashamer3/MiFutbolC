cmake_minimum_required(VERSION 3.20)

project(MiFutbolC LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(MIFUTBOLC_BUILD_TESTS "Build tests" OFF)

# ---- Prevent in-source builds ----
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not allowed.\n"
        "Use:\n"
        "cmake --preset mingw-debug"
    )
endif()

# ---- Sources ----
file(GLOB_RECURSE ALL_C_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/*.c"
    "${CMAKE_SOURCE_DIR}/sqlite/*.c"
)

set(CORE_SOURCES "")
set(MAIN_SOURCE "")
set(TEST_SOURCES "")

foreach(_f ${ALL_C_SOURCES})
    get_filename_component(_name "${_f}" NAME)

    if((_f MATCHES "/tests/" OR _name MATCHES "^test_.*") AND NOT _name STREQUAL "unity.c")
        list(APPEND TEST_SOURCES "${_f}")
    elseif(_name STREQUAL "main.c")
        set(MAIN_SOURCE "${_f}")
    else()
        list(APPEND CORE_SOURCES "${_f}")
    endif()
endforeach()

if(NOT MAIN_SOURCE)
    message(FATAL_ERROR "/main.c not found")
endif()

# ---- Core library ----
add_library(mifutbolc_core STATIC ${CORE_SOURCES})

target_include_directories(mifutbolc_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/sqlite
)

target_compile_definitions(mifutbolc_core PRIVATE
    SQLITE_THREADSAFE=0
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_OMIT_JSON1
    SQLITE_OMIT_PROGRESS_CALLBACK
    SQLITE_OMIT_DEPRECATED
    SQLITE_DEFAULT_MEMSTATUS=0
)

# ---- Warnings ----
if(MSVC)
    target_compile_options(mifutbolc_core PRIVATE /W4)
else()
    target_compile_options(mifutbolc_core PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# ---- Debug / Release ----
if(NOT MSVC)

    target_compile_options(mifutbolc_core PRIVATE

        # DEBUG
        $<$<CONFIG:Debug>:
            -O0
            -g3
            -fstack-protector
            -fsanitize=address
            -fsanitize=undefined
        >

        # RELEASE
        $<$<CONFIG:Release>:
            -O2
            -DNDEBUG
            -fomit-frame-pointer
        >
    )
endif()

# ---- Threads ----
if(NOT WIN32)
    find_package(Threads REQUIRED)
endif()

# ---- Main executable ----
add_executable(MiFutbolC ${MAIN_SOURCE})

target_link_libraries(MiFutbolC PRIVATE
    mifutbolc_core
)

if(WIN32)
    target_link_libraries(MiFutbolC PRIVATE
        comdlg32
        shell32
        bcrypt
    )
else()
    target_link_libraries(MiFutbolC PRIVATE
        Threads::Threads
    )
endif()

# ---- Link optimization ----
if(NOT MSVC)
    target_link_options(MiFutbolC PRIVATE

        # Debug sanitizers
        $<$<CONFIG:Debug>:
            -fsanitize=address
            -fsanitize=undefined
        >

        # Release size/speed
        $<$<CONFIG:Release>:
            -Wl,--gc-sections
            -s
        >
    )
endif()

# ---- Tests ----
if(MIFUTBOLC_BUILD_TESTS AND TEST_SOURCES)

    enable_testing()

    # Rebuild mifutbolc_core with UNIT_TEST for test-only code paths
    target_compile_definitions(mifutbolc_core PRIVATE UNIT_TEST)

    # Build each test file as a separate executable
    foreach(_test_file ${TEST_SOURCES})
        get_filename_component(_test_name ${_test_file} NAME_WE)
        add_executable(${_test_name}
            ${_test_file}
            ${CMAKE_SOURCE_DIR}/tests/unity/unity.c
        )
        target_compile_definitions(${_test_name} PRIVATE UNIT_TEST)
        target_link_libraries(${_test_name} PRIVATE
            mifutbolc_core
            bcrypt
        )
        add_test(NAME ${_test_name}
            COMMAND ${_test_name}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    endforeach()
endif()

# ---- Copy langs to output directory at configure time ----
file(COPY "${CMAKE_SOURCE_DIR}/langs" DESTINATION "${CMAKE_BINARY_DIR}")

# ---- Install ----
include(GNUInstallDirs)

install(TARGETS MiFutbolC
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/langs/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/langs"
)
