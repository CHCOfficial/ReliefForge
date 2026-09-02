#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace rf::test {

using TestFunction = std::function<void()>;

struct TestCase {
    std::string name;
    TestFunction function;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

struct Registration {
    Registration(std::string name, TestFunction function) {
        registry().push_back({std::move(name), std::move(function)});
    }
};

inline void require(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        throw std::runtime_error(
            std::string(file) + ':' + std::to_string(line) + " requirement failed: " + expression);
    }
}

inline void requireNear(
    double actual,
    double expected,
    double tolerance,
    const char* expression,
    const char* file,
    int line) {
    if (std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(
            std::string(file) + ':' + std::to_string(line) + " " + expression +
            " expected " + std::to_string(expected) + " but got " + std::to_string(actual));
    }
}

} // namespace rf::test

#define RF_JOIN_INNER(a, b) a##b
#define RF_JOIN(a, b) RF_JOIN_INNER(a, b)
#define RF_TEST(name) \
    static void RF_JOIN(rf_test_, __LINE__)(); \
    static ::rf::test::Registration RF_JOIN(rf_registration_, __LINE__)(name, RF_JOIN(rf_test_, __LINE__)); \
    static void RF_JOIN(rf_test_, __LINE__)()
#define RF_REQUIRE(expression) ::rf::test::require((expression), #expression, __FILE__, __LINE__)
#define RF_REQUIRE_NEAR(actual, expected, tolerance) \
    ::rf::test::requireNear((actual), (expected), (tolerance), #actual, __FILE__, __LINE__)

