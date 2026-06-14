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

#import "renderer/mac/CandidateView.h"

#import <Foundation/Foundation.h>

#include <algorithm>
#include <set>

#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "client/client_interface.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_style.pb.h"
#include "renderer/candidate_window_util.h"
#include "renderer/mac/mac_view_util.h"
#include "renderer/renderer_style_handler.h"
#include "renderer/table_layout.h"

using mozc::client::SendCommandInterface;
using mozc::commands::CandidateWindow;
using mozc::commands::Output;
using mozc::commands::SessionCommand;
using mozc::renderer::RendererStyleHandler;
using mozc::renderer::ColumnType;
using mozc::renderer::kColumnShortcut;
using mozc::renderer::kColumnGap1;
using mozc::renderer::kColumnCandidate;
using mozc::renderer::kColumnDescription;
using mozc::renderer::kNumberOfColumns;
using mozc::renderer::TableLayout;
using mozc::renderer::mac::MacViewUtil;

// Those constants and most rendering logic is as same as Windows
// native candidate window.
// TODO(mukai): integrate and share the code among Win and Mac.

// Private method declarations.
@interface CandidateView ()
- (void)reloadStyle;
- (void)reloadLogoImage;
- (void)applyViewChrome;
- (CGFloat)cornerRadius;
- (NSColor *)panelBackgroundColor;

// Draw the |row|-th row.
- (void)drawRow:(int)row;

// Draw footer
- (void)drawFooter;

// Draw scroll bar
- (void)drawVScrollBar;
@end

@implementation CandidateView {
  NSImage *logoImage_;
  int columnMinimumWidth_;

  mozc::commands::CandidateWindow candidate_window_;
  mozc::renderer::TableLayout tableLayout_;
  mozc::renderer::RendererStyle style_;

  // The row which has focused background.
  int focusedRow_;

  // Cache of attributed strings which is allocated at updateLayout.
  NSArray *candidateStringsCache_;

  // |command_sender_| holds a callback for mouse clicks.
  mozc::client::SendCommandInterface *command_sender_;
}

#pragma mark initialization

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    logoImage_ = nil;
    RendererStyleHandler::GetRendererStyle(&style_);
    [self reloadStyle];
    focusedRow_ = -1;
    // default line width is specified as 1.0 *pt*, but we want to draw
    // it as 1.0 px.
    [NSBezierPath setDefaultLineWidth:1.0];
    [NSBezierPath setDefaultLineJoinStyle:NSLineJoinStyleMiter];
  }
  return self;
}

- (CGFloat)cornerRadius {
  if (style_.has_corner_radius()) {
    return static_cast<CGFloat>(style_.corner_radius());
  }
  return 10.0;
}

- (NSColor *)panelBackgroundColor {
  if (style_.has_window_background_color()) {
    return MacViewUtil::ToNSColor(style_.window_background_color());
  }
  return [NSColor whiteColor];
}

- (CGFloat)logoBackingScale {
  if (self.window.screen) {
    return self.window.screen.backingScaleFactor;
  }
  return [NSScreen mainScreen].backingScaleFactor;
}

// Wide footer logo bounds (same proportions as macOS toolbar: 120×24 at 14pt).
- (NSSize)logoDisplaySizeForNaturalSize:(NSSize)natural {
  const double font_scale = style_.candidate_style().font_size() / 14.0;
  const CGFloat max_width = 120.0 * font_scale;
  const CGFloat max_height = 24.0 * font_scale;
  if (natural.width <= 0 || natural.height <= 0) {
    return NSMakeSize(max_width, max_height);
  }
  const CGFloat scale =
      MIN(max_width / natural.width, max_height / natural.height);
  return NSMakeSize(natural.width * scale, natural.height * scale);
}

- (void)reloadLogoImage {
  logoImage_ = nil;
  const CGFloat backing = [self logoBackingScale];

  if (style_.has_logo_svg_file_name()) {
    NSString *svg_path =
        [NSString stringWithUTF8String:style_.logo_svg_file_name().c_str()];
    NSImage *probe = MacViewUtil::LoadImageFromResources(svg_path);
    const NSSize logo_size = [self logoDisplaySizeForNaturalSize:probe.size];
    logoImage_ = MacViewUtil::LoadLogoImageFromResources(svg_path, logo_size, backing);
  }
  if (!logoImage_ && style_.has_logo_file_name()) {
    NSString *tiff_path =
        [NSString stringWithUTF8String:style_.logo_file_name().c_str()];
    NSImage *probe = MacViewUtil::LoadImageFromResources(tiff_path);
    const NSSize logo_size = [self logoDisplaySizeForNaturalSize:probe.size];
    logoImage_ = MacViewUtil::LoadLogoImageFromResources(tiff_path, logo_size, backing);
  }
}

