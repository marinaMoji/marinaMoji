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

// Re-show the toolbar after the symbols palette closes (deactivateServer may hide it).
void MozcToolbarReshowAfterPaletteClose();
bool MozcToolbarNeedsReshowAfterPaletteClose();

// Registers the active IMK controller (id<ControllerCallback>) so toolbar
// actions that open the candidate window can route output through processOutput.
void MozcToolbarSetActiveController(void *controller);

// Clears the active controller and hides the toolbar only when |controller| is
// still the active one (avoids palette / IME-bundle focus churn hiding the bar).
void MozcToolbarClearActiveControllerIfMatches(void *controller);

// Updates toolbar state from server output (composition mode, shin/kyu).
// Call after processOutput:.
void MozcToolbarUpdate(const commands::Output &output,
                       commands::CompositionMode mode);

// Persists toolbar visibility in toolbar.conf (default: visible).
bool MozcToolbarLoadVisiblePreference();
void MozcToolbarSaveVisiblePreference(bool visible);

// Call immediately before launching a marinaMoji tool window (dictionary, word
// register, etc.).  macOS may send |-setValue:| DIRECT when focus leaves the
// host app; suppress that transient signal so composition mode is preserved.
void MozcImkNotifyToolLaunchStarting();
bool MozcImkShouldSuppressSetValueDirect();

}  // namespace mac
}  // namespace mozc

#endif  // MOZC_MAC_MOZC_TOOLBAR_H_
