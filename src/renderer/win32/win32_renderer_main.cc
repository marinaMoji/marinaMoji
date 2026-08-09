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

// Mozc renderer process for win32.

#include "base/win32/scoped_com.h"
#include "base/win32/winmain.h"
#include "renderer/init_mozc_renderer.h"
#include "renderer/win32/win32_server.h"
#include "win32/base/sticky_keys_util.h"

int main(int argc, char* argv[]) {
  mozc::renderer::InitMozcRenderer(argv[0], &argc, &argv);

  mozc::ScopedCOMInitializer com_initializer;

  // marinaMoji: this renderer process is the one long-lived, single-instance
  // Windows process in the marinaMoji stack (unlike the TIP DLL, which loads
  // into every focused application), so it is the natural place to hold the
  // "press Shift 5 times" StickyKeys popup off for as long as marinaMoji is
  // running, restoring the previous setting on exit. See
  // win32/base/sticky_keys_util.h for why our own Shift-tap shortcuts need
  // this, and what it deliberately does not touch.
  mozc::win32::StickyKeysUtil sticky_keys;
  sticky_keys.DisableHotkey();

  mozc::renderer::win32::Win32Server server;
  server.SetRendererInterface(&server);
  return server.StartServer();
}
