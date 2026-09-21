import QtQuick

QtObject {
    function options(kind, start, end, count) {
        const dx = end.x - start.x, dy = end.y - start.y
        const width = Math.max(2, Math.abs(dx)), height = Math.max(2, Math.abs(dy))
        const size = Math.max(width, height)
        const result = { centerX: (start.x + end.x) / 2, centerY: (start.y + end.y) / 2,
            width: width, height: height, rotation: 0, placementMode: "selection" }
        if (kind === "line") {
            result.width = Math.hypot(dx, dy)
            result.rotation = Math.atan2(dy, dx) * 180 / Math.PI
        } else if (kind === "circle" || kind === "arc") {
            result.centerX = start.x; result.centerY = start.y
            result.radius = Math.hypot(dx, dy)
            result.startAngle = kind === "circle" ? 0 : Math.atan2(dy, dx) * 180 / Math.PI - 90
            result.sweepAngle = kind === "circle" ? 360 : 180
        } else if (["triangle", "diamond", "polygon", "star", "spiral"].includes(kind)) {
            result.width = size; result.height = size
            result.sides = 6; result.points = 5
            result.outerRadius = size / 2; result.turns = 1.5; result.spiralStyle = "drill"
        } else if (kind === "block") {
            result.rows = Math.max(1, Math.ceil(Math.sqrt(count)))
            const columns = Math.max(1, Math.ceil(count / result.rows))
            result.spacing = Math.max(1, Math.min(width / Math.max(1, columns - 1),
                                                 height / Math.max(1, result.rows - 1)))
        }
        return result
    }
}
