cmake_minimum_required(VERSION 3.28)
project(SPH_Fluid_Solver LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(structures src/structures.cpp)

add_library(parser src/parser.cpp)

add_library(kernel src/kernel.cpp)
target_compile_features(kernel PRIVATE cxx_std_17)
target_link_libraries(kernel PRIVATE structures)

add_library(simulation src/simulate.cpp)
target_compile_features(simulation PRIVATE cxx_std_17)
target_link_libraries(simulation PRIVATE parser structures)

add_library(render src/render.cpp)
target_compile_features(render PRIVATE cxx_std_17)
target_link_libraries(render PRIVATE SFML::Graphics simulation)

add_executable(main src/main.cpp)
target_compile_features(main PRIVATE cxx_std_17)
target_link_libraries(main PRIVATE SFML::Graphics simulation render kernel)

add_executable(animate src/animation.cpp)
target_compile_features(animate PRIVATE cxx_std_17)
target_link_libraries(animate PRIVATE SFML::Graphics simulation kernel render)

add_executable(replay src/replay.cpp)
target_compile_features(replay PRIVATE cxx_std_17)
target_link_libraries(replay PRIVATE SFML::Graphics simulation kernel render)

add_executable(plain src/plain.cpp)
target_compile_features(plain PRIVATE cxx_std_17)
target_link_libraries(plain PRIVATE simulation kernel)

add_executable(reflectionTest data/energyTest.cpp)
target_compile_features(reflectionTest PRIVATE cxx_std_17)
target_link_libraries(reflectionTest PRIVATE simulation kernel)

add_executable(debug test/debug.cpp)
target_compile_features(debug PRIVATE cxx_std_17)
target_link_libraries(debug PRIVATE simulation render kernel)

enable_testing()

add_executable(
  kernel_test
  test/kernel_test.cpp
)
target_link_libraries(kernel_test GTest::gtest_main structures simulation kernel)

include(GoogleTest)
gtest_discover_tests(kernel_test)

