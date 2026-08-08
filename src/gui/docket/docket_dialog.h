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
//
// DocketDialog: a minimalist table review UI for the docket (see
// dictionary/docket_store.h) -- the queue of recently-committed,
// dictionary-unknown compounds. Each row can be triaged Yes (register into
// the user dictionary), No (skip for now; may resurface on a later
// commit), or Never (permanently ignored).
//
// Deliberately built without a Qt Designer .ui file: the layout is small
// enough (a table plus a refresh button) that hand-authoring the widget
// tree in C++ is simpler and less error-prone than an unreviewable .ui
// XML blob.

#ifndef MOZC_GUI_DOCKET_DOCKET_DIALOG_H_
#define MOZC_GUI_DOCKET_DOCKET_DIALOG_H_

#include <QMainWindow>
#include <QString>
#include <memory>
#include <string>

#include "data_manager/pos_list_provider.h"
#include "dictionary/docket_store.h"
#include "dictionary/user_dictionary_storage.h"

class QComboBox;
class QTableWidget;
class QTableWidgetItem;

namespace mozc {

namespace client {
class ClientInterface;
}  // namespace client

namespace gui {

class DocketDialog : public QMainWindow {
 public:
  DocketDialog();
  ~DocketDialog() override;

  bool IsAvailable() const { return is_available_; }

 private:
  // (Re-)populates the table from the on-disk docket.
  void Reload();

  // Adds one row for |entry|, wiring its Yes/No/Never buttons.
  void AddRow(const dictionary::DocketEntry& entry);

  // Row action handlers. |surface_item| identifies the row (its current
  // row() is looked up at click time, since rows shift as others are
  // removed); |original_surface| is the docket key, which stays fixed
  // even if the user edits the surface cell before clicking.
  void OnRegisterClicked(QTableWidgetItem* surface_item,
                        QTableWidgetItem* reading_item,
                        QComboBox* pos_box, std::string original_surface);
  void OnSkipClicked(QTableWidgetItem* surface_item,
                     std::string original_surface);
  void OnNeverClicked(QTableWidgetItem* surface_item,
                      std::string original_surface);

  // Registers (surface, reading, pos_name) into the user dictionary.
  // Returns false (and shows a warning dialog) on failure.
  bool RegisterEntry(const std::string& surface, const std::string& reading,
                     const std::string& pos_name);

  bool is_available_ = true;
  QTableWidget* table_ = nullptr;
  QString window_title_;
  std::unique_ptr<UserDictionaryStorage> storage_;
  std::unique_ptr<client::ClientInterface> client_;
  const PosListProvider pos_list_provider_;
  dictionary::DocketStore docket_store_;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_DOCKET_DOCKET_DIALOG_H_
