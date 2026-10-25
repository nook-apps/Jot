import QtQuick
import QtQuick.Controls.Basic
import Jot

// Подтверждение необратимого действия. Enter — да, Esc — отмена.
// altText — третья кнопка («Не сохранять»), слева от отмены
GlassDialog {
    id: dlg

    property string title: ""
    property string message: ""
    property string actionText: "Удалить"
    property var acceptFn: null
    property bool danger: true
    property string altText: ""
    property var altFn: null

    function ask(t, msg, action, accept) {
        title = t
        message = msg
        actionText = action
        acceptFn = accept
        danger = true
        altText = ""
        altFn = null
        open()
    }
    // Спросить с тремя ответами: action — основной, alt — второй, отмена
    function ask3(t, msg, action, accept, alt, altAccept) {
        ask(t, msg, action, accept)
        danger = false
        altText = alt
        altFn = altAccept
    }
    function chooseAlt() {
        var f = altFn
        close()
        if (f) f()
    }
    function accept() {
        var f = acceptFn
        close()
        if (f) f()
    }

    padding: 22
    width: 420
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: okBtn.forceActiveFocus()
    onClosed: { acceptFn = null; altFn = null }

    contentItem: Column {
        spacing: 10

        Row {
            spacing: 12
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "\u{f071}"
                color: dlg.danger ? Theme.red : Theme.jot
                font.family: Theme.iconFont
                font.pixelSize: Theme.fontSize + 6
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: dlg.availableWidth - 40
                text: dlg.title
                wrapMode: Text.Wrap
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize + 1
                font.weight: Font.DemiBold
            }
        }
        Text {
            width: dlg.availableWidth
            text: dlg.message
            visible: text.length > 0
            wrapMode: Text.Wrap
            color: Theme.subtext0
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize - 1
        }
        Item { width: 1; height: 6 }
        Row {
            anchors.right: parent.right
            spacing: 8
            TextButton {
                text: dlg.altText
                visible: dlg.altText.length > 0
                onClicked: dlg.chooseAlt()
            }
            TextButton {
                text: "Отмена"
                onClicked: dlg.close()
            }
            TextButton {
                id: okBtn
                text: dlg.actionText
                danger: dlg.danger
                primary: !dlg.danger
                onClicked: dlg.accept()
            }
        }
    }
}
