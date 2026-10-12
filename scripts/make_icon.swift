import AppKit
import Foundation

let output = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
for (name, size) in [("icon_16x16",16), ("icon_16x16@2x",32), ("icon_32x32",32), ("icon_32x32@2x",64), ("icon_128x128",128), ("icon_128x128@2x",256), ("icon_256x256",256), ("icon_256x256@2x",512), ("icon_512x512",512), ("icon_512x512@2x",1024)] {
    let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: size, pixelsHigh: size,
        bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
        colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
    let context = NSGraphicsContext.current!.cgContext
    context.scaleBy(x: CGFloat(size)/1024, y: CGFloat(size)/1024)
    let frame = NSRect(x: 64, y: 64, width: 896, height: 896)
    let base = NSBezierPath(roundedRect: frame, xRadius: 198, yRadius: 198)
    let gradient = NSGradient(starting: NSColor(calibratedRed: 0.22, green: 0.81, blue: 0.70, alpha: 1),
        ending: NSColor(calibratedRed: 0.07, green: 0.49, blue: 0.48, alpha: 1))!
    gradient.draw(in: base, angle: -55)
    NSColor.white.withAlphaComponent(0.95).setStroke()
    let photo = NSBezierPath(roundedRect: NSRect(x: 248, y: 291, width: 528, height: 442), xRadius: 50, yRadius: 50)
    photo.lineWidth = 34
    photo.stroke()
    let mountain = NSBezierPath()
    mountain.move(to: NSPoint(x: 271, y: 335))
    mountain.line(to: NSPoint(x: 430, y: 512))
    mountain.line(to: NSPoint(x: 551, y: 382))
    mountain.line(to: NSPoint(x: 641, y: 469))
    mountain.line(to: NSPoint(x: 751, y: 352))
    mountain.lineWidth = 34
    mountain.lineJoinStyle = .round
    mountain.lineCapStyle = .round
    mountain.stroke()
    NSColor.white.withAlphaComponent(0.95).setFill()
    NSBezierPath(ovalIn: NSRect(x: 579, y: 554, width: 87, height: 87)).fill()
    NSGraphicsContext.restoreGraphicsState()
    try bitmap.representation(using: .png, properties: [:])!.write(to: output.appendingPathComponent(name + ".png"))
}
