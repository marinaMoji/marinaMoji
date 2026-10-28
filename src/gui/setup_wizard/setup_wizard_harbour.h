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

#ifndef MOZC_GUI_SETUP_WIZARD_SETUP_WIZARD_HARBOUR_H_
#define MOZC_GUI_SETUP_WIZARD_SETUP_WIZARD_HARBOUR_H_

#include <QPixmap>
#include <QWidget>

class QPaintEvent;

namespace mozc {
namespace gui {

// Draws a crop of the harbour splash, faded into the ivory page so the
// picture has no rectangular frame. The source is scaled once per size.
class HarbourView : public QWidget {
 public:
  enum Crop {
    kHarbour,  // village, boats, and breakwater; wordmark excluded
    kOpenSea,  // open water below the printed wordmark
  };

  explicit HarbourView(Crop crop, QWidget* parent = nullptr);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  Crop crop_;
  QPixmap source_;
  QPixmap scaled_;
  QSize scaled_px_;
  qreal scaled_dpr_ = 0;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_SETUP_WIZARD_SETUP_WIZARD_HARBOUR_H_
