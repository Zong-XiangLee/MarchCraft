.pragma library

function clamp(value, low, high) { return Math.max(low, Math.min(high, value)) }

function zoom(distance, delta) {
    return clamp(distance * Math.exp(-delta * 0.0012), 2, 240)
}

function orbit(pitch, yaw, dx, dy) {
    return { pitch: clamp(pitch - dy * 0.22, -89.5, -3), yaw: yaw - dx * 0.22 }
}

// Pan on the ground plane in camera-relative axes; scale with distance/FOV.
function pan(x, z, yaw, pitch, distance, height, dx, dy) {
    const radians = yaw * Math.PI / 180
    const scale = 2 * distance * Math.tan(22.5 * Math.PI / 180) / Math.max(1, height)
    const depth = dy * scale / Math.max(0.25, Math.sin(-pitch * Math.PI / 180))
    return { x: clamp(x - dx * scale * Math.cos(radians) - depth * Math.sin(radians), -100, 100),
             z: clamp(z + dx * scale * Math.sin(radians) - depth * Math.cos(radians), -70, 70) }
}
