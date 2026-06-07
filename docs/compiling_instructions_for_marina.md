# Compiling marinaMoji on Ubuntu

This guide is for building **marinaMoji** (this Mozc fork) from source on **Ubuntu**. It covers the packages you need, how to install Bazelisk, how to compile, and how to install the result.

---

## 1. Install required packages

Install the following **before** compiling. These provide the libraries and tools the build needs.

### 1.1 One-time setup (copy-paste)

Open a terminal and run:

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  clang \
  git \
  unzip zip \
  openjdk-17-jdk \
  libibus-1.0-dev \
  qt6-base-dev \
  libgtk-3-dev \
  librsvg2-dev \
  libopencc-dev
```

### 1.2 What each package is for

| Package | Purpose |
|--------|--------|
| `build-essential` | GCC, G++, and `make` — Bazel needs a C/C++ toolchain on the system. |
| `clang` | Clang compiler; the Linux build command below passes `CC=clang` / `CXX=clang++`. |
| `git` | Clone the repository (including submodules). |
| `unzip` `zip` | Unpack the built `mozc.zip` when installing. |
| `openjdk-17-jdk` | Java JDK required by Bazel. If the build complains about “javabase”, set `JAVA_HOME` (see Troubleshooting). |
| `libibus-1.0-dev` | IBus input method framework (glib, gobject, ibus headers/libs). |
| `qt6-base-dev` | Qt6 (core, gui, widgets) for the config dialog and candidate window. |
| `libgtk-3-dev` | GTK3 for the marinaMoji floating toolbar. |
| `librsvg2-dev` | Renders SVG icons in the toolbar. |
| `libopencc-dev` | **Optional but recommended.** Enables the Shin/Kyū (traditional vs modern kanji) conversion. Without it, the build still succeeds but that feature has no effect. |

You can omit `libopencc-dev` if you do not need traditional kanji support; the rest are required for a full build including the toolbar.

---

## 2. Install Bazelisk

The project uses **Bazelisk** to run the correct Bazel version automatically. Do **not** use the system `bazel` package; use Bazelisk.

### 2.1 Download and install the binary

1. Open: [Bazelisk releases on GitHub](https://github.com/bazelbuild/bazelisk/releases).
2. Download the **Linux amd64** binary (e.g. `bazelisk-linux-amd64` from the latest release).
3. Make it executable and put it in your `PATH`. For example, to install for your user only:

```bash
mkdir -p ~/bin
mv ~/Downloads/bazelisk-linux-amd64 ~/bin/bazelisk
chmod +x ~/bin/bazelisk
```

4. Ensure `~/bin` is in your `PATH`. Add this to `~/.bashrc` or `~/.profile` if needed:

```bash
export PATH="$HOME/bin:$PATH"
```

5. Reload your shell or run `source ~/.bashrc`, then check:

```bash
bazelisk version
```

You should see Bazelisk run and then download/use the Bazel version required by the repo (defined in `src/.bazeliskrc`).

### 2.2 Alternative: install via Go

If you have Go installed:

```bash
go install github.com/bazelbuild/bazelisk@latest
```

Ensure `$GOPATH/bin` or `$HOME/go/bin` is in your `PATH`, then use `bazelisk` as in the steps below.

---

## 3. Get the source and compile

### 3.1 Clone the repository (with submodules)

Use the **marinaMoji** fork URL (replace with the actual repo URL if different):

```bash
git clone https://github.com/marinaMoji/marinaMoji.git --recursive
cd marinaMoji/src
```

The `--recursive` option is required; the build depends on submodules.

### 3.2 Build the package

From the **`marinaMoji/src`** directory (not the repo root), run:

```bash
bazelisk build package --config oss_linux --config release_build \
  --repo_env=CC=clang --repo_env=CXX=clang++
