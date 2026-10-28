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

#include "gui/setup_wizard/setup_wizard_style.h"

#include <algorithm>

#include <QPainter>
#include <QStyleOption>

#include "gui/setup_wizard/setup_wizard_theme.h"

namespace mozc {
namespace gui {
namespace {

constexpr int kControl = 18;

QRectF ControlBox(const QRect& rect) {
  const QRectF area = QRectF(rect).adjusted(1.0, 1.0, -1.0, -1.0);
  const qreal side = std::min(area.width(), area.height());
  return QRectF(area.center().x() - side / 2.0, area.center().y() - side / 2.0,
                side, side);
}

}  // namespace

SetupWizardStyle::SetupWizardStyle(QStyle* base) : QProxyStyle(base) {}

int SetupWizardStyle::pixelMetric(PixelMetric metric,
                                  const QStyleOption* option,
                                  const QWidget* widget) const {
  switch (metric) {
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
      return kControl;
    default:
      return QProxyStyle::pixelMetric(metric, option, widget);
  }
}

void SetupWizardStyle::drawPrimitive(PrimitiveElement element,
                                     const QStyleOption* option,
                                     QPainter* painter,
                                     const QWidget* widget) const {
  if (element == PE_FrameFocusRect) {
    return;
  }
  if (element != PE_IndicatorRadioButton && element != PE_IndicatorCheckBox) {
    QProxyStyle::drawPrimitive(element, option, painter, widget);
    return;
  }

  const bool on = option->state & State_On;
  const bool enabled = option->state & State_Enabled;
  const bool focus = option->state & State_HasFocus;
  const QColor ink = enabled ? setup_theme::kPrussian : setup_theme::kDisabled;
  const QColor ring =
      on ? ink : (enabled ? setup_theme::kMuted : setup_theme::kDisabled);

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  QPen pen(focus ? setup_theme::kPrussian : ring);
  pen.setWidthF(focus ? 2.0 : 1.35);
  painter->setPen(pen);
  painter->setBrush(Qt::NoBrush);

  const QRectF box = ControlBox(option->rect).adjusted(0.8, 0.8, -0.8, -0.8);
  if (element == PE_IndicatorRadioButton) {
    painter->drawEllipse(box);
    if (on) {
      painter->setPen(Qt::NoPen);
      painter->setBrush(ink);
      const qreal dot = box.width() * 0.46;
      painter->drawEllipse(QRectF(box.center().x() - dot / 2.0,
                                  box.center().y() - dot / 2.0, dot, dot));
    }
  } else {
    painter->drawRoundedRect(box, 3.0, 3.0);
    if (on) {
      QPen check(ink);
      check.setWidthF(1.7);
      check.setCapStyle(Qt::RoundCap);
      check.setJoinStyle(Qt::RoundJoin);
      painter->setPen(check);
      const QPointF a(box.left() + box.width() * 0.22,
                      box.top() + box.height() * 0.52);
      const QPointF b(box.left() + box.width() * 0.42,
                      box.top() + box.height() * 0.72);
      const QPointF c(box.left() + box.width() * 0.78,
                      box.top() + box.height() * 0.30);
      painter->drawLine(a, b);
      painter->drawLine(b, c);
    }
  }
  painter->restore();
}

}  // namespace gui
}  // namespace mozc
