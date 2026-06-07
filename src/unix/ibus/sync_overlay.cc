#include "unix/ibus/sync_overlay.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#if defined(GDK_WINDOWING_WAYLAND)
#include <gdk/gdkwayland.h>
#endif

#include "sync/sync_status.h"

namespace mozc {
namespace ibus {
namespace {

GtkWidget* g_sync_window = nullptr;
GtkWidget* g_sync_label = nullptr;
bool g_sync_active = false;
guint g_status_timer_id = 0;
guint64 g_last_flash_ms = 0;

void MaybeForceX11OnGnomeWayland() {
  const char* session = g_getenv("XDG_SESSION_TYPE");
  if (!session || g_ascii_strcasecmp(session, "wayland") != 0) {
    return;
  }
  const char* desktop = g_getenv("XDG_CURRENT_DESKTOP");
  const bool gnome_like =
      desktop &&
      (g_strrstr(desktop, "GNOME") || g_strrstr(desktop, "ubuntu"));
  if (!gnome_like || g_getenv("GDK_BACKEND")) {
    return;
  }
  g_setenv("GDK_BACKEND", "x11", TRUE);
}

bool EnsureGtkReady() {
  MaybeForceX11OnGnomeWayland();
  static bool gtk_ready = gtk_init_check(nullptr, nullptr);
  return gtk_ready;
}

void OnOverlayRealize(GtkWidget* w, gpointer /*data*/) {
  GdkWindow* gw = gtk_widget_get_window(w);
  if (!gw) return;
  const char* session = g_getenv("XDG_SESSION_TYPE");
  if (session && g_ascii_strcasecmp(session, "x11") == 0) {
    gdk_window_set_override_redirect(gw, TRUE);
    return;
  }
#if defined(GDK_WINDOWING_WAYLAND)
  if (GDK_IS_WAYLAND_DISPLAY(gdk_window_get_display(gw))) {
    gdk_wayland_window_set_application_id(gw, "io.marinamoji.toolbar");
  }
#endif
}

void EnsureOverlay() {
  if (g_sync_window || !EnsureGtkReady()) {
    return;
  }

  g_sync_window = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_window_set_wmclass(GTK_WINDOW(g_sync_window), "marinamoji-toolbar",
                         "marinamoji-toolbar");
  gtk_window_set_decorated(GTK_WINDOW(g_sync_window), FALSE);
  gtk_window_set_keep_above(GTK_WINDOW(g_sync_window), TRUE);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(g_sync_window), TRUE);
  gtk_window_set_skip_pager_hint(GTK_WINDOW(g_sync_window), TRUE);
  gtk_window_set_type_hint(GTK_WINDOW(g_sync_window), GDK_WINDOW_TYPE_HINT_UTILITY);
  gtk_window_set_accept_focus(GTK_WINDOW(g_sync_window), FALSE);
  gtk_widget_set_app_paintable(g_sync_window, TRUE);
  g_signal_connect(g_sync_window, "realize", G_CALLBACK(OnOverlayRealize), nullptr);

  GtkCssProvider* provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(
      provider,
      ".sync-overlay { background-color: rgba(0, 0, 0, 0.55); "
      "border-radius: 8px; } "
      ".sync-label { color: white; font-weight: bold; font-size: 16px; }",
      -1, nullptr);
  GtkStyleContext* context = gtk_widget_get_style_context(g_sync_window);
  gtk_style_context_add_class(context, "sync-overlay");
  gtk_style_context_add_provider(
      context, GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);

  GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_set_border_width(GTK_CONTAINER(box), 16);
  gtk_container_add(GTK_CONTAINER(g_sync_window), box);

  g_sync_label = gtk_label_new("marinaMoji synchronising…");
  gtk_label_set_xalign(GTK_LABEL(g_sync_label), 0.5f);
  GtkStyleContext* label_ctx = gtk_widget_get_style_context(g_sync_label);
  gtk_style_context_add_class(label_ctx, "sync-label");
  gtk_box_pack_start(GTK_BOX(box), g_sync_label, TRUE, TRUE, 0);

