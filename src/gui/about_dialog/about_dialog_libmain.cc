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

#include <QtGui>
#include <cstdlib>
#include <string>

#include "base/process.h"
#include "base/system_util.h"
#include "gui/base/preferences_page_request.h"
#include "gui/base/singleton_window_helper.h"
#include "gui/base/util.h"

#ifdef MARINAMOJI
#ifdef __APPLE__
#include "base/mac/mac_process.h"
#endif  // __APPLE__
#else   // !MARINAMOJI
#include "gui/about_dialog/about_dialog.h"
#endif  // MARINAMOJI

int RunAboutDialog(int argc, char *argv[]) {
#ifdef MARINAMOJI
  // About content now lives on Preferences → Application. Request that page
  // and open Preferences (raising it if already running).
  auto app = mozc::gui::GuiUtil::InitQt(argc, argv);
  Q_UNUSED(app);

  mozc::gui::RequestPreferencesPage(
      mozc::gui::kPreferencesPageApplication);
  qputenv("MARINAMOJI_PREFERENCES_PAGE",
          mozc::gui::kPreferencesPageApplication);

#ifdef __APPLE__
  if (!mozc::MacProcess::LaunchMozcTool("config_dialog")) {
    return 1;
  }
#else   // !__APPLE__
  size_t pid = 0;
  if (!mozc::Process::SpawnProcess(mozc::SystemUtil::GetToolPath(),
                                   "--mode=config_dialog", &pid)) {
    return 1;
  }
#endif  // __APPLE__
  return 0;

#else   // !MARINAMOJI
  Q_INIT_RESOURCE(qrc_about_dialog);
  auto app = mozc::gui::GuiUtil::InitQt(argc, argv);

  std::string name = "about_dialog.";
  name += mozc::SystemUtil::GetDesktopNameAsString();
  mozc::gui::SingletonWindowHelper window_helper(name);

  if (window_helper.FindPreviousWindow()) {
    window_helper.ActivatePreviousWindow();
    return -1;
  }

  mozc::gui::GuiUtil::InstallTranslator("about_dialog");
  mozc::gui::GuiUtil::InstallTranslator("tr");
  mozc::gui::AboutDialog about_dialog;

  about_dialog.show();
  about_dialog.raise();
  return app->exec();
#endif  // MARINAMOJI
}
