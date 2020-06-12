# Minimal CMakeLists.txt for the AWS SDK for C++.
cmake_minimum_required(VERSION 3.2)

# Use shared libraries, which is the default for the AWS C++ SDK build.
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

project(nphw3)

# Locate the AWS SDK for C++ package.
find_package(AWSSDK REQUIRED COMPONENTS s3)

# The executable name and its sourcefiles.
add_executable(server server.cpp)

# Build using the C++ standard version 11.
target_compile_features(server PUBLIC cxx_std_11)

# The libraries used by your executable.
target_link_libraries(server ${AWSSDK_LINK_LIBRARIES})
target_link_libraries(server -lsqlite3)