  gtk_widget_realize(g_sync_window);
}

void CenterOverlay() {
  if (!g_sync_window) {
    return;
  }
  GdkDisplay* display = gtk_widget_get_display(g_sync_window);
  GdkMonitor* monitor = gdk_display_get_primary_monitor(display);
  if (!monitor) {
    const int n = gdk_display_get_n_monitors(display);
    if (n > 0) {
      monitor = gdk_display_get_monitor(display, 0);
    }
  }
  if (!monitor) {
    return;
  }
  GdkRectangle geom;
  gdk_monitor_get_geometry(monitor, &geom);
  GtkAllocation alloc;
  gtk_widget_get_allocation(g_sync_window, &alloc);
  const int x = geom.x + (geom.width - alloc.width) / 2;
  const int y = geom.y + (geom.height - alloc.height) / 2;
  gtk_window_move(GTK_WINDOW(g_sync_window), x, y);
}

void UpdateFromStatus() {
  const auto status_or = sync::ReadSyncStatus();
  const bool running =
      status_or.ok() && status_or->state == "running";
  g_sync_active = running;
  if (running) {
    EnsureOverlay();
    if (g_sync_label && status_or.ok() && !status_or->message.empty()) {
      gtk_label_set_text(GTK_LABEL(g_sync_label),
                         status_or->message.c_str());
    } else if (g_sync_label) {
      gtk_label_set_text(GTK_LABEL(g_sync_label),
                         "marinaMoji synchronising…");
    }
    if (g_sync_window) {
      gtk_widget_show_all(g_sync_window);
      CenterOverlay();
    }
  } else if (g_sync_window) {
    gtk_widget_hide(g_sync_window);
  }
}

gboolean PollStatusTimer(gpointer /*data*/) {
  UpdateFromStatus();
  return G_SOURCE_CONTINUE;
}

guint64 NowMs() {
  return static_cast<guint64>(g_get_monotonic_time() / 1000);
}

}  // namespace

void SyncOverlaySetVisible(bool visible) {
  if (!EnsureGtkReady()) {
    return;
  }
  g_sync_active = visible;
  if (visible) {
    EnsureOverlay();
    if (g_sync_window) {
      gtk_widget_show_all(g_sync_window);
      CenterOverlay();
    }
  } else if (g_sync_window) {
    gtk_widget_hide(g_sync_window);
  }
}

void SyncOverlayFlashBlockedInput() {
  if (!EnsureGtkReady()) {
    return;
  }
  GdkDisplay* display = gdk_display_get_default();
  if (display) {
    gdk_display_beep(display);
  }
  const guint64 now = NowMs();
  if (now - g_last_flash_ms < 1000) {
    return;
  }
  g_last_flash_ms = now;
  EnsureOverlay();
  if (g_sync_label) {
    const auto status_or = sync::ReadSyncStatus();
    if (status_or.ok() && !status_or->message.empty()) {
      gtk_label_set_text(GTK_LABEL(g_sync_label),
                         status_or->message.c_str());
    } else {
      gtk_label_set_text(GTK_LABEL(g_sync_label),
                         "marinaMoji synchronising…");
    }
  }
  if (g_sync_window) {
    gtk_widget_show_all(g_sync_window);
    CenterOverlay();
  }
}

bool SyncOverlayIsActive() { return g_sync_active; }

void SyncOverlayStartWatcher() {
  if (!EnsureGtkReady() || g_status_timer_id != 0) {
    return;
  }
  g_status_timer_id = g_timeout_add(250, PollStatusTimer, nullptr);
  UpdateFromStatus();
}

void SyncOverlayStopWatcher() {
  if (g_status_timer_id != 0) {
    g_source_remove(g_status_timer_id);
    g_status_timer_id = 0;
  }
  g_sync_active = false;
  if (g_sync_window) {
    gtk_widget_hide(g_sync_window);
  }
}

}  // namespace ibus
}  // namespace mozc
