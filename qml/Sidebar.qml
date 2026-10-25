pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import Jot

// Боковая панель: дерево заметок одной папки, как места в Shelf.
// Папка — либо та, где лежит открытый документ, либо выбранная явно
// и закреплённая булавкой (режим хранилища)
Rectangle {
    id: side

    property FolderModel model: null
    property bool pinned: false
    // Путь открытого документа — его строка подсвечена
    property string currentPath: ""

    signal openFile(string path)
    signal chooseFolder()
    signal newNote()
    signal togglePin()

    color: Theme.material ? Theme.surface0 : Qt.rgba(0, 0, 0, Theme.light ? 0.04 : 0.14)

    // Строку открытого документа держим в поле зрения
    function showCurrent() {
        if (!model) return
        var row = model.reveal(currentPath)
        if (row >= 0) list.positionViewAtIndex(row, ListView.Contain)
    }
    onCurrentPathChanged: Qt.callLater(showCurrent)

    // Шапка — та же высота, что у полосы вкладок справа
    Item {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 46

        Text {
            anchors { left: parent.left; leftMargin: 16; right: buttons.left; rightMargin: 6
                      verticalCenter: parent.verticalCenter }
            text: side.model && side.model.root !== "" ? side.model.rootName : "Заметки"
            elide: Text.ElideMiddle
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 1
            font.weight: Font.DemiBold
        }
        Row {
            id: buttons
            anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
            spacing: 0
            GlassButton {
                text: "\u{f08d}"
                iconSize: Theme.fontSize - 2
                visible: side.model && side.model.root !== ""
                active: side.pinned
                tooltip: side.pinned ? "Папка закреплена — не меняется при открытии файлов"
                                     : "Закрепить папку"
                onClicked: side.togglePin()
            }
            GlassButton {
                text: "\u{f07c}"
                iconSize: Theme.fontSize - 2
                tooltip: "Открыть папку  Ctrl+Shift+O"
                onClicked: side.chooseFolder()
            }
            GlassButton {
                text: "\u{f067}"
                iconSize: Theme.fontSize - 2
                visible: side.model && side.model.root !== ""
                tooltip: "Новая заметка в этой папке"
                onClicked: side.newNote()
            }
        }
    }

    Rectangle {
        id: rule
        anchors { left: parent.left; right: parent.right; top: header.bottom }
        height: 1
        color: Theme.ink(0.08)
    }

    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: rule.bottom; bottom: parent.bottom
                  topMargin: 6; bottomMargin: 6 }
        clip: true
        model: side.model
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        // Перестройка модели (файл появился или пропал) не должна сбрасывать прокрутку
        property real keepY: 0
        Connections {
            target: side.model
            function onModelAboutToBeReset() { list.keepY = list.contentY }
            function onModelReset() { list.contentY = Math.min(list.keepY, Math.max(0, list.contentHeight - list.height)) }
        }

        delegate: Item {
            id: row
            required property int index
            required property string name
            required property string path
            required property bool isDir
            required property int depth
            required property bool expanded
            readonly property bool current: !isDir && path === side.currentPath

            width: ListView.view.width
            height: 30

            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: 6; anchors.rightMargin: 6
                radius: Theme.btnRadius(height, false, Theme.widgetRadius)
                color: row.current ? Theme.activeFill(Theme.accent)
                     : ma.containsMouse ? Theme.hoverFill : "transparent"
                Behavior on color { ColorAnimation { duration: 90 } }
            }
            // Стрелка у папок, у файлов на её месте пусто — имена выровнены
            Text {
                id: chevron
                x: 14 + row.depth * 14
                width: 12
                anchors.verticalCenter: parent.verticalCenter
                text: row.isDir ? (row.expanded ? "\u{f078}" : "\u{f054}") : ""
                color: Theme.overlay2
                font.family: Theme.iconFont
                font.pixelSize: Theme.fontSize - 6
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                id: glyph
                anchors { left: chevron.right; leftMargin: 6; verticalCenter: parent.verticalCenter }
                width: 16
                text: row.isDir ? (row.expanded ? "\u{f07c}" : "\u{f07b}") : "\u{f0f6}"
                color: row.current ? Theme.activeFg(Theme.accent)
                     : row.isDir ? Theme.accent : Theme.subtext0
                font.family: Theme.iconFont
                font.pixelSize: Theme.fontSize - 2
            }
            Text {
                anchors { left: glyph.right; leftMargin: 8; right: parent.right; rightMargin: 14
                          verticalCenter: parent.verticalCenter }
                // Расширение .md у всех одинаковое — не показываем
                text: row.isDir ? row.name : row.name.replace(/\.(md|markdown|mdown)$/i, "")
                elide: Text.ElideRight
                color: row.current ? Theme.activeFg(Theme.accent) : Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 1
            }
            MouseArea {
                id: ma
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: row.isDir ? side.model.toggle(row.index) : side.openFile(row.path)
            }
        }
    }

    // Пусто: папка не выбрана или заметок в ней нет
    Column {
        anchors { left: parent.left; right: parent.right; top: rule.bottom; topMargin: 28 }
        spacing: 14
        visible: !side.model || side.model.count === 0
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            leftPadding: 20; rightPadding: 20
            text: side.model && side.model.root !== "" ? "В этой папке пока нет заметок"
                                                       : "Откройте папку с заметками —\nони появятся здесь"
            color: Theme.overlay1
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 2
        }
        TextButton {
            anchors.horizontalCenter: parent.horizontalCenter
            text: side.model && side.model.root !== "" ? "Новая заметка" : "Открыть папку…"
            onClicked: side.model && side.model.root !== "" ? side.newNote() : side.chooseFolder()
        }
    }
}