- (void)applyViewChrome {
  // Rounded chrome is drawn in drawRect; keep the panel transparent outside the fill.
  if (self.window) {
    [self.window setBackgroundColor:NSColor.clearColor];
    self.window.opaque = NO;
  }
}

- (void)reloadStyle {
#ifdef __APPLE__
  mozc::config::ConfigHandler::Reload();
#endif  // __APPLE__
  RendererStyleHandler::GetRendererStyle(&style_);

  const NSAttributedString *minimumWidthString = MacViewUtil::ToNSAttributedString(
      style_.column_minimum_width_string(), style_.shortcut_style());
  columnMinimumWidth_ = [minimumWidthString size].width;

  [self reloadLogoImage];
  [self applyViewChrome];
}

- (void)viewDidChangeEffectiveAppearance {
  [super viewDidChangeEffectiveAppearance];
  if (candidate_window_.candidate_size() > 0) {
    [self updateLayout];
  } else {
    [self reloadStyle];
  }
  [self setNeedsDisplay:YES];
}

- (void)setCandidateWindow:(const CandidateWindow *)candidate_window {
  candidate_window_ = *candidate_window;
}

- (void)setSendCommandInterface:(SendCommandInterface *)command_sender {
  command_sender_ = command_sender;
}

// Override of NSView.
- (BOOL)isFlipped {
  return YES;
}

- (void)dealloc {
  candidateStringsCache_ = nil;
}

- (const TableLayout *)tableLayout {
  return &tableLayout_;
}

#pragma mark drawing

- (NSSize)updateLayout {
  [self reloadStyle];
  candidateStringsCache_ = nil;
  tableLayout_.Initialize(candidate_window_.candidate_size(), kNumberOfColumns);
  tableLayout_.SetWindowBorder(style_.window_border());

  // calculating focusedRow_
  focusedRow_ = mozc::renderer::FocusedDisplayRow(candidate_window_);

  // Reserve footer space.
  if (candidate_window_.has_footer()) {
    NSSize footerSize = NSZeroSize;

    const mozc::commands::Footer &footer = candidate_window_.footer();

    if (footer.has_label()) {
      const NSAttributedString *footerLabel =
          MacViewUtil::ToNSAttributedString(footer.label(), style_.footer_style());
      const NSSize footerLabelSize =
          MacViewUtil::applyTheme([footerLabel size], style_.footer_style());
      footerSize.width += footerLabelSize.width;
      footerSize.height = std::max(footerSize.height, footerLabelSize.height);
    }

    if (footer.has_sub_label()) {
      const NSAttributedString *footerSubLabel =
          MacViewUtil::ToNSAttributedString(footer.sub_label(), style_.footer_sub_label_style());
      const NSSize footerSubLabelSize =
          MacViewUtil::applyTheme([footerSubLabel size], style_.footer_sub_label_style());
      footerSize.width += footerSubLabelSize.width;
      footerSize.height = std::max(footerSize.height, footerSubLabelSize.height);
    }

    if (footer.logo_visible() && logoImage_) {
      const NSSize logoSize = [logoImage_ size];
      const double font_scale = style_.candidate_style().font_size() / 14.0;
      footerSize.width += logoSize.width + 4.0 * font_scale;
      footerSize.height = std::max(footerSize.height, logoSize.height);
    }

    if (footer.index_visible()) {
      const int focusedIndex = candidate_window_.focused_index();
      const int totalItems = candidate_window_.size();
      const NSString *footerIndex =
          [NSString stringWithFormat:@"%d/%d", focusedIndex + 1, totalItems];
      const NSAttributedString *footerAttributedIndex =
          MacViewUtil::ToNSAttributedString([footerIndex UTF8String], style_.footer_style());
      const NSSize footerIndexSize =
          MacViewUtil::applyTheme([footerAttributedIndex size], style_.footer_style());
      footerSize.width += footerIndexSize.width;
      footerSize.height = std::max(footerSize.height, footerIndexSize.height);
    }

    footerSize.height += style_.footer_border_colors_size();
    tableLayout_.EnsureFooterSize(MacViewUtil::ToSize(footerSize));
  }

  tableLayout_.SetRowRectPadding(style_.row_rect_padding());
  if (candidate_window_.candidate_size() < candidate_window_.size()) {
    tableLayout_.SetVScrollBar(style_.scrollbar_width());
  }

  const NSAttributedString *gap1 =
      MacViewUtil::ToNSAttributedString(" ", style_.gap1_style());
  tableLayout_.EnsureCellSize(kColumnGap1, MacViewUtil::ToSize([gap1 size]));

  NSMutableArray *newCache = [[NSMutableArray array] init];
  for (size_t i = 0; i < candidate_window_.candidate_size(); ++i) {
    const CandidateWindow::Candidate &candidate = candidate_window_.candidate(i);
    const NSAttributedString *shortcut = MacViewUtil::ToNSAttributedString(
        candidate.annotation().shortcut(), style_.shortcut_style());
    std::string value = candidate.value();
    if (candidate.annotation().has_prefix()) {
      value.insert(0, candidate.annotation().prefix());  // Prepend the prefix() to value.
    }
    if (candidate.annotation().has_suffix()) {
      value.append(candidate.annotation().suffix());
    }
    if (!value.empty()) {
      value.append("  ");
    }

    const NSAttributedString *candidateValue =
        MacViewUtil::ToNSAttributedString(value, style_.candidate_style());
    const NSAttributedString *description = MacViewUtil::ToNSAttributedString(
        candidate.annotation().description(), style_.description_style());
    if ([shortcut length] > 0) {
      const NSSize shortcutSize =
          MacViewUtil::applyTheme([shortcut size], style_.shortcut_style());
      tableLayout_.EnsureCellSize(kColumnShortcut, MacViewUtil::ToSize(shortcutSize));
    }
    if ([candidateValue length] > 0) {
      const NSSize valueSize =
          MacViewUtil::applyTheme([candidateValue size], style_.candidate_style());
      tableLayout_.EnsureCellSize(kColumnCandidate, MacViewUtil::ToSize(valueSize));
    }
    if ([description length] > 0) {
      const NSSize descriptionSize =
          MacViewUtil::applyTheme([description size], style_.description_style());
      tableLayout_.EnsureCellSize(kColumnDescription, MacViewUtil::ToSize(descriptionSize));
    }

    [newCache
        addObject:[NSArray arrayWithObjects:shortcut, gap1, candidateValue, description, nil]];
  }

  tableLayout_.EnsureColumnsWidth(kColumnCandidate, kColumnDescription, columnMinimumWidth_);

  candidateStringsCache_ = newCache;
  tableLayout_.FreezeLayout();
  // Re-load logo after |window| exists (backing scale + layout footer width).
  [self reloadLogoImage];
  return MacViewUtil::ToNSSize(tableLayout_.GetTotalSize());
}

