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

#include "gui/config_dialog/kaeriten_table_editor.h"

#include <QMenu>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <istream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_split.h"
#include "base/util.h"
#include "composer/kaeriten_table_util.h"
#include "gui/base/table_util.h"
#include "gui/base/util.h"
#include "gui/config_dialog/generic_table_editor.h"

namespace mozc {
namespace gui {
namespace {

constexpr int kMaxSuffixLength = 8;

bool IsValidSuffix(const std::string& suffix) {
  if (suffix.empty() || suffix.size() > kMaxSuffixLength) {
    return false;
  }
  for (const char c : suffix) {
    if (c == ';' || c == '\t' || c == '\n' || c == '\r') {
      return false;
    }
  }
  return true;
}

enum {
  NEW_INDEX = 0,
  REMOVE_INDEX = 1,
  IMPORT_FROM_FILE_INDEX = 2,
  EXPORT_TO_FILE_INDEX = 3,
  RESET_INDEX = 4,
  MENU_SIZE = 5,
};

}  // namespace

KaeritenTableEditorDialog::KaeritenTableEditorDialog(QWidget* parent)
    : GenericTableEditorDialog(parent, 2), actions_(MENU_SIZE) {
  actions_[NEW_INDEX] = mutable_edit_menu()->addAction(tr("New entry"));
  actions_[REMOVE_INDEX] =
      mutable_edit_menu()->addAction(tr("Remove selected entries"));
  mutable_edit_menu()->addSeparator();
  actions_[IMPORT_FROM_FILE_INDEX] =
      mutable_edit_menu()->addAction(tr("Import from file..."));
  actions_[EXPORT_TO_FILE_INDEX] =
      mutable_edit_menu()->addAction(tr("Export to file..."));
  mutable_edit_menu()->addSeparator();
  actions_[RESET_INDEX] =
      mutable_edit_menu()->addAction(tr("Reset to defaults"));

  setWindowTitle(tr("Kaeriten shortcuts"));
  GuiUtil::ReplaceWidgetLabels(this);
  dialog_title_ = GuiUtil::ReplaceString(tr("[ProductName] settings"));
  CHECK(mutable_table_widget());
  CHECK_EQ(mutable_table_widget()->columnCount(), 2);
  QStringList headers;
  headers << tr("Keys (after ;)") << tr("Inserts");
  mutable_table_widget()->setHorizontalHeaderLabels(headers);

  resize(360, 380);
  UpdateMenuStatus();
}

std::string KaeritenTableEditorDialog::GetDefaultKaeritenTable() {
  std::vector<composer::KaeritenRow> rows;
  composer::ParseKaeritenTsvString(composer::GetBundledKaeritenTable(), &rows);
  return composer::SerializeKaeritenTable(rows);
}

bool KaeritenTableEditorDialog::LoadFromStream(std::istream* is) {
  CHECK(is);
  std::string content((std::istreambuf_iterator<char>(*is)),
                      std::istreambuf_iterator<char>());
  std::vector<composer::KaeritenRow> rows;
  composer::ParseKaeritenTsvString(content, &rows);

  mutable_table_widget()->setRowCount(0);
  mutable_table_widget()->verticalHeader()->hide();

  int row = 0;
  for (const composer::KaeritenRow& kaeriten_row : rows) {
    mutable_table_widget()->insertRow(row);
    mutable_table_widget()->setItem(
        row, 0, new QTableWidgetItem(QString::fromStdString(kaeriten_row.suffix)));
    mutable_table_widget()->setItem(
        row, 1,
        new QTableWidgetItem(QString::fromStdString(kaeriten_row.result)));
    ++row;
    if (row >= max_entry_size()) {
      QMessageBox::warning(
          this, dialog_title_,
          tr("You can't have more than %1 entries").arg(max_entry_size()));
      break;
    }
  }

  UpdateMenuStatus();
  return true;
}

bool KaeritenTableEditorDialog::LoadDefaultKaeritenTable() {
  const std::string bundled = composer::GetBundledKaeritenTable();
  struct viewbuf : std::streambuf {
    explicit viewbuf(absl::string_view sv) {
      char* p = const_cast<char*>(sv.data());
      setg(p, p, p + sv.size());
    }
  };
  viewbuf buffer(bundled);
  std::istream is(&buffer);
  return LoadFromStream(&is);
}

bool KaeritenTableEditorDialog::Update() {
  if (mutable_table_widget()->rowCount() == 0) {
    QMessageBox::warning(this, dialog_title_,
                         tr("Kaeriten shortcut table is empty."));
    return false;
  }

  std::vector<composer::KaeritenRow> rows;
  std::set<std::string> seen_suffixes;
  for (int i = 0; i < mutable_table_widget()->rowCount(); ++i) {
    const std::string suffix =
        TableUtil::SafeGetItemText(mutable_table_widget(), i, 0).toStdString();
    const std::string result =
        TableUtil::SafeGetItemText(mutable_table_widget(), i, 1).toStdString();
    if (suffix.empty() && result.empty()) {
      continue;
    }
    if (!IsValidSuffix(suffix) || result.empty()) {
      QMessageBox::warning(
          this, dialog_title_,
          tr("Each row needs keys (after ;) and an inserted character. "
             "Keys must be unique and cannot contain ; or tab."));
      return false;
    }
    if (!seen_suffixes.insert(suffix).second) {
      QMessageBox::warning(this, dialog_title_,
                           tr("Duplicate keys after semicolon: %1")
                               .arg(QString::fromStdString(suffix)));
      return false;
    }
    rows.push_back({suffix, result});
  }

  if (rows.empty()) {
    QMessageBox::warning(this, dialog_title_,
                         tr("Kaeriten shortcut table is empty."));
    return false;
  }

  *mutable_table() = composer::SerializeKaeritenTable(rows);
  return true;
}

void KaeritenTableEditorDialog::UpdateMenuStatus() {
  const bool status = (mutable_table_widget()->rowCount() > 0);
  actions_[RESET_INDEX]->setEnabled(status);
  actions_[REMOVE_INDEX]->setEnabled(status);
  UpdateOKButton(status);
}

void KaeritenTableEditorDialog::OnEditMenuAction(QAction* action) {
  if (action == actions_[NEW_INDEX]) {
    AddNewItem();
  } else if (action == actions_[REMOVE_INDEX]) {
    DeleteSelectedItems();
  } else if (action == actions_[IMPORT_FROM_FILE_INDEX] ||
             action == actions_[RESET_INDEX]) {
    if (mutable_table_widget()->rowCount() > 0 &&
        QMessageBox::Ok !=
            QMessageBox::question(
                this, dialog_title_,
                tr("Do you want to overwrite the current kaeriten table?"),
                QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel)) {
      return;
    }

    if (action == actions_[IMPORT_FROM_FILE_INDEX]) {
      Import();
    } else if (action == actions_[RESET_INDEX]) {
      LoadDefaultKaeritenTable();
    }
  } else if (action == actions_[EXPORT_TO_FILE_INDEX]) {
    Export();
  }
}

bool KaeritenTableEditorDialog::Show(QWidget* parent,
                                     const std::string& current_kaeriten_table,
                                     std::string* new_kaeriten_table) {
  KaeritenTableEditorDialog window(parent);

  if (current_kaeriten_table.empty()) {
    window.LoadDefaultKaeritenTable();
  } else {
    window.LoadFromString(current_kaeriten_table);
  }

  const bool result = (QDialog::Accepted == window.exec());
  new_kaeriten_table->clear();

  if (result && window.table() != window.GetDefaultKaeritenTable()) {
    *new_kaeriten_table = window.table();
  }

  return result;
}

}  // namespace gui
}  // namespace mozc
