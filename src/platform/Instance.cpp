#include "Instance.h"

#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QStandardPaths>

namespace {
Instance *s_instance = nullptr;

// Сообщение — JSON-массив путей и перевод строки; ответ — "ok\n"
constexpr int kTimeoutMs = 2000;
}

Instance::Instance(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server.nextPendingConnection()) {
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] { readClient(socket); });
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
}

Instance *Instance::create(QQmlEngine *, QJSEngine *)
{
    // Владелец — main, движок не должен его удалять
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QString Instance::serverName()
{
    // Имя — как app_id: так же зовётся окно и пакет. На Linux — полный путь
    // в каталоге сеанса пользователя (у каждого свой, и Flatpak его не путает),
    // на Windows — имя канала, к нему дописываем пользователя
    const QString id = QStringLiteral("io.github.nook_apps.Jot");
#ifdef Q_OS_WIN
    return id + QLatin1Char('-') + qEnvironmentVariable("USERNAME");
#else
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString path = runtime + QLatin1Char('/') + id + QStringLiteral(".sock");
    // Путь сокета в Linux — не длиннее 107 байт (sun_path). Длинный XDG_RUNTIME_DIR
    // (Flatpak, тесты) — тогда короткое имя: Qt положит сокет во временную папку
    if (runtime.isEmpty() || path.toLocal8Bit().size() > 100)
        return id + QLatin1Char('-') + qEnvironmentVariable("USER");
    return path;
#endif
}

bool Instance::forward(const QStringList &paths)
{
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(500))
        return false;
    socket.write(QJsonDocument(QJsonArray::fromStringList(paths)).toJson(QJsonDocument::Compact) + '\n');
    if (!socket.waitForBytesWritten(kTimeoutMs))
        return false;
    // Окно зависло или закрывается — тогда откроемся сами
    while (!socket.canReadLine())
        if (!socket.waitForReadyRead(kTimeoutMs))
            return false;
    return socket.readLine().trimmed() == "ok";
}

bool Instance::listen()
{
    // Сокет виден только этому пользователю
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (m_server.listen(serverName()))
        return true;
    // Сокет занят. Либо его оставил упавший Jot, либо два запуска пришли разом
    // и второй успел первым — тогда ему и передать бы файлы, но main уже решил
    // открываться сам. Чужой живой сокет не трогаем: удаляем только мёртвый
    QLocalSocket probe;
    probe.connectToServer(serverName());
    if (probe.waitForConnected(300))
        return false;
    QLocalServer::removeServer(serverName());
    return m_server.listen(serverName());
}

void Instance::readClient(QLocalSocket *socket)
{
    if (!socket->canReadLine())
        return;
    const QJsonArray list = QJsonDocument::fromJson(socket->readLine()).array();
    socket->write("ok\n");
    socket->flush();
    socket->disconnectFromServer();
    QStringList paths;
    for (const QJsonValue &v : list)
        paths << v.toString();
    emit openRequested(paths);
}
