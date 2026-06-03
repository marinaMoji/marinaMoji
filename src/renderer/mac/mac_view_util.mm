// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "renderer/mac/mac_view_util.h"

#include <algorithm>

#include "base/coordinates.h"
#include "base/mac/mac_util.h"
#include "protocol/renderer_style.pb.h"

namespace mozc {
namespace renderer {
namespace mac {

NSPoint MacViewUtil::ToNSPoint(const mozc::Point &point) { return NSMakePoint(point.x, point.y); }

mozc::Point MacViewUtil::ToPoint(const NSPoint &nspoint) {
  return mozc::Point(nspoint.x, nspoint.y);
}

NSSize MacViewUtil::ToNSSize(const mozc::Size &size) { return NSMakeSize(size.width, size.height); }

mozc::Size MacViewUtil::ToSize(const NSSize &nssize) {
  return mozc::Size(nssize.width, nssize.height);
}

NSRect MacViewUtil::ToNSRect(const mozc::Rect &rect) {
  return NSMakeRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
}

mozc::Rect MacViewUtil::ToRect(const NSRect &nsrect) {
  return mozc::Rect(ToPoint(nsrect.origin), ToSize(nsrect.size));
}

NSSize MacViewUtil::applyTheme(const NSSize &size, const RendererStyle::TextStyle &style) {
  return NSMakeSize(size.width + style.left_padding() + style.right_padding(), size.height);
}

NSColor *MacViewUtil::MacViewUtil::ToNSColor(
    const mozc::renderer::RendererStyle::RGBAColor &color) {
  return [NSColor colorWithCalibratedRed:color.r() / 255.0
                                   green:color.g() / 255.0
                                    blue:color.b() / 255.0
                                   alpha:color.a()];
}

NSAttributedString *MacViewUtil::ToNSAttributedString(const std::string &str,
                                                      const RendererStyle::TextStyle &style) {
  NSString *nsstr = [NSString stringWithUTF8String:str.c_str()];
  NSFont *font;
  if (style.has_font_name()) {
    font = [NSFont fontWithName:[NSString stringWithUTF8String:style.font_name().c_str()]
                           size:style.font_size()];
  } else {
    font = [NSFont messageFontOfSize:style.font_size()];
  }
  NSDictionary *attr;
  if (style.has_foreground_color()) {
    attr = [NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName,
                                                      ToNSColor(style.foreground_color()),
                                                      NSForegroundColorAttributeName, nil];
  } else {
    attr = [NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, nil];
  }
  return [[NSAttributedString alloc] initWithString:nsstr attributes:attr];
}

namespace {

NSBezierPath *RoundedRectPath(NSRect rect, CGFloat corner_radius) {
  const CGFloat radius = std::max(0.0, std::min(corner_radius, std::min(rect.size.width, rect.size.height) / 2.0));
  return [NSBezierPath bezierPathWithRoundedRect:rect xRadius:radius yRadius:radius];
}

NSRect InsetStrokeRect(NSRect rect) {
  rect.origin.x += 0.5;
  rect.origin.y += 0.5;
  rect.size.width -= 1.0;
  rect.size.height -= 1.0;
  return rect;
}

}  // namespace

void MacViewUtil::FillRoundedRect(NSRect rect, CGFloat corner_radius) {
  NSBezierPath *path = RoundedRectPath(rect, corner_radius);
  [path fill];
}

void MacViewUtil::StrokeRoundedRect(NSRect rect, CGFloat corner_radius) {
  NSBezierPath *path = RoundedRectPath(InsetStrokeRect(rect), corner_radius);
  [path stroke];
}

void MacViewUtil::ClipToRoundedRect(NSRect rect, CGFloat corner_radius) {
  NSBezierPath *path = RoundedRectPath(rect, corner_radius);
  [path addClip];
}

NSImage *MacViewUtil::LoadImageFromResources(NSString *relative_path) {
  if (relative_path.length == 0) {
    return nil;
  }
  const std::string resource_dir = mozc::MacUtil::GetServerDirectory();
  NSString *dir = [NSString stringWithUTF8String:resource_dir.c_str()];
  NSString *path = [dir stringByAppendingPathComponent:relative_path];
  if (![[NSFileManager defaultManager] fileExistsAtPath:path]) {
    return nil;
  }
  return [[NSImage alloc] initWithContentsOfFile:path];
}

namespace {

void SetPreservesVectorOnScaling(NSImage *image, BOOL preserve) {
  SEL sel = NSSelectorFromString(@"setPreservesVectorRepresentationOnScaling:");
  if (![image respondsToSelector:sel]) {
    return;
  }
  void (*setter)(id, SEL, BOOL) =
      reinterpret_cast<void (*)(id, SEL, BOOL)>([image methodForSelector:sel]);
  setter(image, sel, preserve);
}

// Draw |source| into a bitmap at Retina pixel dimensions, return points-sized NSImage.
NSImage *RasterizeLogoAtDisplaySize(NSImage *source, NSSize point_size, CGFloat backing_scale) {
  const CGFloat scale = std::max<CGFloat>(1.0, backing_scale);
  const NSInteger px =
      std::max<NSInteger>(1, static_cast<NSInteger>(point_size.width * scale + 0.5));
  const NSInteger py =
      std::max<NSInteger>(1, static_cast<NSInteger>(point_size.height * scale + 0.5));

  NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:NULL
                    pixelsWide:px
                    pixelsHigh:py
                 bitsPerSample:8
               samplesPerPixel:4
                      hasAlpha:YES
                      isPlanar:NO
                colorSpaceName:NSDeviceRGBColorSpace
                   bytesPerRow:0
                  bitsPerPixel:0];
  if (!bitmap) {
    source.size = point_size;
    return source;
  }

  // Hint the source size so SVG/PDF rasterize at target resolution (not ~16×16).
  const NSSize source_points =
      NSMakeSize(static_cast<CGFloat>(px) / scale, static_cast<CGFloat>(py) / scale);
  source.size = source_points;

  NSGraphicsContext *bitmap_ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:bitmap_ctx];
  [[NSGraphicsContext currentContext] setImageInterpolation:NSImageInterpolationHigh];
  [source drawInRect:NSMakeRect(0, 0, px, py)
            fromRect:NSZeroRect
           operation:NSCompositingOperationSourceOver
            fraction:1.0
      respectFlipped:YES
              hints:nil];
  [NSGraphicsContext restoreGraphicsState];

  NSImage *result = [[NSImage alloc] initWithSize:point_size];
  [result addRepresentation:bitmap];
  return result;
}

}  // namespace

NSImage *MacViewUtil::LoadLogoImageFromResources(NSString *relative_path, NSSize point_size,
                                                 CGFloat backing_scale) {
  NSImage *source = LoadImageFromResources(relative_path);
  if (!source || point_size.width <= 0 || point_size.height <= 0) {
    return nil;
  }
  SetPreservesVectorOnScaling(source, YES);
  return RasterizeLogoAtDisplaySize(source, point_size, backing_scale);
}

}  // namespace mozc::renderer::mac
}  // namespace mozc::renderer
}  // namespace mozc
