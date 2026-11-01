Shipgen Designer for macOS
==========================

Drag "Shipgen Designer" to Applications.

The app is not signed with an Apple Developer account, so macOS blocks it
the first time. Try to open it and close the warning, then go to System
Settings > Privacy & Security, scroll down and choose "Open Anyway". If
macOS says the app is damaged, run this in Terminal once:

  xattr -dr com.apple.quarantine "/Applications/Shipgen Designer.app"

The other programs are inside the app, in Contents/MacOS:

  shipgen-gui     the automatic generator's window
  shipgen-mesh    thin and tidy a 3D model
  shipgen         the command line:
                  "/Applications/Shipgen Designer.app/Contents/MacOS/shipgen" --help

Use the download for your kind of Mac: arm64 for Apple processors (M1 and
later), x86_64 for Intel. Needs macOS 15 or newer.

This is an alpha, built automatically and with little testing on a real Mac;
please report anything that goes wrong.

RELEASE_NOTES.txt says what is new. The programs include source from Shores
of Hazeron, copyright Hazeron Freehold LLC, under the licence in
LICENSE-Shores-of-Hazeron.txt.
