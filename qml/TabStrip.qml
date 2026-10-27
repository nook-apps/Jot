pragma ComponentBehavior: Bound

import QtQuick
import Glaze
import Jot

// Полоска вкладок, как у Shelf: тянутся мышью, средняя кнопка закрывает.
// У вкладки с несохранёнными правками вместо крестика — фиолетовая точка,
// крестик появляется под мышью
Item {
    id: strip

    property var tabs: []         // Editor
    // Текущая вкладка — сам объект, а не номер: при перестановке кнопки пересоздаются
    // по новому списку раньше, чем приходит новый номер (см. Shelf, issue #1)
    property var currentTab: null

    signal activate(int index)
    signal closeTab(int index)
    signal moveTab(int from, int to)
    signal newTab()

    // Перетаскивание вкладки: откуда и на какое место. Остальные отъезжают,
    // освобождая слот; порядок меняется, когда вкладка доехала до места
    property int dragFrom: -1
    property int dragTo: -1
    function slotOf(i) {
        if (dragFrom < 0) return i
        if (i === dragFrom) return dragTo
        if (dragFrom < dragTo && i > dragFrom && i <= dragTo) return i - 1
        if (dragTo < dragFrom && i >= dragTo && i < dragFrom) return i + 1
        return i
    }
    Timer {
        id: commitMove
        interval: 200   // чуть дольше анимации x
        onTriggered: {
            var from = strip.dragFrom, to = strip.dragTo
            strip.dragFrom = -1
            strip.dragTo = -1
            if (from >= 0 && to !== from) strip.moveTab(from, to)
        }
    }

    readonly property real tabWidth: Math.max(96, Math.min(220, (width - addBtn.width - 6) / Math.max(1, tabs.length)))

    Repeater {
        model: strip.tabs

        delegate: Item {
            id: tabItem
            required property var modelData
            required property int index
            readonly property Document doc: modelData.document
            readonly property bool current: modelData === strip.currentTab
            property bool dragging: false
            property real dragX: 0

            width: strip.tabWidth
            height: strip.height
            x: dragging ? dragX : strip.slotOf(index) * strip.tabWidth
            z: dragging || index === strip.dragFrom ? 2 : 0
            Behavior on x { enabled: !tabItem.dragging; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

            // Едущая вкладка проходит над соседями — плотная подложка, чтобы их подписи не просвечивали
            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: Theme.btnRadius(height, false, Theme.widgetRadius)
                visible: tabItem.index === strip.dragFrom
                color: Theme.material ? Theme.surface1 : Theme.base
            }
            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: Theme.btnRadius(height, false, Theme.widgetRadius)
                color: tabItem.current ? Theme.activeFill(Theme.accent)
                     : ma.containsMouse ? Theme.hoverFill : "transparent"
                border.width: 1
                border.color: tabItem.current ? Theme.edge(Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, Theme.activeBorderAlpha))
                            : "transparent"
                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
            }

            Text {
                anchors { left: parent.left; leftMargin: 14; right: closeBtn.left; rightMargin: 4; verticalCenter: parent.verticalCenter }
                text: tabItem.doc.name
                elide: Text.ElideRight
                color: tabItem.current ? Theme.activeFg(Theme.accent) : Theme.text
                // Безымянная — курсивом: её ещё нет на диске
                font.italic: tabItem.doc.untitled
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize - 2
            }

            MouseArea {
                id: ma
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                // В координатах полоски: сама вкладка едет под курсором
                property real pressX: 0
                onPressed: (m) => {
                    pressX = mapToItem(strip, m.x, 0).x
                    if (m.button === Qt.LeftButton) strip.activate(tabItem.index)
                }
                onPositionChanged: (m) => {
                    if (!(m.buttons & Qt.LeftButton) || strip.tabs.length < 2 || commitMove.running) return
                    var dx = mapToItem(strip, m.x, 0).x - pressX
                    if (!tabItem.dragging && Math.abs(dx) < Application.styleHints.startDragDistance) return
                    tabItem.dragging = true
                    var maxX = (strip.tabs.length - 1) * strip.tabWidth
                    tabItem.dragX = Math.max(0, Math.min(maxX, tabItem.index * strip.tabWidth + dx))
                    strip.dragFrom = tabItem.index
                    strip.dragTo = Math.round(tabItem.dragX / strip.tabWidth)
                }
                onReleased: {
                    if (!tabItem.dragging) return
                    // Отпустили — x снова от slotOf, Behavior довозит вкладку до слота
                    tabItem.dragging = false
                    commitMove.start()
                }
                onClicked: (m) => { if (m.button === Qt.MiddleButton) strip.closeTab(tabItem.index) }
            }

            Item {
                id: closeBtn
                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                width: 20; height: 20
                readonly property bool hovered: ma.containsMouse || closeMa.containsMouse

                Rectangle {
                    anchors.fill: parent
                    radius: 10
                    visible: closeBtn.hovered || (tabItem.current && !tabItem.doc.modified)
                    color: closeMa.containsMouse ? Theme.hoverFill : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "\u{f00d}"
                        color: closeMa.containsMouse ? Theme.text : Theme.subtext0
                        font.family: Theme.iconFont
                        font.pixelSize: Theme.fontSize - 4
                    }
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 7; height: 7; radius: 3.5
                    color: Theme.brand
                    visible: tabItem.doc.modified && !closeBtn.hovered
                }
                MouseArea {
                    id: closeMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: strip.closeTab(tabItem.index)
                }
            }
        }
    }

    GlassButton {
        id: addBtn
        x: Math.min(strip.tabs.length * strip.tabWidth + 2, strip.width - width)
        anchors.verticalCenter: parent.verticalCenter
        text: "\u{f067}"
        iconSize: Theme.fontSize - 2
        tooltip: "Новая вкладка  Ctrl+T"
        onClicked: strip.newTab()
    }
}
