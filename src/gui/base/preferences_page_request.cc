// Copyright 2010-2021, Google Inc.
#include "gui/base/preferences_page_request.h"

#include <string>

#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/system_util.h"

namespace mozc {
namespace gui {
namespace {

constexpr char kRequestFileName[] = ".preferences_page_request";

}  // namespace

std::string PreferencesPageRequestPath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            kRequestFileName);
}

void RequestPreferencesPage(absl::string_view page_id) {
  if (page_id.empty()) {
    return;
  }
  const std::string path = PreferencesPageRequestPath();
  if (absl::Status s = FileUtil::SetContents(path, page_id); !s.ok()) {
    LOG(ERROR) << "RequestPreferencesPage failed: " << s;
  }
}

std::string ConsumePreferencesPageRequest() {
  const std::string path = PreferencesPageRequestPath();
  InputFileStream ifs(path);
  if (!ifs) {
    return std::string();
  }
  std::string contents;
  std::getline(ifs, contents);
  ifs.close();
  FileUtil::UnlinkIfExists(path).IgnoreError();
  // Trim trailing whitespace.
  while (!contents.empty() &&
         (contents.back() == '\n' || contents.back() == '\r' ||
          contents.back() == ' ' || contents.back() == '\t')) {
    contents.pop_back();
  }
  return contents;
}

}  // namespace gui
}  // namespace mozc
