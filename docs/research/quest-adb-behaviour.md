# How a Quest answers adb

What adb can and cannot do on a Meta Quest, as measured on a real headset. The installer library and the recipes depend on these results; [decision 0003](../decisions/0003-pushes-into-android-data.md) is built on them.

All checks were made on a Quest 3 with Android 14 (build 52641960034500520) on 2026-11-12, over Wi-Fi. They used an installed port (galaxyquest) and test files that were removed afterwards.

## Granting All files access

`appops set <package> MANAGE_EXTERNAL_STORAGE allow` succeeds over adb, and `appops get` then reads `allow`. Android checks this app op when an app asks whether it has All files access, so a recipe's `grant` step can replace the tap in the headset.

Confirmed in use: Urban Terror VR, installed and granted this way, went straight to its 1.4 GB data download without asking for file access, then played.

## Pushing files

Outside `Android/data`, `adb push` works as usual, including a whole folder. Files there go through Android's storage layer, which gives apps access.

Inside `/sdcard/Android/data/<package>/`, the rules differ. The app's own folders belong to the app (`drwxrws---`, owner the app's user). What adb creates belongs to the shell user (2000):

- A single file pushed into a folder that exists works, and the file is readable by everyone (`-rw-r--r--`).
- A folder that adb creates is `drwxrws---`, so the app cannot enter it.
- `adb push` of a whole folder fails (`failed to read copy response: EOF`), and so does a file push that must create more than one folder level (`secure_mkdirs failed: Operation not permitted`).
- `adb shell mkdir -p` creates nested folders, and `chmod` on them works.
- galaxyquest's own install script leaves its pushed folders `drwxrwxrwx`, and the game runs with them. descent-3's guide uses `chmod 2777` for the same reason.

The app must create its own `files` folder first, which is why recipes launch the app once before such a push.

## Packages and versions

- `pm list packages -3 --show-versioncode` lists the third-party packages with their versionCode, one per line: `package:com.bilko.urtquest versionCode:101`. The headset had 163.
- `dumpsys package <name>` holds, in the package's `Package [<name>]` section, `versionCode=`, `versionName=` and `lastUpdateTime=` four spaces in, and `firstInstallTime=` deeper, under `User 0:`. For a package that is not installed it prints `Unable to find package: <name>`.
- The headset's versionName often matches the release tag without its `v` (`1.0.1` for `v1.0.1`), but tags such as `vcop-v0.2.4`, `b003` or `n64` do not map to versions reliably. See [decision 0006](../decisions/0006-update-detection.md).

## A file of qgp's own

`adb push` of `/sdcard/qgp/installs.json` creates the `qgp` folder itself; the file lands as ordinary shared storage (`-rw-rw----`, owner 10080, group 1023, as for every file outside `Android/data`). Reinstalling an app with `install -r` left its data in `/sdcard` untouched. See [decision 0008](../decisions/0008-install-records-on-the-headset.md).

## Storage

`stat -f -c '%S %b %a' /sdcard` prints the block size, total blocks and blocks free for apps: `4096 117550775 15477877`, about 59 GB free of 448 GiB. `df` agrees, and shows that `/sdcard` and `Android/data` share one partition.

## Identity

Over Wi-Fi the adb serial is `IP:port`. The same headset appeared twice, on port 5555 and on its wireless-debugging port. `getprop ro.serialno` returned the same hardware serial on both. See [decision 0004](../decisions/0004-headset-identity.md).

## Keeping the screen on

Measured 2026-11-12 on a Quest 3 (Horizon OS, Android 14), first from the android-dev-settings app and then from adb.

- The proximity sensor puts the headset to sleep about 20 seconds after it is taken off. Neither `settings put global stay_on_while_plugged_in 7` nor a screen wake lock prevents this.
- The broadcast `am broadcast -a com.oculus.vrpowermanager.prox_close` makes the headset act as if it is worn. It needs no permission; from `adb shell` it answers `Broadcast completed: result=0`.
- `am broadcast -a com.oculus.vrpowermanager.automation_disable` hands control back to the sensor.
- Meta Quest Developer Hub sends the same broadcast. Its override lapses about 10 minutes after it stops renewing it, so a caller renews it well within that.
