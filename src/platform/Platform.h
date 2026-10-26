#pragma once

#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// Пути и мелочи, которые на Linux и Windows выглядят по-разному.
// QML сам не умеет ни спросить файловую систему, ни правильно разобрать
// file:///C:/… — делаем это здесь
class Platform : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // linux, windows, macos
    Q_PROPERTY(QString os READ os CONSTANT)

public:
    explicit Platform(QObject *parent = nullptr);

    static QString os();

    // Путь для показа: домашняя папка — «~», на Windows — с обратными косыми
    Q_INVOKABLE static QString prettyPath(const QString &path);
    // file://… из перетаскивания или окна выбора — в обычный путь; не файл — пусто
    Q_INVOKABLE static QString localPath(const QUrl &url);
    Q_INVOKABLE static QUrl fileUrl(const QString &path);
    Q_INVOKABLE static bool isDir(const QString &path);
};
