import QtQuick

Canvas {
    id: root
    property string name: "home"
    property color iconColor: MarchCraftTheme.textPrimary
    implicitWidth: MarchCraftTheme.iconMedium
    implicitHeight: MarchCraftTheme.iconMedium
    onNameChanged: requestPaint()
    onIconColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()
    onPaint: {
        const c = getContext("2d")
        c.reset(); c.strokeStyle = iconColor; c.fillStyle = iconColor
        c.lineWidth = 1.8; c.lineCap = "round"; c.lineJoin = "round"
        const w = width, h = height, x = w / 2, y = h / 2
        c.beginPath()
        if (name === "home") {
            c.moveTo(3, y); c.lineTo(x, 3); c.lineTo(w - 3, y)
            c.moveTo(5.5, y - 1); c.lineTo(5.5, h - 3); c.lineTo(w - 5.5, h - 3); c.lineTo(w - 5.5, y - 1)
        } else if (name === "undo" || name === "redo") {
            if (name === "undo") { c.moveTo(7, 5); c.lineTo(3, 9); c.lineTo(7, 13); c.moveTo(4, 9); c.bezierCurveTo(14, 5, 17, 10, 15, 15) }
            else { c.moveTo(w - 7, 5); c.lineTo(w - 3, 9); c.lineTo(w - 7, 13); c.moveTo(w - 4, 9); c.bezierCurveTo(6, 5, 3, 10, 5, 15) }
        } else if (name === "plus") {
            c.moveTo(x, 4); c.lineTo(x, h - 4); c.moveTo(4, y); c.lineTo(w - 4, y)
        } else if (name === "grid") {
            c.rect(3, 3, w - 6, h - 6); c.moveTo(x, 3); c.lineTo(x, h - 3); c.moveTo(3, y); c.lineTo(w - 3, y)
        } else if (name === "chevron-left") { c.moveTo(w - 6, 4); c.lineTo(6, y); c.lineTo(w - 6, h - 4) }
        else if (name === "chevron-right") { c.moveTo(6, 4); c.lineTo(w - 6, y); c.lineTo(6, h - 4) }
        else if (name === "play") { c.moveTo(6, 3); c.lineTo(w - 4, y); c.lineTo(6, h - 3); c.closePath(); c.fill(); return }
        else if (name === "stop") { c.rect(5, 5, w - 10, h - 10); c.fill(); return }
        else if (name === "more") { for (let i=0;i<3;++i) { c.moveTo(4+i*6,y); c.arc(4+i*6,y,1.2,0,Math.PI*2) } c.fill(); return }
        c.stroke()
    }
}
