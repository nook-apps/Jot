#include "Backdrop.h"

#include <QWindow>

#if defined(Q_OS_WIN)
#include <QOperatingSystemVersion>
#include <dwmapi.h>
#elif defined(JOT_HAVE_KWINDOWSYSTEM)
#include <KWindowEffects>
#endif

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#endif

namespace {
#if defined(Q_OS_WIN)
// Из dwmapi.h свежих SDK; объявляем сами, чтобы собираться и со старым
constexpr DWORD kUseDarkMode = 20;      // DWMWA_USE_IMMERSIVE_DARK_MODE
constexpr DWORD kBackdropType = 38;     // DWMWA_SYSTEMBACKDROP_TYPE
constexpr int kBackdropNone = 1;        // DWMSBT_NONE
constexpr int kBackdropAcrylic = 3;     // DWMSBT_TRANSIENTWINDOW
#endif

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
bool hyprlandBlur()
{
    if (qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE"))
        return false;
    QProcess p;
    p.start(QStringLiteral("hyprctl"), {QStringLiteral("-j"), QStringLiteral("getoption"),
                                       QStringLiteral("decoration:blur:enabled")});
    return p.waitForFinished(1000)
        && QJsonDocument::fromJson(p.readAllStandardOutput()).object().value(QStringLiteral("int")).toInt() == 1;
}
#endif

bool detect()
{
#if defined(Q_OS_WIN)
    // Фон окна через DWMWA_SYSTEMBACKDROP_TYPE — с Windows 11 22H2 (сборка 22621)
    const QOperatingSystemVersion os = QOperatingSystemVersion::current();
    return os.majorVersion() >= 10 && os.microVersion() >= 22621;
#elif defined(Q_OS_MACOS)
    return false;
#else
    if (hyprlandBlur())
        return true;
#ifdef JOT_HAVE_KWINDOWSYSTEM
    return KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind);
#else
    return false;
#endif
#endif
}
}

Backdrop::Backdrop(QObject *parent)
    : QObject(parent)
{
}

bool Backdrop::available()
{
    static const bool result = detect();
    return result;
}

void Backdrop::apply(QWindow *window, bool on, bool dark)
{
    if (!window)
        return;
#if defined(Q_OS_WIN)
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    // Заголовок окна — в тон теме, со стеклом и без
    const BOOL darkMode = dark;
    DwmSetWindowAttribute(hwnd, kUseDarkMode, &darkMode, sizeof darkMode);
    if (!available())
        return;
    // Фон DWM виден только там, где рамка «заходит» в окно: растягиваем её на всё окно
    const MARGINS margins = on ? MARGINS{-1, -1, -1, -1} : MARGINS{0, 0, 0, 0};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    const int type = on ? kBackdropAcrylic : kBackdropNone;
    DwmSetWindowAttribute(hwnd, kBackdropType, &type, sizeof type);
#elif defined(JOT_HAVE_KWINDOWSYSTEM)
    Q_UNUSED(dark)
    // В Hyprland не нужно: он размывает все полупрозрачные окна сам
    if (available())
        KWindowEffects::enableBlurBehind(window, on);
#else
    Q_UNUSED(on)
    Q_UNUSED(dark)
#endif
}