- (void)drawRect:(NSRect)rect {
  if (!Category_IsValid(candidate_window_.category())) {
    LOG(WARNING) << "Unknown candidates category: " << candidate_window_.category();
    return;
  }

  const mozc::Size windowSize = tableLayout_.GetTotalSize();
  const NSRect bounds =
      MacViewUtil::ToNSRect(mozc::Rect(mozc::Point(0, 0), windowSize));
  const CGFloat radius = [self cornerRadius];

  [[NSGraphicsContext currentContext] saveGraphicsState];
  MacViewUtil::ClipToRoundedRect(bounds, radius);
  [[self panelBackgroundColor] set];
  MacViewUtil::FillRoundedRect(bounds, radius);

  for (int i = 0; i < candidate_window_.candidate_size(); ++i) {
    [self drawRow:i];
  }

  if (candidate_window_.candidate_size() < candidate_window_.size()) {
    [self drawVScrollBar];
  }
  [self drawFooter];
  [[NSGraphicsContext currentContext] restoreGraphicsState];

  [NSBezierPath setDefaultLineWidth:1.0];
  [MacViewUtil::ToNSColor(style_.border_color()) set];
  MacViewUtil::StrokeRoundedRect(bounds, radius);
}

#pragma mark drawing aux methods

- (void)drawRow:(int)row {
  if (row == focusedRow_) {
    // Draw focused background
    NSRect focusedRect = MacViewUtil::ToNSRect(tableLayout_.GetRowRect(focusedRow_));
    [MacViewUtil::ToNSColor(style_.focused_background_color()) set];
    [NSBezierPath fillRect:focusedRect];
    [NSBezierPath setDefaultLineWidth:1.0];
    [MacViewUtil::ToNSColor(style_.focused_border_color()) set];
    // Fix the border position.  Because a line should be drawn at the
    // middle point of the pixel, origin should be shifted by 0.5 unit
    // and the size should be shrinked by 1.0 unit.
    focusedRect.origin.x += 0.5;
    focusedRect.origin.y += 0.5;
    focusedRect.size.width -= 1.0;
    focusedRect.size.height -= 1.0;
    [NSBezierPath strokeRect:focusedRect];
  } else {
    // Draw normal background
    auto drawBackground = [&](ColumnType type,
                              const mozc::renderer::RendererStyle::TextStyle& text_style) {
      const mozc::Rect cellRect = tableLayout_.GetCellRect(row, type);
      if (cellRect.size.width > 0 && cellRect.size.height > 0 &&
          text_style.has_background_color()) {
        [MacViewUtil::ToNSColor(text_style.background_color()) set];
        [NSBezierPath fillRect:MacViewUtil::ToNSRect(cellRect)];
      }
    };
    drawBackground(kColumnShortcut, style_.shortcut_style());
    drawBackground(kColumnGap1, style_.gap1_style());
    drawBackground(kColumnCandidate, style_.candidate_style());
    drawBackground(kColumnDescription, style_.description_style());
  }

  NSArray<NSAttributedString *> *candidate = [candidateStringsCache_ objectAtIndex:row];

  auto drawText = [&](ColumnType type,
                      const mozc::renderer::RendererStyle::TextStyle& text_style) {
    const NSAttributedString *text = [candidate objectAtIndex:type];
    NSRect cellRect = MacViewUtil::ToNSRect(tableLayout_.GetCellRect(row, type));
    NSPoint position = cellRect.origin;
    position.x += text_style.left_padding();
    position.y += (cellRect.size.height - [text size].height) / 2;
    [text drawAtPoint:position];
  };

  drawText(kColumnShortcut, style_.shortcut_style());
  drawText(kColumnGap1, style_.gap1_style());
  drawText(kColumnCandidate, style_.candidate_style());
  drawText(kColumnDescription, style_.description_style());

  if (candidate_window_.candidate(row).has_information_id()) {
    NSRect rect = MacViewUtil::ToNSRect(tableLayout_.GetRowRect(row));
    [NSBezierPath setDefaultLineWidth:1.0];
    [MacViewUtil::ToNSColor(style_.border_color()) set];
    rect.origin.x += rect.size.width - 6.0;
    rect.size.width = 4.0;
    rect.origin.y += 2.0;
    rect.size.height -= 4.0;
    [NSBezierPath fillRect:rect];
  }
}

