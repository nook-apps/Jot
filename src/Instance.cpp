#include "Instance.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QJSEngine>

namespace {
const QString kService = QStringLiteral("org.stasyao.jot");
const QString kPath = QStringLiteral("/org/stasyao/jot");
const QString kInterface = QStringLiteral("org.stasyao.jot.Instance");

Instance *s_instance = nullptr;
}

Instance::Instance(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
}

Instance *Instance::create(QQmlEngine *, QJSEngine *)
{
    // Владелец — main, движок не должен его удалять
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool Instance::forward(const QStringList &paths)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface()->isServiceRegistered(kService))
        return false;
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kInterface, QStringLiteral("Open"));
    msg << paths;
    // Окно зависло или закрывается — тогда откроемся сами
    const QDBusMessage reply = bus.call(msg, QDBus::Block, 2000);
    return reply.type() == QDBusMessage::ReplyMessage;
}

bool Instance::registerOnBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    return bus.registerObject(kPath, this, QDBusConnection::ExportScriptableSlots)
        && bus.registerService(kService);
}

void Instance::Open(const QStringList &paths)
{
    emit openRequested(paths);
}
