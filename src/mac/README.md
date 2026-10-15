# mac

`mac/` is the directory for the macOS specific implementation.

It contains

*   Installer
*   Uninstaller
*   Interface classes for macOS IMF (i.e. the subclass of `IMKInputController`).

## DevConfirmPane

Installer module for the dev channel. (The ActivatePane installer plugin was removed: macOS only lets the user add a third-party input method, and the plugin only caused a "needs to run a program" warning.)

## English.lproj / Japanese.lproj

Localized strings used for the menu bar.

## Uninstaller / installer

Installer and Uninstaller.
