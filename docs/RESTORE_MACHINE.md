Step 1 — Stop marinaMoji from interfering (both accounts)
In each user account:

System Settings → Keyboard → Input Sources
Remove every marinaMoji row
Switch to ABC or U.S.
Optional but cleanest: temporarily remove the app so macOS stops auto-discovering it:

# only if marinaMoji.app is still installed
sudo mv "/Library/Input Methods/marinaMoji.app" ~/Desktop/marinaMoji.app.disabled
Then log out and back in (or restart).

You can reinstall marinaMoji later when you want to debug it again.

Step 2 — Restore normal keyboards (each account)
In System Settings → Keyboard → Input Sources → Edit → +, add back what you need:

English → U.S. or ABC (your default Latin keyboard)
English → Dvorak (if you use it)
Any other layouts you use (Chinese, etc.)
Set your default in the menu bar and confirm typing works in TextEdit.

Do not restore old hitoolbox.backup.plist files from Desktop wholesale — those backups may contain the corrupted marinaMoji/dpm split state from debugging.

Step 3 — Restore dpm (each account)
If you use your custom dpm layout:

ls ~/Library/Keyboard\ Layouts/
# find dpm.bundle.off.YYYYMMDD-HHMMSS
mv ~/Library/Keyboard\ Layouts/dpm.bundle.off.* ~/Library/Keyboard\ Layouts/dpm.bundle
Log out/in, then add dpm once in Input Sources.

Step 4 — Clean marinaMoji leftovers (each account, optional)
If you want that account fully clean of mozc registry:

cd ~/Code/marinaMozc/src
bash ./mac/scrub_marinamoji.sh
Skip this if marinaMoji is already removed and Input Sources look normal — Step 1–2 may be enough.

Step 5 — Verify each account
In Terminal (each user):

defaults read com.apple.HIToolbox AppleSelectedInputSources
defaults read com.apple.inputsources 2>/dev/null || echo "inputsources empty (OK if no third-party IMEs)"
ls "/Library/Input Methods/"
You want:

Selected = U.S./ABC or Dvorak (whatever you chose), not marinaMoji
No marinaMoji in Input Methods (if you disabled it)
Typing works in TextEdit and your usual apps
If an account is still broken after that
Try in order:

Log out / restart (IMK caches live in the running session)

Delete only that account’s input prefs (last resort for one account):

killall cfprefsd TextInputMenuAgent imklaunchagent 2>/dev/null || true
defaults delete com.apple.HIToolbox
defaults delete com.apple.inputsources
rm -f ~/Library/Preferences/com.apple.HIToolbox.plist
rm -f ~/Library/Preferences/com.apple.inputsources.plist
Log out/in, then re-add keyboards from Step 2.

New temporary macOS user — if a brand-new user gets normal keyboards, the broken account’s prefs are the problem (not the whole machine). You can migrate files later and leave the old account unused.