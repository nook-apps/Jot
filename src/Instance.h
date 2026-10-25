#pragma once

#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

// Одно окно Jot на сеанс. Первый запуск занимает имя org.stasyao.jot на шине;
// следующие `jot файл.md` (двойной щелчок в Shelf, xdg-open) передают файлы ему
// и выходят — файлы открываются вкладками в уже открытом окне
class Instance : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_CLASSINFO("D-Bus Interface", "org.stasyao.jot.Instance")

public:
    // Без значения по умолчанию намеренно: у типа с конструктором по умолчанию
    // QML создаёт свой экземпляр и create() не зовёт
    explicit Instance(QObject *parent);

    // Объект создаётся в main до движка (имя на шине нужно занять до окна),
    // QML получает его же, а не новый
    static Instance *create(QQmlEngine *, QJSEngine *engine);

    // true — окно уже есть и файлы переданы ему: этому процессу пора выйти
    static bool forward(const QStringList &paths);
    // Занять имя на шине; не вышло — Jot всё равно работает, просто без «одного окна»
    bool registerOnBus();

public Q_SLOTS:
    Q_SCRIPTABLE void Open(const QStringList &paths);

signals:
    void openRequested(const QStringList &paths);
};
