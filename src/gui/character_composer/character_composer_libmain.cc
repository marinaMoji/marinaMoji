// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include <QGuiApplication>
#include <QSocketNotifier>
#include <QString>
#include <QtGui>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#ifndef _WIN32
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#endif  // !_WIN32

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

#ifndef _WIN32
namespace {
int g_wake_pipe[2] = {-1, -1};

// Async-signal-safe: just poke the pipe the Qt event loop is watching.
void OnWakeSignal(int) {
  const char byte = 1;
  if (g_wake_pipe[1] >= 0) (void)!write(g_wake_pipe[1], &byte, 1);
}

// SIGUSR1 means "a second composer was started: show yourself". Installed
// before the singleton check so the signal can never hit the default action.
void InstallWakeSignal() {
  if (pipe(g_wake_pipe) != 0) return;
  for (int fd : g_wake_pipe) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
  }
  struct sigaction action = {};
  action.sa_handler = OnWakeSignal;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_RESTART;
  sigaction(SIGUSR1, &action, nullptr);
}
}  // namespace
#endif  // !_WIN32

// Runs the character composer. A Unicode character is passed to the input
// method as UTF-8 (Linux stdout, --result_file on Windows, default path on
// macOS). An unencoded structure writes an SVG beside that path (and to the
// clipboard from the dialog) for office plugins. Exit status 0 means Insert
// was used; 1 means the dialog was closed without inserting.
int RunCharacterComposer(int argc, char *argv[]) {
  Q_INIT_RESOURCE(qrc_character_composer);
#if defined(__linux__) && !defined(__ANDROID__)
  // On a Wayland session Qt would pick its native Wayland backend, where a
  // window knows no global position: the IBus candidate window (drawn by the
  // xcb renderer) opened far left of the caret, and "stay on top" and raise()
  // are ignored, so the document hid the composer. Run through XWayland like
  // the renderer does. An explicit QT_QPA_PLATFORM still wins.
  if (std::getenv("DISPLAY") != nullptr && std::getenv("DISPLAY")[0] != '\0') {
    ::setenv("QT_QPA_PLATFORM", "xcb", /*overwrite=*/0);
  }
#endif  // __linux__ && !__ANDROID__
  auto app = mozc::gui::GuiUtil::InitQt(argc, argv);
#ifndef _WIN32
  InstallWakeSignal();
#endif  // !_WIN32

  std::string name = "character_composer.";
  name += mozc::SystemUtil::GetDesktopNameAsString();
  mozc::gui::SingletonWindowHelper window_helper(name);
  if (window_helper.FindPreviousWindow()) {
    window_helper.ActivatePreviousWindow();
#ifndef _WIN32
    // ActivatePreviousWindow only does something on Windows. Ask the running
    // composer to raise itself, or the toolbar button seems to do nothing
    // while the window is hidden behind the document.
    if (uint32_t pid = 0; window_helper.PreviousProcessId(&pid) && pid > 1) {
      kill(static_cast<pid_t>(pid), SIGUSR1);
    }
#endif  // !_WIN32
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

  // The drawing is written before the dialog closes: office plugins react to
  // their window regaining focus, which can happen before this process
  // returns from exec().
  auto write_svg = [](const std::string &svg) -> absl::Status {
    // Office plugins read the SVG from the user-profile directory, so it is
    // always written there, including on Windows where the TIP passes a temp
    // --result_file (the plugin never looks beside that).
    if (absl::Status status = mozc::composer_ids::WriteComposerSvgResult(
            mozc::composer_ids::DefaultSvgResultFilePath(), svg);
        !status.ok()) {
      return status;
    }
    if (!absl::GetFlag(FLAGS_result_file).empty()) {
      // Also keep a copy beside the text result file.
      if (absl::Status status = mozc::composer_ids::WriteComposerSvgResult(
              absl::GetFlag(FLAGS_result_file) + ".svg", svg);
          !status.ok()) {
        LOG(WARNING) << "Cannot write the SVG beside the result file: " << status;
      }
    }
    // The marinaMoji ONLYOFFICE plugin picks this up and inserts the drawing.
    const int64_t now_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    if (absl::Status status = mozc::composer_ids::WriteOnlyOfficeHandoff(
            mozc::composer_ids::OnlyOfficePluginDirectory(), svg, now_ms);
        !status.ok()) {
      LOG(WARNING) << "Cannot hand the SVG to the ONLYOFFICE plugin: " << status;
    }
    return absl::OkStatus();
  };
  bool svg_written = false;
  dialog.set_svg_inserted_callback([&](const std::string &svg) {
    if (absl::Status status = write_svg(svg); status.ok()) {
      svg_written = true;
    } else {
      LOG(ERROR) << "Cannot write the SVG: " << status;
    }
  });

#ifndef _WIN32
  QSocketNotifier wake_notifier(g_wake_pipe[0], QSocketNotifier::Read);
  QObject::connect(&wake_notifier, &QSocketNotifier::activated, [&]() {
    char drain[16];
    while (read(g_wake_pipe[0], drain, sizeof(drain)) > 0) {
    }
    dialog.showNormal();
    dialog.raise();
    dialog.activateWindow();
  });
#endif  // !_WIN32

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

  if (has_svg && !svg_written) {
    if (absl::Status status = write_svg(dialog.result_svg()); !status.ok()) {
      LOG(ERROR) << "Cannot write the SVG: " << status;
      return 2;
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
