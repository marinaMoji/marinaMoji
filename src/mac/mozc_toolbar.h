// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// marinaMoji: macOS floating toolbar (mode indicator, shin/kyu, odoriji, dict,
// shortcuts).  Mirrors the Linux GTK toolbar API.

#ifndef MOZC_MAC_MOZC_TOOLBAR_H_
#define MOZC_MAC_MOZC_TOOLBAR_H_

#include "client/client_interface.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace mac {

// Shows the toolbar, creating it lazily on first call.  |client| is used for
// SendCommand/GetConfig/LaunchTool.  Call from activateServer:.
void MozcToolbarShow(client::ClientInterface *client,
                     commands::CompositionMode mode);

// Hides the toolbar.  Call from deactivateServer:.
void MozcToolbarHide();

// Registers the active IMK controller (id<ControllerCallback>) so toolbar
// actions that open the candidate window can route output through processOutput.
// Pass nullptr on deactivateServer:.
void MozcToolbarSetActiveController(void *controller);

// Updates toolbar state from server output (composition mode, shin/kyu).
// Call after processOutput:.
void MozcToolbarUpdate(const commands::Output &output,
                       commands::CompositionMode mode);

// Persists toolbar visibility in toolbar.conf (default: visible).
bool MozcToolbarLoadVisiblePreference();
void MozcToolbarSaveVisiblePreference(bool visible);

}  // namespace mac
}  // namespace mozc

#endif  // MOZC_MAC_MOZC_TOOLBAR_H_
