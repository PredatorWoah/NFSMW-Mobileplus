# Update manifest

`latest.json` is what the launcher's update check reads
(`android/app/src/main/java/com/onenonlygit/speedbreakerone/UpdateCheck.java`), from
`raw.githubusercontent.com` on `main`. It holds only the package, the newest version, its
notes and where to get it.

The APK itself is never published here: it carries the game, recompiled from the owner's
own `default.xex`. It goes to the owner's **private** repository
`PredatorWoah/speedbreaker-builds`, as a release; the launcher's **Download** opens that
release page in the browser, where only a signed-in owner can reach it.

To publish a build:

1. Raise `versionCode` and `versionName` in `android/app/build.gradle` (and the version in
   `CMakeLists.txt`).
2. Build the game APK and sign it with the same key as the installed app (it then
   installs over it, keeping the saves).
3. Release it in `speedbreaker-builds` (pushing `apk/<name>.apk` there makes the release:
   its workflow attaches the file).
4. Here, set `versionCode`, `versionName` and `notes` in `latest.json` to that build.
