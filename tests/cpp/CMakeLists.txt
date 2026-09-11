include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(
    backtester_tests
    test_version.cpp
    test_bar.cpp
)
target_link_libraries(backtester_tests PRIVATE backtester::core Catch2::Catch2WithMain)
target_compile_features(backtester_tests PRIVATE cxx_std_23)

include(Catch)
catch_discover_tests(backtester_tests)

