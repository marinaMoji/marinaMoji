// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "base/init_mozc.h"
#ifdef _WIN32
#include "base/win32/winmain.h"
#endif  // _WIN32

int RunCharacterComposer(int argc, char *argv[]);

int main(int argc, char *argv[]) {
  mozc::InitMozc(argv[0], &argc, &argv);
  return RunCharacterComposer(argc, argv);
}
