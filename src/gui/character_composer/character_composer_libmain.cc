// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include <QGuiApplication>
#include <QString>
#include <QtGui>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "base/system_util.h"
#include "composer_ids/composer_result.h"
#include "composer_ids/ids_table.h"
#include "gui/base/singleton_window_helper.h"
#include "gui/base/util.h"
#include "gui/character_composer/character_composer_dialog.h"

ABSL_FLAG(std::string, result_file, "",
          "character_composer: write the chosen character to this file "
          "instead of stdout (Windows; macOS uses a default path).");

// Runs the character composer. The chosen character is passed to the input
// method as UTF-8 and nothing else: on Linux it is printed to stdout followed
// by a newline; with --result_file (Windows) it is written to that file, and
// on macOS, where an app bundle cannot be given arguments, to
// composer_ids::DefaultResultFilePath(). Exit status 0 means a character was
// chosen, 1 that the dialog was closed without one (no file is written).
int RunCharacterComposer(int argc, char *argv[]) {
  Q_INIT_RESOURCE(qrc_character_composer);
  auto app = mozc::gui::GuiUtil::InitQt(argc, argv);

  std::string name = "character_composer.";
  name += mozc::SystemUtil::GetDesktopNameAsString();
  mozc::gui::SingletonWindowHelper window_helper(name);
  if (window_helper.FindPreviousWindow()) {
    window_helper.ActivatePreviousWindow();
    return -1;
  }

  mozc::gui::GuiUtil::InstallTranslator("character_composer");

  const std::string directory = mozc::composer_ids::IdsTable::DefaultDirectory();
  std::unique_ptr<mozc::composer_ids::IdsTable> table;
  if (auto loaded = mozc::composer_ids::IdsTable::LoadFromDirectory(directory);
      loaded.ok()) {
    table = std::make_unique<mozc::composer_ids::IdsTable>(*std::move(loaded));
  } else {
    LOG(ERROR) << "Cannot load the character composer data from " << directory
               << ": " << loaded.status();
  }

  mozc::gui::CharacterComposerDialog dialog(
      std::move(table), QString::fromStdString(directory));
  dialog.show();
  dialog.raise();
  dialog.activateWindow();
  app->exec();

  if (dialog.result().empty()) return 1;

  std::string result_file = absl::GetFlag(FLAGS_result_file);
#ifdef __APPLE__
  if (result_file.empty()) {
    result_file = mozc::composer_ids::DefaultResultFilePath();
  }
#endif  // __APPLE__
  if (!result_file.empty()) {
    if (absl::Status status = mozc::composer_ids::WriteComposerResult(
            result_file, dialog.result());
        !status.ok()) {
      LOG(ERROR) << "Cannot write the result to " << result_file << ": "
                 << status;
      return 2;
    }
    return 0;
  }
  std::fputs(dialog.result().c_str(), stdout);
  std::fputc('\n', stdout);
  std::fflush(stdout);
  return 0;
}
