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

#include "gui/setup_wizard/setup_wizard_mark.h"

#include <algorithm>

#include <QPaintEvent>
#include <QPainter>

#include "absl/log/log.h"

namespace mozc {
namespace gui {

MarkView::MarkView(const QString& resource, int logical_height, bool expand,
                   QWidget* parent)
    : QWidget(parent), logical_height_(logical_height), expand_(expand) {
  source_ = QPixmap(resource);
  if (source_.isNull()) {
    LOG(ERROR) << "Branding image could not be loaded";
  }
  // Avoid painting into a neighbour: Qt clips each widget to its rect, so a
  // height that is only a few pixels can eat italic descenders after scaling.
  setFixedHeight(logical_height_);
  setAttribute(Qt::WA_TransparentForMouseEvents, true);
  if (expand_) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  } else {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedSize(sizeHint());
  }
}

QSize MarkView::sizeHint() const {
  if (source_.isNull() || source_.height() < 1) {
    return QSize(logical_height_ * 4, logical_height_);
  }
  const int width = std::max(
      1, qRound(logical_height_ * (static_cast<double>(source_.width()) /
                                   source_.height())));
  return QSize(width, logical_height_);
}

void MarkView::paintEvent(QPaintEvent* /*event*/) {
  if (source_.isNull() || height() < 2 || width() < 2) {
    return;
  }
  const qreal dpr = devicePixelRatioF();
  // Leave a device-pixel of air so SmoothTransformation tips are not clipped
  // by the widget's paint rect.
  const int pad = std::max(1, qRound(dpr));
  const int target_h = std::max(1, qRound(height() * dpr) - 2 * pad);
  if (scaled_.isNull() || scaled_h_ != target_h) {
    const int target_w = std::max(
        1, qRound(target_h * (static_cast<double>(source_.width()) /
                              source_.height())));
    scaled_ = source_.scaled(target_w, target_h, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
    scaled_.setDevicePixelRatio(dpr);
    scaled_h_ = target_h;
  }
  const int logical_w = std::max(1, qRound(scaled_.width() / dpr));
  const int logical_h = std::max(1, qRound(scaled_.height() / dpr));
  const int x = expand_ ? std::max(0, (width() - logical_w) / 2) : 0;
  const int y = std::max(0, (height() - logical_h) / 2);
  QPainter painter(this);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter.drawPixmap(x, y, scaled_);
}

}  // namespace gui
}  // namespace mozc
