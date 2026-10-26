#pragma once

#include <QLocalServer>
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QLocalSocket;
class QQmlEngine;

// Одно окно Jot на сеанс. Первый запуск слушает локальный сокет (на Linux —
// в $XDG_RUNTIME_DIR, на Windows — именованный канал); следующие `jot файл.md`
// (двойной щелчок в проводнике, xdg-open) передают файлы ему и выходят —
// файлы открываются вкладками в уже открытом окне.
// QLocalServer есть везде, где есть Qt: D-Bus для этого не нужен
class Instance : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    // Без значения по умолчанию намеренно: у типа с конструктором по умолчанию
    // QML создаёт свой экземпляр и create() не зовёт
    explicit Instance(QObject *parent);

    // Объект создаётся в main до движка (сокет нужно занять до окна),
    // QML получает его же, а не новый
    static Instance *create(QQmlEngine *, QJSEngine *);

    // true — окно уже есть и файлы переданы ему: этому процессу пора выйти
    static bool forward(const QStringList &paths);
    // Занять сокет; не вышло — Jot всё равно работает, просто без «одного окна»
    bool listen();

signals:
    void openRequested(const QStringList &paths);

private:
    static QString serverName();
    void readClient(QLocalSocket *socket);

    QLocalServer m_server;
};
