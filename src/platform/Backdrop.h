#pragma once

#include <QObject>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

// Размытие того, что под окном, — основа стекла Glaze. Само окно полупрозрачное,
// размывает композитор; Backdrop только просит его об этом там, где умеют:
//   Hyprland — размывает сам, если blur включён, просить не нужно
//   KDE, GNOME 51+, niri и другие с ext-background-effect-v1, KWin на X11 —
//     через KF6 WindowSystem (необязательная зависимость, JOT_WITH_KWINDOWSYSTEM)
//   Windows 11 22H2+ — системный фон Acrylic через DWM
// Где размытия нет (Windows 10, sway…), available ложно — Theme рисует окно
// непрозрачным, без стекла
class Backdrop : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ isAvailable CONSTANT)

public:
    explicit Backdrop(QObject *parent = nullptr);

    bool isAvailable() const { return available(); }
    // Спрашивает окружение один раз; нужен и ThemeSource — стекло по умолчанию
    static bool available();

    // on — стекло включено; dark — тёмная тема (на Windows — цвет заголовка окна)
    Q_INVOKABLE void apply(QWindow *window, bool on, bool dark);
};
