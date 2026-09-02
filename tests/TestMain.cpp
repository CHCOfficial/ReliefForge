#include "TestHarness.hpp"
#ifdef RELIEFFORGE_TEST_QT
#include <QGuiApplication>
#endif

int main(int argc, char** argv) {
#ifdef RELIEFFORGE_TEST_QT
    QGuiApplication application(argc, argv);
#else
    (void)argc;
    (void)argv;
#endif
    std::size_t failures{};
    for (const auto& test : rf::test::registry()) {
        try {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << "\n       " << error.what() << '\n';
        }
    }
    std::cout << "\n" << (rf::test::registry().size() - failures) << " passed, "
              << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
