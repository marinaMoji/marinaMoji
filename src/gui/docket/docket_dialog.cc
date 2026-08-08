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

#include "gui/docket/docket_dialog.h"

#include <QAbstractItemView>
#include <QAction>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QWidget>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "client/client.h"
#include "data_manager/pos_list_provider.h"
#include "dictionary/docket_store.h"
#include "dictionary/user_dictionary_util.h"
#include "dictionary/user_pos.h"
#include "gui/base/util.h"
#include "protocol/user_dictionary_storage.pb.h"

namespace mozc {
namespace gui {
namespace {

using mozc::user_dictionary::UserDictionary;

QString QUtf8(absl::string_view str) {
  return QString::fromUtf8(str.data(), static_cast<int>(str.size()));
}

constexpr int kSurfaceColumn = 0;
constexpr int kReadingColumn = 1;
constexpr int kPosColumn = 2;
constexpr int kActionsColumn = 3;

}  // namespace

DocketDialog::DocketDialog()
    : window_title_(GuiUtil::ProductName()),
      storage_(std::make_unique<UserDictionaryStorage>()),
      client_(client::ClientFactory::NewClient()) {
  setWindowTitle(window_title_);

  if (!storage_->Load().ok()) {
    LOG(WARNING) << "UserDictionaryStorage::Load() failed";
  }
  if (!storage_->Lock()) {
    QMessageBox::information(
        this, window_title_,
        tr("Close dictionary tool before using the docket dialog."));
    is_available_ = false;
    return;
  }
  if (!storage_->Exists().ok() || storage_->dictionaries_size() == 0) {
    if (!storage_->CreateDictionary(tr("User Dictionary 1").toStdString())
             .ok()) {
      LOG(ERROR) << "Failed to create a new dictionary.";
      is_available_ = false;
      return;
    }
  }

  table_ = new QTableWidget(this);
  table_->setColumnCount(4);
  QStringList header_labels;
  header_labels << tr("Surface") << tr("Reading") << tr("POS")
                << tr("Action");
  table_->setHorizontalHeaderLabels(header_labels);
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->verticalHeader()->hide();
  table_->setAlternatingRowColors(true);
  table_->setEditTriggers(QAbstractItemView::DoubleClicked |
                          QAbstractItemView::EditKeyPressed);
  setCentralWidget(table_);

  QToolBar* toolbar = addToolBar(tr("Docket"));
  QAction* refresh_action = toolbar->addAction(tr("Refresh"));
  connect(refresh_action, &QAction::triggered, this, &DocketDialog::Reload);

  resize(560, 360);

  Reload();
}

DocketDialog::~DocketDialog() { storage_->UnLock(); }

void DocketDialog::Reload() {
  table_->setRowCount(0);
  const absl::StatusOr<dictionary::DocketData> data =
      dictionary::ReadDocketDataUnlocked();
  if (!data.ok()) {
    LOG(WARNING) << "Failed to read docket: " << data.status();
    return;
  }
  for (const dictionary::DocketEntry& entry : data->pending) {
    AddRow(entry);
  }
}

void DocketDialog::AddRow(const dictionary::DocketEntry& entry) {
  const int row = table_->rowCount();
  table_->insertRow(row);

  auto* surface_item = new QTableWidgetItem(QUtf8(entry.surface));
  auto* reading_item = new QTableWidgetItem(QUtf8(entry.reading));
  table_->setItem(row, kSurfaceColumn, surface_item);
  table_->setItem(row, kReadingColumn, reading_item);

  auto* pos_box = new QComboBox(table_);
  const std::vector<std::string> pos_list = pos_list_provider_.GetPosList();
  int selected_index = pos_list_provider_.GetPosListDefaultIndex();
  for (size_t i = 0; i < pos_list.size(); ++i) {
    pos_box->addItem(QUtf8(pos_list[i]));
    if (pos_list[i] == entry.pos) {
      selected_index = static_cast<int>(i);
    }
  }
  pos_box->setCurrentIndex(selected_index);
  table_->setCellWidget(row, kPosColumn, pos_box);

  auto* actions = new QWidget(table_);
  auto* layout = new QHBoxLayout(actions);
  layout->setContentsMargins(2, 2, 2, 2);
  auto* yes_button = new QPushButton(tr("Yes"), actions);
  auto* no_button = new QPushButton(tr("No"), actions);
  auto* never_button = new QPushButton(tr("Never"), actions);
  layout->addWidget(yes_button);
  layout->addWidget(no_button);
  layout->addWidget(never_button);
  table_->setCellWidget(row, kActionsColumn, actions);

  const std::string original_surface = entry.surface;
  connect(yes_button, &QPushButton::clicked, this,
          [this, surface_item, reading_item, pos_box, original_surface]() {
            OnRegisterClicked(surface_item, reading_item, pos_box,
                              original_surface);
          });
  connect(no_button, &QPushButton::clicked, this,
          [this, surface_item, original_surface]() {
            OnSkipClicked(surface_item, original_surface);
          });
  connect(never_button, &QPushButton::clicked, this,
          [this, surface_item, original_surface]() {
            OnNeverClicked(surface_item, original_surface);
          });
}

void DocketDialog::OnRegisterClicked(QTableWidgetItem* surface_item,
                                     QTableWidgetItem* reading_item,
                                     QComboBox* pos_box,
                                     std::string original_surface) {
  const std::string surface = surface_item->text().toStdString();
  const std::string reading = reading_item->text().toStdString();
  const std::string pos_name = pos_box->currentText().toStdString();
  if (!RegisterEntry(surface, reading, pos_name)) {
    return;
  }
  const absl::Status status = docket_store_.RemovePending(original_surface);
  LOG_IF(WARNING, !status.ok())
      << "Failed to remove registered docket entry: " << status;
  table_->removeRow(table_->row(surface_item));
}

void DocketDialog::OnSkipClicked(QTableWidgetItem* surface_item,
                                 std::string original_surface) {
  const absl::Status status = docket_store_.RemovePending(original_surface);
  LOG_IF(WARNING, !status.ok())
      << "Failed to remove skipped docket entry: " << status;
  table_->removeRow(table_->row(surface_item));
}

void DocketDialog::OnNeverClicked(QTableWidgetItem* surface_item,
                                  std::string original_surface) {
  const absl::Status status = docket_store_.AddNever(original_surface);
  LOG_IF(WARNING, !status.ok())
      << "Failed to never-list docket entry: " << status;
  table_->removeRow(table_->row(surface_item));
}

bool DocketDialog::RegisterEntry(const std::string& surface,
                                 const std::string& reading,
                                 const std::string& pos_name) {
  if (surface.empty() || reading.empty()) {
    QMessageBox::warning(this, window_title_,
                         tr("Surface and reading must not be empty."));
    return false;
  }
  if (!user_dictionary::IsValidReading(reading)) {
    QMessageBox::warning(this, window_title_,
                         tr("Reading part contains invalid characters."));
    return false;
  }
  const UserDictionary::PosType pos = dictionary::UserPos::ToPosType(pos_name);
  if (!UserDictionary::PosType_IsValid(pos)) {
    QMessageBox::warning(this, window_title_, tr("POS is invalid."));
    return false;
  }
  if (storage_->dictionaries_size() == 0) {
    QMessageBox::warning(this, window_title_,
                         tr("No user dictionary is available."));
    return false;
  }

  UserDictionary* dic = storage_->GetProto().mutable_dictionaries(0);
  UserDictionary::Entry* entry = dic->add_entries();
  entry->set_key(reading);
  entry->set_value(surface);
  entry->set_pos(pos);

  if (absl::Status s = storage_->Save(); !s.ok()) {
    LOG(ERROR) << "Cannot save dictionary: " << s;
    QMessageBox::warning(this, window_title_,
                         tr("Failed to update user dictionary."));
    return false;
  }

  if (client_->PingServer()) {
#ifndef __APPLE__
    client_->CheckVersionOrRestartServer();
#endif  // __APPLE__
    client_->Reload();
  }
  return true;
}

}  // namespace gui
}  // namespace mozc