- (void)drawFooter {
  if (!candidate_window_.has_footer()) {
    return;
  }
  const mozc::commands::Footer &footer = candidate_window_.footer();
  NSRect footerRect = MacViewUtil::ToNSRect(tableLayout_.GetFooterRect());

  // Footer separator (1pt, same colour as toolbar border).
  if (style_.footer_border_colors_size() > 0) {
    [NSBezierPath setDefaultLineWidth:1.0];
    [MacViewUtil::ToNSColor(style_.footer_border_colors(0)) set];
    const NSPoint fromPoint = NSMakePoint(footerRect.origin.x, footerRect.origin.y + 0.5);
    const NSPoint toPoint =
        NSMakePoint(footerRect.origin.x + footerRect.size.width, footerRect.origin.y + 0.5);
    [NSBezierPath strokeLineFromPoint:fromPoint toPoint:toPoint];
    footerRect.origin.y += 1;
  }

  // Opaque white footer (top/bottom colors match in marinaMoji theme).
  [MacViewUtil::ToNSColor(style_.footer_bottom_color()) set];
  [NSBezierPath fillRect:footerRect];

  // Draw logo
  if (footer.logo_visible() && logoImage_) {
    const NSSize logoSize = logoImage_.size;
    const double font_scale = style_.candidate_style().font_size() / 14.0;
    NSPoint logoPoint = footerRect.origin;
    logoPoint.x += 4.0 * font_scale;
    logoPoint.y += (footerRect.size.height - logoSize.height) / 2;
    const NSRect logoRect = NSMakeRect(logoPoint.x, logoPoint.y, logoSize.width, logoSize.height);
    [logoImage_ drawInRect:logoRect
                    fromRect:NSZeroRect   // Draw the entire image
                  operation:NSCompositingOperationSourceOver
                    fraction:1.0  // Opacity
              respectFlipped:YES
                      hints:nil];
    footerRect.origin.x += logoSize.width;
    footerRect.size.width -= logoSize.width;
  }

  // Draw label
  if (footer.has_label()) {
    const NSAttributedString *footerLabel =
        MacViewUtil::ToNSAttributedString(footer.label(), style_.footer_style());
    footerRect.origin.x += style_.footer_style().left_padding();
    const NSSize labelSize = [footerLabel size];
    NSPoint labelPosition = footerRect.origin;
    labelPosition.y += (footerRect.size.height - labelSize.height) / 2;
    [footerLabel drawAtPoint:labelPosition];
  }

  // Draw sub_label
  if (footer.has_sub_label()) {
    const NSAttributedString *footerSubLabel =
        MacViewUtil::ToNSAttributedString(footer.sub_label(), style_.footer_sub_label_style());
    footerRect.origin.x += style_.footer_sub_label_style().left_padding();
    const NSSize subLabelSize = [footerSubLabel size];
    NSPoint subLabelPosition = footerRect.origin;
    subLabelPosition.y += (footerRect.size.height - subLabelSize.height) / 2;
    [footerSubLabel drawAtPoint:subLabelPosition];
  }

  // Draw footer index (e.g. "10/120")
  if (footer.index_visible()) {
    const std::string footerIndex =
        absl::StrFormat("%d/%d",
                        candidate_window_.focused_index() + 1,  // +1 to 1-origin from 0-origin.
                        candidate_window_.size());
    const NSAttributedString *footerAttributedIndex =
        MacViewUtil::ToNSAttributedString(footerIndex, style_.footer_style());
    const NSSize indexSize = [footerAttributedIndex size];
    NSPoint footerPosition = footerRect.origin;
    footerPosition.x = footerPosition.x + footerRect.size.width - indexSize.width -
                        style_.footer_style().right_padding();
    footerPosition.y += (footerRect.size.height - indexSize.height) / 2;
    [footerAttributedIndex drawAtPoint:footerPosition];
  }
}

