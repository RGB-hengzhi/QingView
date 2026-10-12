#!/usr/bin/env swift
// Synthetic fixtures only: no user photographs or third-party assets.
// Run: xcrun swift tests/generate_fixtures.swift tests/fixtures
// WebP is added by generate_webp_fixture.cpp using the same Qt plugins as QingView.

import Foundation
import CoreGraphics
import ImageIO

enum FixtureError: Error {
    case imageCreation
    case unsupportedDestination(String)
    case writeFailed(String)
}

let root = URL(fileURLWithPath: CommandLine.arguments.dropFirst().first ?? "tests/fixtures", isDirectory: true)
let formats = root.appendingPathComponent("formats", isDirectory: true)
let navigation = root.appendingPathComponent("navigation", isDirectory: true)
try FileManager.default.createDirectory(at: formats, withIntermediateDirectories: true)
try FileManager.default.createDirectory(at: navigation, withIntermediateDirectories: true)

let sRGB = CGColorSpace(name: CGColorSpace.sRGB)!

// The first data row is the image's top row. Distinct quadrants make EXIF
// orientation and accidental vertical flips observable without OCR.
func pattern(width: Int = 96, height: Int = 64, alpha: Bool = false) throws -> CGImage {
    var bytes = [UInt8](repeating: 0, count: width * height * 4)
    for y in 0..<height {
        for x in 0..<width {
            let color: [UInt8]
            if y < height / 2 {
                color = x < width / 2 ? [255, 0, 0] : [0, 255, 0]
            } else {
                color = x < width / 2 ? [0, 0, 255] : [255, 255, 0]
            }
            let offset = (y * width + x) * 4
            let opacity: UInt8 = alpha && x >= width / 2 ? 0 : 255
            bytes[offset] = opacity == 0 ? 0 : color[0]
            bytes[offset + 1] = opacity == 0 ? 0 : color[1]
            bytes[offset + 2] = opacity == 0 ? 0 : color[2]
            bytes[offset + 3] = opacity
        }
    }
    let data = Data(bytes)
    guard let provider = CGDataProvider(data: data as CFData),
          let image = CGImage(width: width, height: height,
                              bitsPerComponent: 8, bitsPerPixel: 32,
                              bytesPerRow: width * 4, space: sRGB,
                              bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue),
                              provider: provider, decode: nil,
                              shouldInterpolate: false, intent: .defaultIntent)
    else { throw FixtureError.imageCreation }
    return image
}

func solid(_ rgb: [CGFloat], width: Int = 96, height: Int = 64,
           colorSpace: CGColorSpace = sRGB) throws -> CGImage {
    guard let context = CGContext(data: nil, width: width, height: height,
                                  bitsPerComponent: 8, bytesPerRow: width * 4,
                                  space: colorSpace,
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)
    else { throw FixtureError.imageCreation }
    context.setFillColor(CGColor(colorSpace: colorSpace, components: rgb + [1])!)
    context.fill(CGRect(x: 0, y: 0, width: width, height: height))
    guard let image = context.makeImage() else { throw FixtureError.imageCreation }
    return image
}

func write(_ image: CGImage, to url: URL, type: String, properties: [CFString: Any] = [:]) throws {
    guard let destination = CGImageDestinationCreateWithURL(url as CFURL, type as CFString, 1, nil)
    else { throw FixtureError.unsupportedDestination(type) }
    CGImageDestinationAddImage(destination, image, properties as CFDictionary)
    guard CGImageDestinationFinalize(destination) else { throw FixtureError.writeFailed(url.path) }
}

let basic = try pattern()
try write(basic, to: formats.appendingPathComponent("sample.png"), type: "public.png")
try write(basic, to: formats.appendingPathComponent("sample.jpg"), type: "public.jpeg",
          properties: [kCGImageDestinationLossyCompressionQuality: 0.95])
try write(basic, to: formats.appendingPathComponent("sample.heic"), type: "public.heic",
          properties: [kCGImageDestinationLossyCompressionQuality: 0.95])
// A profile-bearing HEIC and its ImageIO-to-sRGB reference detect accidental
// ICC loss or relabeling of unconverted Display P3 pixel values.
let p3URL = formats.appendingPathComponent("sample-p3.heic")
try write(solid([0.75, 0.25, 0.1], colorSpace: CGColorSpace(name: CGColorSpace.displayP3)!),
          to: p3URL, type: "public.heic",
          properties: [kCGImageDestinationLossyCompressionQuality: 0.95])
guard let p3Source = CGImageSourceCreateWithURL(p3URL as CFURL, nil),
      let p3Image = CGImageSourceCreateImageAtIndex(p3Source, 0, nil),
      let referenceContext = CGContext(data: nil, width: p3Image.width, height: p3Image.height,
                                       bitsPerComponent: 8, bytesPerRow: p3Image.width * 4,
                                       space: sRGB,
                                       bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)
