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

#include "gui/setup_wizard/setup_wizard_harbour.h"

#include <algorithm>

#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>

#include "absl/log/log.h"
#include "gui/setup_wizard/setup_wizard_theme.h"

namespace mozc {
namespace gui {
namespace {

// Source image is 2172×724. The printed wordmark sits at about x=1604–1936,
// y=133–245. Crops stay outside that rectangle.
constexpr QRect kHarbourCrop(0, 80, 1560, 520);
constexpr QRect kSeaCrop(1080, 300, 1060, 400);

QRect CropRect(HarbourView::Crop crop) {
  return crop == HarbourView::kHarbour ? kHarbourCrop : kSeaCrop;
}

}  // namespace

HarbourView::HarbourView(Crop crop, QWidget* parent)
    : QWidget(parent), crop_(crop) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setAttribute(Qt::WA_OpaquePaintEvent, false);
  source_ = QPixmap(QStringLiteral(":/harbour_splash.png"));
  if (source_.isNull()) {
    LOG(ERROR) << "Harbour splash could not be loaded";
  }
}

QSize HarbourView::sizeHint() const {
  return crop_ == kHarbour ? QSize(680, 200) : QSize(680, 150);
}

QSize HarbourView::minimumSizeHint() const { return QSize(240, 96); }

void HarbourView::paintEvent(QPaintEvent* /*event*/) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  if (source_.isNull() || width() < 2 || height() < 2) {
    return;
  }

  const qreal dpr = devicePixelRatioF();
  const QSize px(std::max(1, qRound(width() * dpr)),
                 std::max(1, qRound(height() * dpr)));
  if (scaled_px_ != px || scaled_dpr_ != dpr) {
    const QPixmap cropped = source_.copy(CropRect(crop_));
    scaled_ = cropped.scaled(px, Qt::KeepAspectRatioByExpanding,
                             Qt::SmoothTransformation);
    scaled_.setDevicePixelRatio(dpr);
    scaled_px_ = px;
    scaled_dpr_ = dpr;
  }

  const QSize logical(qRound(scaled_.width() / dpr),
                      qRound(scaled_.height() / dpr));
  const int x = (width() - logical.width()) / 2;
  const int y = (height() - logical.height()) / 2;
  painter.drawPixmap(x, y, scaled_);

  const QColor ivory = setup_theme::kIvory;
  const auto fade = [&](const QRect& area, const QPointF& from,
                        const QPointF& to) {
    QLinearGradient gradient(from, to);
    gradient.setColorAt(0.0, ivory);
    gradient.setColorAt(1.0, QColor(ivory.red(), ivory.green(), ivory.blue(), 0));
    painter.fillRect(area, gradient);
  };
  const int vertical = std::max(16, height() / 5);
  const int horizontal = std::max(20, width() / 12);
  fade(QRect(0, 0, width(), vertical), QPointF(0, 0), QPointF(0, vertical));
  fade(QRect(0, height() - vertical, width(), vertical),
       QPointF(0, height()), QPointF(0, height() - vertical));
  fade(QRect(0, 0, horizontal, height()), QPointF(0, 0),
       QPointF(horizontal, 0));
  fade(QRect(width() - horizontal, 0, horizontal, height()),
       QPointF(width(), 0), QPointF(width() - horizontal, 0));
}

}  // namespace gui
}  // namespace mozc
