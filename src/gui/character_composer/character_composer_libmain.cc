// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include <QGuiApplication>
#include <QString>
#include <QtGui>
#include <chrono>
#include <cstdint>
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

// Runs the character composer. A Unicode character is passed to the input
// method as UTF-8 (Linux stdout, --result_file on Windows, default path on
// macOS). An unencoded structure writes an SVG beside that path (and to the
// clipboard from the dialog) for office plugins. Exit status 0 means Insert
// was used; 1 means the dialog was closed without inserting.
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

  const bool has_char = !dialog.result().empty();
  const bool has_svg = !dialog.result_svg().empty();
  if (!has_char && !has_svg) return 1;

  std::string result_file = absl::GetFlag(FLAGS_result_file);
#ifdef __APPLE__
  if (result_file.empty()) {
    result_file = mozc::composer_ids::DefaultResultFilePath();
  }
#endif  // __APPLE__

  if (has_svg) {
    std::string svg_path = mozc::composer_ids::DefaultSvgResultFilePath();
    if (!absl::GetFlag(FLAGS_result_file).empty()) {
      // Windows temp path: keep the SVG beside the text result file.
      svg_path = absl::GetFlag(FLAGS_result_file) + ".svg";
    }
    if (absl::Status status = mozc::composer_ids::WriteComposerSvgResult(
            svg_path, dialog.result_svg());
        !status.ok()) {
      LOG(ERROR) << "Cannot write the SVG to " << svg_path << ": " << status;
      return 2;
    }
    // The marinaMoji ONLYOFFICE plugin picks this up and inserts the drawing.
    const int64_t now_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    if (absl::Status status = mozc::composer_ids::WriteOnlyOfficeHandoff(
            mozc::composer_ids::OnlyOfficePluginDirectory(), dialog.result_svg(),
            now_ms);
        !status.ok()) {
      LOG(WARNING) << "Cannot hand the SVG to the ONLYOFFICE plugin: " << status;
    }
  }

  if (!has_char) {
    // SVG-only: nothing for the IME to type; office plugins use clipboard/file.
    return 0;
  }

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