else { throw FixtureError.imageCreation }
referenceContext.draw(p3Image, in: CGRect(x: 0, y: 0, width: p3Image.width, height: p3Image.height))
guard let referenceImage = referenceContext.makeImage() else { throw FixtureError.imageCreation }
try write(referenceImage, to: formats.appendingPathComponent("sample-p3-reference.png"), type: "public.png")
try write(basic, to: formats.appendingPathComponent("oriented.jpg"), type: "public.jpeg",
          properties: [kCGImageDestinationLossyCompressionQuality: 0.95,
                       kCGImagePropertyOrientation: 6])
try write(basic, to: formats.appendingPathComponent("oriented.heic"), type: "public.heic",
          properties: [kCGImageDestinationLossyCompressionQuality: 0.95,
                       kCGImagePropertyOrientation: 6])
try write(pattern(alpha: true), to: formats.appendingPathComponent("transparent.png"), type: "public.png")
try write(basic, to: formats.appendingPathComponent("中文 测试 图片.png"), type: "public.png")

// Each GIF frame is a solid color so the test can verify playback rather
// than treating a decodable first frame as full animation support.
let gifURL = formats.appendingPathComponent("animated.gif")
guard let gif = CGImageDestinationCreateWithURL(gifURL as CFURL, "com.compuserve.gif" as CFString, 3, nil)
else { throw FixtureError.unsupportedDestination("GIF") }
CGImageDestinationSetProperties(gif, [kCGImagePropertyGIFDictionary:
                                    [kCGImagePropertyGIFLoopCount: 0]] as CFDictionary)
for color: [CGFloat] in [[1, 0, 0], [0, 1, 0], [0, 0, 1]] {
    CGImageDestinationAddImage(gif, try solid(color),
                              [kCGImagePropertyGIFDictionary:
                               [kCGImagePropertyGIFDelayTime: 0.15,
                                kCGImagePropertyGIFUnclampedDelayTime: 0.15]] as CFDictionary)
}
guard CGImageDestinationFinalize(gif) else { throw FixtureError.writeFailed(gifURL.path) }

// 24 megapixels exercises the background decode and cache budget.
try autoreleasepool {
    let width = 6000, height = 4000
    guard let context = CGContext(data: nil, width: width, height: height,
                                  bitsPerComponent: 8, bytesPerRow: width * 4,
                                  space: sRGB,
                                  bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue),
          let gradient = CGGradient(colorsSpace: sRGB,
                                    colors: [CGColor(red: 1, green: 0, blue: 0, alpha: 1),
                                             CGColor(red: 0, green: 0, blue: 1, alpha: 1)] as CFArray,
                                    locations: [0, 1])
    else { throw FixtureError.imageCreation }
    context.drawLinearGradient(gradient,
                               start: CGPoint(x: 0, y: 0),
                               end: CGPoint(x: width, y: height), options: [])
    guard let image = context.makeImage() else { throw FixtureError.imageCreation }
    try write(image, to: formats.appendingPathComponent("large.jpg"), type: "public.jpeg",
              properties: [kCGImageDestinationLossyCompressionQuality: 0.9])
}

for (filename, color): (String, [CGFloat]) in [("1.png", [1, 0, 0]),
                                             ("2.png", [0, 1, 0]),
                                             ("10.png", [0, 0, 1])] {
    try write(solid(color), to: navigation.appendingPathComponent(filename), type: "public.png")
}
try Data("This intentionally is not a JPEG image.\n".utf8).write(to: formats.appendingPathComponent("broken.jpg"))

for folder in [formats, navigation] {
    for url in try FileManager.default.contentsOfDirectory(at: folder, includingPropertiesForKeys: nil)
        .filter({ !$0.lastPathComponent.hasPrefix(".") }).sorted(by: { $0.lastPathComponent < $1.lastPathComponent }) {
        guard let source = CGImageSourceCreateWithURL(url as CFURL, nil) else {
            print("\(url.lastPathComponent): intentionally undecodable")
            continue
        }
        let properties = CGImageSourceCopyPropertiesAtIndex(source, 0, nil) as? [CFString: Any] ?? [:]
        let width = properties[kCGImagePropertyPixelWidth] ?? "?"
        let height = properties[kCGImagePropertyPixelHeight] ?? "?"
        let orientation = properties[kCGImagePropertyOrientation] ?? 1
        print("\(url.lastPathComponent): \(width)x\(height), frames=\(CGImageSourceGetCount(source)), EXIF=\(orientation)")
    }
}
