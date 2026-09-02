#include "app/AppController.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>

int main(int argc, char* argv[]) {
    QGuiApplication::setOrganizationName(QStringLiteral("ReliefForge"));
    QGuiApplication::setApplicationName(QStringLiteral("ReliefForge"));
    QGuiApplication::setApplicationVersion(QStringLiteral(RELIEFFORGE_VERSION));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    QGuiApplication application(argc, argv);

    rf::app::AppController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("ReliefForge"), QStringLiteral("Main"));

    // Defer until QML is ready so the first image submission also reaches the
    // support toast. Explicit image/project arguments override the demo.
    const auto arguments = application.arguments();
    QTimer::singleShot(0, &controller, [&controller, arguments] { controller.openStartup(arguments); });

    return application.exec();
}
