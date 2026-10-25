import QtQuick
import QtQuick.Layouts
import Jot

Window {
    id: win

    // Файлы из командной строки: открываются вкладками поверх прошлой сессии
    property var startFiles: []

    width: 1000
    height: 720
    minimumWidth: 480
    minimumHeight: 320
    visible: true
    // Прозрачный фон — Hyprland размоет то, что под окном, как у Shelf
    color: "transparent"
    title: (doc.modified ? "• " : "") + doc.name + " — Jot"

    // ── Вкладки ──────────────────────────────────────────────────────
    // Editor в порядке полоски; tab и doc — текущей вкладки
    property var tabs: []
    property Editor tab: null
    readonly property int current: Math.max(0, tabs.indexOf(tab))
    readonly property Document doc: tab ? tab.document : stub.document

    // ── Панель папки ─────────────────────────────────────────────────
    // Показывает папку открытого документа; выбранная явно (Ctrl+Shift+O, jot ~/notes)
    // закрепляется и больше не следует за документами — режим хранилища
    FolderModel { id: folders }
    property bool folderPinned: false
    // undefined — по ширине окна; F9 закрепляет выбор, как в Shelf
    property var sidebarShown: undefined
    readonly property bool sidebarVisible: sidebarShown ?? width >= 760
    readonly property string docPath: doc.path
    onDocPathChanged: followDoc()

    function inside(path, dir) { return dir !== "" && (path === dir || path.startsWith(dir + "/")) }
    function followDoc() {
        if (folderPinned || doc.untitled) return
        if (!inside(doc.path, folders.root)) folders.root = doc.folder
    }
    function openFolder(path, pin) {
        folders.root = path
        folderPinned = pin
        if (!sidebarVisible) sidebarShown = true
        saveSession()
    }
    function togglePin() {
        folderPinned = !folderPinned
        followDoc()
        saveSession()
    }
    function newNote() {
        var p = folders.createNote("")
        if (p) openPath(p)
        else toast.show("Не удалось создать заметку в " + folders.rootName)
    }
    Connections {
        target: folders
        function onRootChanged() { win.saveSession() }
        function onExpandedChanged() { win.saveSession() }
    }

    // Пустышка на время, пока вкладок ещё нет: привязкам к doc всегда есть что читать
    Editor { id: stub; visible: false }

    Component {
        id: editorComp
        Editor {
            id: page
            anchors.fill: parent
            visible: page === win.tab
            onChanged: win.saveSession()
        }
    }

    function newTab() {
        var ed = editorComp.createObject(pages)
        ed.document.failed.connect(toast.show)
        ed.document.changedOnDisk.connect(function () { win.askDisk(ed) })
        ed.document.load("")
        var list = tabs.slice()
        list.splice(current + 1, 0, ed)
        tabs = list
        switchTo(ed)
        return ed
    }
    function switchTo(ed) {
        tab = ed
        ed.focusText()
        saveSession()
    }
    function tabOf(path) {
        for (var i = 0; i < tabs.length; ++i)
            if (tabs[i].document.path === path) return tabs[i]
        return null
    }
    function dropTab(ed) {
        var i = tabs.indexOf(ed)
        var list = tabs.slice()
        list.splice(i, 1)
        tabs = list
        if (tab === ed) tab = list.length > 0 ? list[Math.min(i, list.length - 1)] : null
        ed.destroy()
        // Последнюю закрыли — остаётся чистый лист, а не пустое окно.
        // Пока сессия поднимается, пустоту заполнит она сама
        if (tabs.length === 0) { if (restored) newTab() }
        else switchTo(tab)
    }
    function closeTab(i) {
        var ed = tabs[i]
        if (!ed) return
        var d = ed.document
        if (!d.modified || (!d.untitled && d.save())) { dropTab(ed); return }
        switchTo(ed)
        confirm.ask3("Сохранить «" + d.name + "»?",
                     d.untitled ? "Документ ещё ни разу не сохранялся." : "Не удалось записать файл.",
                     "Сохранить…", function () { win.saveAs(ed, function () { win.dropTab(ed) }) },
                     "Не сохранять", function () { win.dropTab(ed) })
    }
    function moveTab(from, to) {
        var list = tabs.slice()
        list.splice(to, 0, list.splice(from, 1)[0])
        tabs = list   // текущая вкладка та же, её номер пересчитается сам
        saveSession()
    }
    function cycleTab(step) { switchTo(tabs[(current + step + tabs.length) % tabs.length]) }

    // ── Файлы ────────────────────────────────────────────────────────
    function openPath(path) {
        if (folders.isDir(path)) { openFolder(path, true); return }
        var ed = tabOf(path)
        if (ed) { switchTo(ed); return }
        // Чистый безымянный лист занимаем, а не плодим рядом новую вкладку
        var reuse = tab && tab.document.blank && !tab.document.modified
        ed = reuse ? tab : newTab()
        if (ed.document.load(path)) switchTo(ed)
        else if (!reuse) dropTab(ed)
    }
    function openDoc() { FilePortal.open("open", doc.folder) }
    function save() {
        if (doc.untitled) saveAs(tab, null)
        else doc.save()
    }
    // Сохранить как: окно выбора отвечает позже, запоминаем, для какой вкладки
    property var saveAsTab: null
    property var afterSave: null
    function saveAs(ed, then) {
        saveAsTab = ed
        afterSave = then
        var d = ed.document
        FilePortal.save("saveAs", d.folder, d.untitled ? d.name.replace(/[\/…]/g, "").trim() + ".md" : d.name)
    }

    function askDisk(ed) {
        switchTo(ed)
        confirm.ask3("Файл изменён снаружи",
                     ed.document.name + " поменяли в другой программе, а здесь есть несохранённые правки.",
                     "Загрузить с диска", function () { ed.document.reload() },
                     "Оставить мои", function () { ed.document.save() })
    }

    Connections {
        target: FilePortal
        function onChosen(purpose, paths) {
            if (purpose === "open") {
                paths.forEach(win.openPath)
            } else if (purpose === "folder") {
                win.openFolder(paths[0], true)
            } else if (purpose === "saveAs" && win.saveAsTab) {
                var path = paths[0]
                // Расширение подставляем сами, если в окне его стёрли
                if (path.indexOf(".", path.lastIndexOf("/")) < 0) path += ".md"
                var then = win.afterSave
                var ok = win.saveAsTab.document.saveAs(path)
                win.saveAsTab = null
                win.afterSave = null
                if (ok) {
                    win.saveSession()
                    if (then) then()
                }
            }
        }
        function onCancelled(purpose) { win.saveAsTab = null; win.afterSave = null }
        function onFailed(purpose, message) {
            win.saveAsTab = null
            win.afterSave = null
            toast.show("Окно выбора файлов недоступно: " + message)
        }
    }

    // Другой `jot файл.md` передал файлы сюда
    Connections {
        target: Instance
        function onOpenRequested(paths) {
            paths.forEach(win.openPath)
            win.raise()
            win.requestActivate()
        }
    }

    // ── Сессия ───────────────────────────────────────────────────────
    // Пишется по ходу работы (на случай сбоя) и при закрытии. Несохранённое —
    // черновиком: безымянные вкладки переживают закрытие окна без вопросов
    property bool restored: false
    function saveSession() { if (restored) sessionTimer.restart() }
    function writeSession() {
        sessionTimer.stop()
        Session.save(tabs.map(function (ed) {
            var d = ed.document
            var t = { path: d.path, cursor: d.cursorPosition }
            if (d.modified && !d.blank) t.draft = d.text()
            return t
        }).filter(function (t) { return t.path !== "" || t.draft !== undefined }), current, {
            folder: folders.root,
            pinned: folderPinned,
            expanded: folders.expanded,
            // null — «по ширине окна»: undefined в JSON не попадает
            sidebar: sidebarShown === undefined ? null : sidebarShown
        })
    }
    Timer { id: sessionTimer; interval: 800; onTriggered: win.writeSession() }

    Component.onCompleted: {
        var s = Session.load()
        folders.expanded = s.expanded || []
        folders.root = s.folder || ""
        folderPinned = !!s.pinned
        if (s.sidebar !== undefined && s.sidebar !== null) sidebarShown = s.sidebar
        var want = null
        ;(s.tabs || []).forEach(function (t, i) {
            var ed = win.newTab()
            if (!ed.document.restore(t.path || "", t.draft !== undefined, t.draft || "", t.cursor || 0)) {
                win.dropTab(ed)
                return
            }
            if (i === s.current) want = ed
        })
        if (want) switchTo(want)
        startFiles.forEach(openPath)
        if (tabs.length === 0) newTab()
        restored = true
        writeSession()
    }

    onClosing: {
        // Файлы дописываем на диск, безымянное уходит в сессию черновиком
        tabs.forEach(function (ed) {
            if (ed.document.modified && !ed.document.untitled) ed.document.save()
        })
        writeSession()
    }

    // ── Клавиши ──────────────────────────────────────────────────────
    Shortcut { sequences: [StandardKey.New, "Ctrl+T"]; onActivated: win.newTab() }
    Shortcut { sequences: [StandardKey.Open]; onActivated: win.openDoc() }
    Shortcut { sequences: [StandardKey.Save]; onActivated: win.save() }
    Shortcut { sequence: "Ctrl+Shift+S"; onActivated: win.saveAs(win.tab, null) }
    Shortcut { sequences: ["Ctrl+W", "Ctrl+F4"]; onActivated: win.closeTab(win.current) }
    Shortcut { sequences: ["Ctrl+Tab", "Ctrl+PgDown"]; onActivated: win.cycleTab(1) }
    Shortcut { sequences: ["Ctrl+Shift+Tab", "Ctrl+Backtab", "Ctrl+PgUp"]; onActivated: win.cycleTab(-1) }
    Shortcut { sequence: "Ctrl+Q"; onActivated: win.close() }
    Shortcut { sequence: "F9"; onActivated: { win.sidebarShown = !win.sidebarVisible; win.saveSession() } }
    Shortcut { sequence: "Ctrl+Shift+O"; onActivated: FilePortal.openFolder("folder", folders.root || win.doc.folder) }
    Shortcut { sequence: "Ctrl+B"; onActivated: win.tab.wrap("**") }
    Shortcut { sequence: "Ctrl+I"; onActivated: win.tab.wrap("*") }
    Shortcut { sequence: "Ctrl+E"; onActivated: win.tab.wrap("`") }
    Shortcut { sequence: "Ctrl+Shift+X"; onActivated: win.tab.wrap("~~") }
    // Alt+1…8 — вкладка по номеру, Alt+9 — последняя
    Repeater {
        model: 9
        Item {
            required property int index
            Shortcut {
                sequence: "Alt+" + (parent.index + 1)
                onActivated: {
                    var i = parent.index === 8 ? win.tabs.length - 1 : parent.index
                    if (i < win.tabs.length) win.switchTo(win.tabs[i])
                }
            }
        }
    }

    // ── Вид ──────────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(Theme.base.r, Theme.base.g, Theme.base.b, Theme.glassAlpha)
        Behavior on color { ColorAnimation { duration: 300 } }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            // Прячется и появляется, плавно меняя ширину, как в Shelf
            Layout.preferredWidth: win.sidebarVisible ? 250 : 0
            Behavior on Layout.preferredWidth { NumberAnimation { duration: Theme.dur(200); easing.type: Easing.OutCubic } }
            Layout.fillHeight: true
            visible: Layout.preferredWidth > 0.5
            clip: true
            model: folders
            pinned: win.folderPinned
            currentPath: win.doc.path
            onOpenFile: (p) => win.openPath(p)
            onChooseFolder: FilePortal.openFolder("folder", folders.root || win.doc.folder)
            onNewNote: win.newNote()
            onTogglePin: win.togglePin()
        }
        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            visible: win.sidebarVisible
            color: Theme.ink(0.08)
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Верхняя полоса: открыть и сохранить, дальше вкладки
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                spacing: 2

                GlassButton { text: "\u{f07c}"; tooltip: "Открыть  Ctrl+O"; onClicked: win.openDoc() }
                GlassButton {
                    text: "\u{f0c7}"
                    tooltip: "Сохранить  Ctrl+S"
                    accentColor: Theme.jot
                    active: win.doc.modified
                    onClicked: win.save()
                }
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 20
                    Layout.leftMargin: 6
                    Layout.rightMargin: 6
                    color: Theme.ink(0.12)
                }
                TabStrip {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    tabs: win.tabs
                    currentTab: win.tab
                    onActivate: (i) => win.switchTo(win.tabs[i])
                    onCloseTab: (i) => win.closeTab(i)
                    onMoveTab: (from, to) => win.moveTab(from, to)
                    onNewTab: win.newTab()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.ink(0.08)
            }

            // Все вкладки живут здесь, видна только текущая
            Item {
                id: pages
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
                    text: win.doc.untitled ? "не сохранён" : win.doc.path.replace(/^\/home\/[^/]+/, "~")
                    color: Theme.overlay1
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 3
                    elide: Text.ElideMiddle
                }
                Text {
                    id: stats
                    anchors { right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
                    text: win.doc.words + " " + win.plural(win.doc.words, "слово", "слова", "слов")
                          + "   ·   " + win.doc.line + ":" + win.doc.column
                    color: Theme.overlay1
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSize - 3
                }
            }
        }
    }

    // Файлы, брошенные в окно (из Shelf), открываются вкладками
    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            drop.urls.forEach(function (u) {
                var s = u.toString()
                if (s.startsWith("file://")) win.openPath(decodeURIComponent(s.substring(7)))
            })
            drop.acceptProposedAction()
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