- (void)drawVScrollBar {
  const mozc::Rect vscrollRect = tableLayout_.GetVScrollBarRect();
  if (vscrollRect.IsRectEmpty() || candidate_window_.candidate_size() <= 0) {
    return;
  }

  const int beginIndex = candidate_window_.candidate(0).index();
  const int candidatesTotal = candidate_window_.size();
  const int endIndex = candidate_window_.candidate(candidate_window_.candidate_size() - 1).index();

  const NSRect trackRect = MacViewUtil::ToNSRect(vscrollRect);
  const CGFloat trackRadius =
      std::min([self cornerRadius] / 2.0, static_cast<CGFloat>(style_.scrollbar_width()));
  [MacViewUtil::ToNSColor(style_.scrollbar_background_color()) set];
  MacViewUtil::FillRoundedRect(trackRect, trackRadius);

  const mozc::Rect indicatorRect =
      tableLayout_.GetVScrollIndicatorRect(beginIndex, endIndex, candidatesTotal);
  const NSRect thumbRect = MacViewUtil::ToNSRect(indicatorRect);
  const CGFloat thumbRadius =
      std::min(trackRadius, static_cast<CGFloat>(thumbRect.size.height / 2.0));
  [MacViewUtil::ToNSColor(style_.scrollbar_indicator_color()) set];
  MacViewUtil::FillRoundedRect(thumbRect, thumbRadius);
}

#pragma mark event handling callbacks

- (void)mouseDown:(NSEvent *)event {
  const mozc::Point localPos = MacViewUtil::ToPoint([self convertPoint:[event locationInWindow]
                                                              fromView:nil]);
  int clickedRow = -1;
  for (int i = 0; i < tableLayout_.number_of_rows(); ++i) {
    const mozc::Rect rowRect = tableLayout_.GetRowRect(i);
    if (rowRect.PtrInRect(localPos)) {
      clickedRow = i;
      break;
    }
  }

  if (clickedRow >= 0 && clickedRow != focusedRow_) {
    focusedRow_ = clickedRow;
    [self setNeedsDisplay:YES];
  }
}

- (void)mouseUp:(NSEvent *)event {
  const mozc::Point localPos = MacViewUtil::ToPoint([self convertPoint:[event locationInWindow]
                                                              fromView:nil]);
  if (command_sender_ == nullptr) {
    return;
  }
  if (candidate_window_.candidate_size() < tableLayout_.number_of_rows()) {
    return;
  }
  for (int i = 0; i < tableLayout_.number_of_rows(); ++i) {
    const mozc::Rect rowRect = tableLayout_.GetRowRect(i);
    if (rowRect.PtrInRect(localPos)) {
      SessionCommand command;
      command.set_type(SessionCommand::SELECT_CANDIDATE);
      command.set_id(candidate_window_.candidate(i).id());
      Output dummy_output;
      command_sender_->SendCommand(command, &dummy_output);
      break;
    }
  }
}

- (void)mouseDragged:(NSEvent *)event {
  [self mouseDown:event];
}
@end
