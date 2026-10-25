import QtQuick
import QtQuick.Layouts
import Jot

Window {
    id: win

    property string startFile: ""

    readonly property Document doc: editor.document

    width: 1000
    height: 720
    minimumWidth: 480
    minimumHeight: 320
    visible: true
    // Прозрачный фон — Hyprland размоет то, что под окном, как у Shelf
    color: "transparent"
    title: (doc.modified ? "• " : "") + doc.name + " — Jot"

    Component.onCompleted: {
        doc.load(startFile)
        editor.focusText()
    }

    // ── Файлы ────────────────────────────────────────────────────────
    // Действие, которое ждёт, пока текущий документ будет сохранён или брошен
    // (новый, открыть другой, закрыть окно)
    property var afterSave: null
    property bool closingConfirmed: false

    // Можно ли бросить текущий документ без вопроса. У документа с файлом
    // правки просто дописываются на диск; спрашиваем только про безымянный
    // или когда запись не удалась
    function settle(then) {
        if (!doc.modified) { then(); return }
        if (doc.path !== "" && doc.save()) { then(); return }
        confirm.ask3("Сохранить изменения?",
                     doc.path === "" ? "Документ ещё ни разу не сохранялся."
                                     : "Не удалось записать " + doc.name + ".",
                     "Сохранить…", function () { win.afterSave = then; win.saveAs() },
                     "Не сохранять", then)
    }

    function newDoc() {
        settle(function () { doc.load(""); editor.focusText() })
    }
    function openDoc() {
        settle(function () { FilePortal.open("open", doc.folder) })
    }
    function openPath(path) {
        settle(function () { if (doc.load(path)) editor.focusText() })
    }
    function save() {
        if (doc.path === "") saveAs()
        else doc.save()
    }
    function saveAs() {
        FilePortal.save("saveAs", doc.folder, doc.path === "" ? "Без названия.md" : doc.name)
    }

    Connections {
        target: FilePortal
        function onChosen(purpose, paths) {
            if (purpose === "open") {
                if (doc.load(paths[0])) editor.focusText()
            } else if (purpose === "saveAs") {
                var path = paths[0]
                // Расширение подставляем сами, если в окне его стёрли
                if (path.indexOf(".", path.lastIndexOf("/")) < 0) path += ".md"
                if (doc.saveAs(path)) {
                    var then = win.afterSave
                    win.afterSave = null
                    if (then) then()
                }
            }
        }
        function onCancelled(purpose) { win.afterSave = null }
        function onFailed(purpose, message) {
            win.afterSave = null
            toast.show("Окно выбора файлов недоступно: " + message)
        }
    }

    Connections {
        target: doc
        function onFailed(message) { toast.show(message) }
        function onChangedOnDisk() {
            confirm.ask3("Файл изменён снаружи",
                         doc.name + " поменяли в другой программе, а здесь есть несохранённые правки.",
                         "Загрузить с диска", function () { doc.reload() },
                         "Оставить мои", function () { doc.save() })
        }
    }

    onClosing: (close) => {
        if (closingConfirmed || !doc.modified) return
        if (doc.path !== "" && doc.save()) return
        close.accepted = false
        settle(function () { win.closingConfirmed = true; win.close() })
    }

    // ── Клавиши ──────────────────────────────────────────────────────
    Shortcut { sequences: [StandardKey.New]; onActivated: win.newDoc() }
    Shortcut { sequences: [StandardKey.Open]; onActivated: win.openDoc() }
    Shortcut { sequences: [StandardKey.Save]; onActivated: win.save() }
    Shortcut { sequence: "Ctrl+Shift+S"; onActivated: win.saveAs() }
    Shortcut { sequence: "Ctrl+Q"; onActivated: win.close() }
    Shortcut { sequence: "Ctrl+B"; onActivated: editor.wrap("**") }
    Shortcut { sequence: "Ctrl+I"; onActivated: editor.wrap("*") }
    Shortcut { sequence: "Ctrl+E"; onActivated: editor.wrap("`") }
    Shortcut { sequence: "Ctrl+Shift+X"; onActivated: editor.wrap("~~") }

    // ── Вид ──────────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(Theme.base.r, Theme.base.g, Theme.base.b, Theme.glassAlpha)
        Behavior on color { ColorAnimation { duration: 300 } }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Верхняя полоса: файл слева, имя документа по центру
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 46

            Row {
                anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                spacing: 2
                GlassButton { text: "\u{f016}"; tooltip: "Новый  Ctrl+N"; onClicked: win.newDoc() }
                GlassButton { text: "\u{f07c}"; tooltip: "Открыть  Ctrl+O"; onClicked: win.openDoc() }
                GlassButton {
                    text: "\u{f0c7}"
                    tooltip: "Сохранить  Ctrl+S"
                    accentColor: Theme.jot
                    active: doc.modified
                    onClicked: win.save()
                }
            }

            Row {
                anchors.centerIn: parent
                spacing: 8
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: doc.name
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize
                    font.weight: Font.DemiBold
                    elide: Text.ElideMiddle
                    width: Math.min(implicitWidth, win.width - 320)
                }
                // Несохранённые правки — фиолетовая точка, как у вкладок редакторов
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 7; height: 7; radius: 3.5
                    color: Theme.jot
                    opacity: doc.modified ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: Theme.animDuration } }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.ink(0.08)
        }

        Editor {
            id: editor
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        // Строка состояния: где файл, сколько слов, где курсор
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 28

            Text {
                anchors { left: parent.left; leftMargin: 16; right: stats.left; rightMargin: 16
                          verticalCenter: parent.verticalCenter }
                text: doc.path === "" ? "не сохранён" : doc.path.replace(/^\/home\/[^/]+/, "~")
                color: Theme.overlay1
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 3
                elide: Text.ElideMiddle
            }
            Text {
                id: stats
                anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
                text: doc.words + " " + win.plural(doc.words, "слово", "слова", "слов")
                      + "   ·   " + doc.line + ":" + doc.column
                color: Theme.overlay1
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 3
            }
        }
    }

    function plural(n, one, few, many) {
        var m10 = n % 10, m100 = n % 100
        if (m10 === 1 && m100 !== 11) return one
        if (m10 >= 2 && m10 <= 4 && (m100 < 12 || m100 > 14)) return few
        return many
    }

    ConfirmDialog { id: confirm }

    // Тост: ошибки записи и чтения
    Rectangle {
        id: toast
        function show(msg) {
            toastText.text = msg
            opacity = 1
            toastTimer.restart()
        }
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 44 }
        width: Math.min(toastText.implicitWidth + 32, parent.width - 40)
        height: 36
        radius: Theme.btnRadius(height, false, Theme.widgetRadius)
        color: Theme.material ? Theme.surface2 : Qt.rgba(Theme.base.r, Theme.base.g, Theme.base.b, 0.96)
        border.width: 1
        border.color: Theme.material ? "transparent" : Theme.glassBorder
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.dur(200); easing.type: Easing.OutCubic } }
        transform: Translate { y: (1 - toast.opacity) * 12 }
        Text {
            id: toastText
            anchors.centerIn: parent
            width: parent.width - 32
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideMiddle
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 2
        }
        Timer { id: toastTimer; interval: 3500; onTriggered: toast.opacity = 0 }
    }
}
