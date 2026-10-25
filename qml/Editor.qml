import QtQuick
import QtQuick.Controls.Basic
import Jot

// Поле редактора: колонка текста по центру окна, прокрутка — на всю ширину,
// чтобы колесо работало и над полями. Подсветку, файл и курсорную строку
// ведёт Document
FocusScope {
    id: root

    readonly property Document document: doc
    readonly property alias area: edit

    function wrap(marker) {
        var r = doc.toggleWrap(edit.selectionStart, edit.selectionEnd, marker)
        edit.select(r[0], r[1])
    }
    function focusText() { edit.forceActiveFocus() }

    Document {
        id: doc
        textDocument: edit.textDocument
        cursorPosition: edit.cursorPosition
        baseSize: Theme.editorSize
        codeFamily: Theme.codeFont
        palette: ({
            text: Theme.text,
            subtext: Theme.subtext0,
            marker: Theme.overlay0,
            markerActive: Theme.overlay2,
            accent: Theme.jot,
            link: Theme.blue,
            code: Theme.subtext1,
            codeBg: Theme.ink(0.07)
        })
    }

    Connections {
        target: doc
        function onLoaded(cursor) {
            edit.cursorPosition = cursor
            edit.deselect()
        }
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        TextArea {
            id: edit
            focus: true
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.Wrap
            // Колонка не шире editorMeasure, поля — поровну по бокам
            readonly property real side: Math.max(28, (scroll.width - Theme.editorMeasure) / 2)
            leftPadding: side
            rightPadding: side
            topPadding: 28
            // Большой отступ снизу (полэкрана, «прокрутка за конец») ломает TextEdit
            // в Qt 6.10: блоки ниже первого экрана перестают рисоваться
            bottomPadding: 48
            background: null
            color: Theme.text
            font.family: Theme.editorFont
            font.pixelSize: Theme.editorSize
            selectByMouse: true
            persistentSelection: true
            selectionColor: Qt.rgba(Theme.jot.r, Theme.jot.g, Theme.jot.b, 0.32)
            selectedTextColor: Theme.text
            placeholderText: "Начните писать…"
            placeholderTextColor: Theme.overlay1

            // Фирменный курсор: тонкая фиолетовая черта, мигает мягко
            cursorDelegate: Rectangle {
                width: 2
                color: Theme.jot
                radius: 1
                visible: edit.activeFocus
                SequentialAnimation on opacity {
                    running: edit.activeFocus && Theme.animations
                    loops: Animation.Infinite
                    NumberAnimation { to: 1; duration: 0 }
                    PauseAnimation { duration: 500 }
                    NumberAnimation { to: 0.15; duration: 220; easing.type: Easing.InOutQuad }
                    PauseAnimation { duration: 280 }
                    NumberAnimation { to: 1; duration: 220; easing.type: Easing.InOutQuad }
                }
                // Набор текста — курсор сразу виден, мигание с начала
                Connections {
                    target: edit
                    function onCursorPositionChanged() { parent.opacity = 1 }
                }
            }

            Keys.onPressed: (event) => {
                if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                        && !(event.modifiers & (Qt.ShiftModifier | Qt.ControlModifier | Qt.AltModifier))
                        && edit.selectionStart === edit.selectionEnd) {
                    var pos = doc.continueBlock(edit.cursorPosition)
                    if (pos >= 0) {
                        edit.cursorPosition = pos
                        event.accepted = true
                    }
                }
            }
        }
    }
}
