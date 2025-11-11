cmake_minimum_required(VERSION 3.15)
project(GitHubUploader VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Disable compiler-specific extensions

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/dist)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Add source files
set(SOURCES
    src/main.cpp
    src/GitHubUploader.cpp
)

# Option to allow GitHub download fallback
option(ALLOW_GITHUB_DOWNLOAD "Allow downloading nlohmann_json from GitHub if not found" OFF)

# --- Try to find nlohmann_json from system first ---
message(STATUS "Checking for nlohmann_json library...")

find_package(nlohmann_json 3.11.0 QUIET)

if(nlohmann_json_FOUND)
    message(STATUS "✓ Found nlohmann_json installed on system (version ${nlohmann_json_VERSION})")
else()
    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "ERROR: nlohmann_json is not installed!")
    message(STATUS "========================================")
    message(STATUS "")
    message(STATUS "Please choose an installation method:")
    message(STATUS "")
    message(STATUS "Option 1: Install via package manager (RECOMMENDED)")
    message(STATUS "  Run this command:")
    message(STATUS "    bash ${PROJECT_SOURCE_DIR}/bash_scripts/install_dependencies.sh")
    message(STATUS "  Or use make:")
    message(STATUS "    make install_deps")
    message(STATUS "  Then run cmake again.")
    message(STATUS "")
    message(STATUS "Option 2: Download from GitHub (fallback)")
    message(STATUS "  Run cmake with the flag:")
    message(STATUS "    cmake -DALLOW_GITHUB_DOWNLOAD=ON ..")
    message(STATUS "")
    message(STATUS "========================================")
    
    if(NOT ALLOW_GITHUB_DOWNLOAD)
        message(FATAL_ERROR "nlohmann_json not found. Please install it using one of the options above.")
    else()
        message(STATUS "")
        message(STATUS "ALLOW_GITHUB_DOWNLOAD is enabled. Downloading from GitHub...")
        
        # Download from GitHub
        include(FetchContent)
        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
        )
        
        message(STATUS "Downloading nlohmann_json from GitHub... Please wait...")
        
        FetchContent_GetProperties(nlohmann_json)
        if(NOT nlohmann_json_POPULATED)
            FetchContent_Populate(nlohmann_json)
            if(nlohmann_json_POPULATED)
                add_subdirectory(${nlohmann_json_SOURCE_DIR} ${nlohmann_json_BINARY_DIR})
                message(STATUS "✓ nlohmann_json download completed!")
            else()
                message(WARNING "")
                message(WARNING "========================================")
                message(WARNING "Failed to download nlohmann_json from GitHub.")
                message(WARNING "")
                message(WARNING "Please install manually:")
                message(WARNING "  bash ${PROJECT_SOURCE_DIR}/bash_scripts/install_dependencies.sh")
                message(WARNING "========================================")
                message(FATAL_ERROR "nlohmann_json library is required to build this project.")
            endif()
        endif()
    endif()
endif()

# --- Find required packages ---
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

# Add executable
add_executable(GitHubUploader ${SOURCES})

# Link libraries
target_link_libraries(GitHubUploader 
    PRIVATE 
        CURL::libcurl 
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
)

# Enable threading support (required for std::thread)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(GitHubUploader PRIVATE Threads::Threads)

# Create data directory if it doesn't exist and copy files
add_custom_command(TARGET GitHubUploader POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:GitHubUploader>/data
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:GitHubUploader>/data
    COMMENT "Copying data directory to build output"
)

# Optional: Add compiler warnings for better code quality
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(GitHubUploader PRIVATE 
        -Wall -Wextra -Wpedantic
    )
elseif(MSVC)
    target_compile_options(GitHubUploader PRIVATE 
        /W4
    )
endif()

# Custom target to install dependencies using the bash script
add_custom_target(install_deps
    COMMAND bash ${PROJECT_SOURCE_DIR}/bash_scripts/install_dependencies.sh
    COMMENT "Installing project dependencies via package manager"
    USES_TERMINAL
)

# Add custom target to run the application after building
add_custom_target(run
    COMMAND ${CMAKE_COMMAND} -E echo "Running GitHubUploader..."
    COMMAND $<TARGET_FILE:GitHubUploader>
    DEPENDS GitHubUploader
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Building and running GitHubUploader"
    USES_TERMINAL
)