```

The first run can take a long time (tens of minutes) while dependencies are downloaded and compiled. When it finishes successfully, the installable archive is:

- **`bazel-bin/unix/mozc.zip`**

---

## 4. Install and use marinaMoji

### 4.0 Remove a previous install (optional)

If you installed an older build with `sudo unzip … mozc.zip -d /` and/or the **Rime** marinaMoji from `marinaMoji_Repo`, remove them before installing again.

**Mozc fork only** (zip install):

```bash
sudo docs/uninstall_linux_marinamozc.sh
```

**Mozc fork + Rime marinaMoji** (recommended clean slate before a new Mozc build):

```bash
sudo docs/uninstall_linux_marinamozc.sh --all-marina --remove-config
```

**Rime marinaMoji only** (keep nothing from the old Mozc zip):

```bash
sudo docs/uninstall_linux_marinamozc.sh --rime-only --remove-config
```

This removes system files such as `marina.xml`, `/usr/libexec/ibus-marinamoji`, `/usr/share/marinamoji`, and `/usr/share/ibus-marinamoji`. It does **not** uninstall pacman packages (`librime`, `ibus`, etc.).

To also delete user settings (`~/.config/marinamozc`, `~/.config/ibus/marinaMoji`, Fontconfig fallback), add **`--remove-config`** (included in the commands above).

Then run `ibus write-cache` and `ibus restart`, and remove old entries in **Settings → Keyboard → Input Sources**.

### 4.1 Install the built files

From the **`marinaMoji/src`** directory:

```bash
sudo unzip -o bazel-bin/unix/mozc.zip -d /
```

This unpacks the engine, server, icons, sync daemon, and IBus component into system directories (e.g. `/usr/lib/marinamoji/`, `/usr/lib/ibus-marinamoji/`, `/usr/share/ibus/component/marinamoji.xml`).

### 4.2 Enable the sync daemon (optional but recommended)

Encrypted user-data sync uses `mozc_sync`. After installing the zip, enable the background scheduler as your normal user (not root):

```bash
chmod +x unix/install_sync_daemon.sh
./unix/install_sync_daemon.sh
```

This installs a systemd **user** unit (`marinamoji-sync.service`) that runs `/usr/lib/marinamoji/mozc_sync --daemon`. Configure sync in **Properties → Sync** before expecting automatic sync.

Verify:

```bash
test -x /usr/lib/marinamoji/mozc_sync && echo "Sync binary OK"
systemctl --user is-active marinamoji-sync.service
```

### 4.3 Verify installation

```bash
test -f /usr/share/ibus/component/marinamoji.xml && echo "Component OK"
test -x /usr/lib/ibus-marinamoji/ibus-engine-marinamoji && echo "Engine OK"
```

Both lines should print “OK”.

### 4.4 Reload IBus

So that IBus picks up the new component:

```bash
ibus write-cache
ibus restart
```

If you use a full desktop session, logging out and back in is an alternative.

### 4.5 Add marinaMoji as an input method

- **GNOME:** **Settings → Keyboard → Input Sources → Add (+)** → choose **Japanese** → select **marinaMoji** (or “marinaMoji (Japanese Input Method)”).
- **Other (IBus):** In your input method settings, add the engine named **marinaMoji** / **Japanese (marinaMoji)**.

If it does not appear, make sure `ibus-daemon` is running and check for errors when opening input settings or running `ibus engine`.

---

## 5. Candidate window (IBus vs Mozc)

By default, marinaMoji uses the **IBus** candidate window (equivalent to `MOZC_IBUS_CANDIDATE_WINDOW=ibus`). The Mozc candidate window offers more detail but can have positioning issues on some setups.

You can change this in **Properties → Misc → Candidate window** (dropdown: IBus / Mozc). The choice is stored in `~/.config/mozc/ibus_config.textproto` and applies after switching input method away and back, or restarting IBus.

To force IBus from the command line when starting the daemon:

```bash
ibus exit
MOZC_IBUS_CANDIDATE_WINDOW=ibus ibus-daemon -d
```

---

## 6. Optional: Wayland and gtk-layer-shell

On **Wayland**, the toolbar is positioned like marinaMoji (bottom-right) without extra packages. If you want to use **gtk-layer-shell** (e.g. on Sway/Hyprland) for layer-shell positioning:

1. Install:  
   `sudo apt install -y libgtk-layer-shell-dev`
2. Build with:  
   `bazelisk build --define=gtk_layer_shell=1 package --config oss_linux --config release_build`

If you do not add the define, the toolbar still works on X11 and on Wayland with the built-in positioning.

---

## 7. Troubleshooting

### “Could not find system javabase” / “must point to a JDK, not a JRE”

Install a JDK (e.g. `openjdk-17-jdk` as above), then point Bazel to it, for example:

```bash
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
```

Adjust the path to match your system (`/usr/lib/jvm/` and `ls` there to see the exact name). Then run the build again from `marinaMoji/src`.

### “required_aspect_providers, got element of type NoneType” (rules_swift)

Use **bazelisk** (not the system `bazel`) and run it from the **`src`** directory. The repo pins Bazel and dependency versions via `src/.bazeliskrc`; using system Bazel can cause this error.

### “Cannot find gcc or CC (clang)”

Bazel needs a C/C++ compiler on your system. Install **`build-essential`** (GCC/G++) and **`clang`** (used by the build command below), then run the build again.

### Build or dependency errors after system updates

Try cleaning Bazel’s cache and rebuilding:

```bash
cd marinaMoji/src
bazelisk clean --expunge
bazelisk build package --config oss_linux --config release_build
```

### marinaMoji does not appear in the input method list

- Run `ibus write-cache` and `ibus restart` again.
- Confirm the component file exists:  
  `ls -l /usr/share/ibus/component/marinamoji.xml`
- Check that the engine is executable:  
  `ls -l /usr/lib/ibus-marinamoji/ibus-engine-marinamoji`

---

## 8. Summary checklist

1. **Packages:** `build-essential`, `clang`, `git`, `unzip`, `zip`, `openjdk-17-jdk`, `libibus-1.0-dev`, `qt6-base-dev`, `libgtk-3-dev`, `librsvg2-dev`, and (optional) `libopencc-dev`.
2. **Bazelisk:** Download the Linux binary from GitHub, put it in `PATH` as `bazelisk`, and run `bazelisk version`.
3. **Clone:** `git clone ... marinaMoji --recursive` and `cd marinaMoji/src`.
4. **Build:** `bazelisk build package --config oss_linux --config release_build`.
5. **Install:** `sudo unzip -o bazel-bin/unix/mozc.zip -d /`.
6. **Reload:** `ibus write-cache` and `ibus restart`.
7. **Add input method:** In Settings, add **Japanese → marinaMoji**.

Config is stored under `~/.config/marinamoji/` and is separate from stock Mozc, so you can install marinaMoji alongside it for testing.
