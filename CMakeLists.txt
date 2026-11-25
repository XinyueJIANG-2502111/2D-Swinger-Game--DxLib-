cmake_minimum_required(VERSION 3.16)
project(NeonSwingerGame LANGUAGES CXX)

if(NOT MSVC)
    message(FATAL_ERROR "The Visual C++ edition of DxLib requires MSVC.")
endif()

set(DXLIB_DIR "C:/DxLib_VC/プロジェクトに追加すべきファイル_VC用"
    CACHE PATH "Directory containing DxLib.h and the Visual C++ libraries")
if(NOT EXISTS "${DXLIB_DIR}/DxLib.h")
    message(FATAL_ERROR
        "DxLib.h was not found in '${DXLIB_DIR}'. "
        "Set -DDXLIB_DIR:PATH to the directory containing DxLib.h and the DxLib libraries.")
endif()
message(STATUS "DxLib directory: ${DXLIB_DIR}")

# Track source files added to or removed from src.
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.hpp"
    "src/*.h"
)
add_executable(${PROJECT_NAME} WIN32 ${GAME_SOURCES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_include_directories(${PROJECT_NAME} PRIVATE "${DXLIB_DIR}")

# DxLib headers select libraries for the active architecture and MSVC runtime.
target_link_directories(${PROJECT_NAME} PRIVATE "${DXLIB_DIR}")

# Copy optional game assets beside the executable.
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
        VERBATIM
    )
endif()
