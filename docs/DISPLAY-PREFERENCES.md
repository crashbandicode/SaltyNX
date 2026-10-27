# Remembering a monitor's refresh rate

Use this fork with the matching FPSLocker monitor-preference fork. Select the
desired refresh rate once in FPSLocker's **Display settings**, with automatic
**Docked Display Sync disabled**. A 60-FPS game can then keep its separate 120-Hz
display setting. The supported rate table and existing hardware validation remain
in force; the feature does not make a dock or monitor support additional modes.

## Stored settings

FPSLocker saves into `SaltySD/plugins/FPSLocker/ExtDisplays/<CRC32>.ini`, using the
existing CRC32 of the complete EDID structure. Each monitor has independent values:

```ini
[Common]
tvName=Example monitor
refreshRateAllowed={50,60,120}
refreshRateAllowed720p={50,60,90,120}
preferredRefreshRate=120
preferredRefreshRate720p=90
displayDisclaimerAcknowledged=True
```

The monitor is remembered automatically when a manual refresh-rate change succeeds
in FPSLocker. Reconnecting the same EDID selects that monitor's saved profile;
with Docked Display Sync off, SaltyNX restores its valid preferred rate once the
link is stable. The warning acceptance is saved separately when you choose Accept.

`preferredRefreshRate` applies only at 1920×1080; `preferredRefreshRate720p` applies
only at 1280×720. Neither is copied to other monitors/resolutions. Missing, zero,
malformed, disallowed, or currently unsupported preferences fall back to 60 Hz.
Delete the preferred key to return to default connection behavior. Keep the other
keys in the file. Choosing 60 Hz explicitly also replaces that resolution's saved
preference. Unknown keys, including FPSLocker's acknowledgment, are ignored here.

FPSLocker handles the acknowledgment: a new monitor still prompts; handheld uses a
separate `SaltySD/plugins/FPSLocker/display.ini`. Only explicit Accept persists it.
Removing the acknowledgment key or setting it to False restores that prompt.

## Reconnection policy

The sysmodule probes EDID/link identity at most four times a second during stable
docked operation. A new identity, resolution, trained link, observed disconnect,
or polling gap over two seconds starts a fresh connection epoch. It waits for one
second of stability before reading that monitor's INI and capability limits.
Refresh-only changes do not create an epoch. The steady-state loop does not reread
or rewrite the INI.

When Display Sync is off, a valid saved rate is applied once per epoch. Driver
failure gets at most three attempts, one second apart. A successful manual request
cancels any pending automatic attempt. Display Sync owns rate selection while
enabled. Link/readiness failures invalidate the previous monitor's settings and
reported refresh instead of carrying them to the next display.

The docked setter now bounds-checks the rate index and reports actual validation/
set failures. Shared refresh metadata is updated only for a successful selected
mode. Existing IPC command numbers and shared-memory layout are unchanged.

EDID changes reported by the OS create a different profile even if the physical
monitor is the same. A disconnect or suspend that is not observable through link,
EDID, resolution or poll timing cannot be distinguished from an unchanged display;
the hardware result and coverage scenarios below describe the tested setup.

## Build and host checks

Upstream base: `cf2adb7b8facdc34e3cbf9ec6ff7686f67575048`, the latest upstream
`master` verified on 2026-10-27. This includes release `2.1.3` at
`a3fee3fbe05c494d5246f9fe61066c429c09528a` (GL/logo rendering fix) and the subsequent
NVN stack-usage fix. Fork version: `2.1.3-monitor.1`.
Existing component licenses and attribution are retained; no new project-wide
license is assigned by this fork.

Portable production-policy test (GCC with C++20):

```powershell
g++ -std=c++20 -Wall -Wextra -Werror -pedantic '-fsanitize=address,undefined' -fno-omit-frame-pointer tests/display_preferences.cpp -o /tmp/salty-display-preferences-test
& /tmp/salty-display-preferences-test
```

Switch builds use devkitA64 r29.2/GCC 15.2.0, devkitARM r67.1, libnx 4.12.0 and
switch-tools 1.13.1 from `devkitpro/devkita64:20260219` (image
`sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282`).
Run `make -j1 all` with DEVKITPRO/DEVKITA64/DEVKITARM set and both toolchains plus
devkitPro tools on PATH. Serial building avoids concurrent shader packing by the
32-/64-bit recipes. Require nonempty core/bootstrap ELF+DBG outputs for both
architectures, the process NSP/ELF, and staged copies. Upstream uses
`--ignore-errors` for 32-bit recipes, so exit status alone is insufficient.

## Installation and acceptance

Back up the existing SaltyNX/FPSLocker files and display INIs. Exit games, install
the matching overlay and complete runtime update, then reboot the Switch. Update
`atmosphere/contents/0000000000534C56/exefs.nsp` and the four supplied
`SaltySD/saltynx_{core,core32,bootstrap,bootstrap32}.elf` runtime files together.
Preserve exceptions, flags, configuration and third-party plugins. In particular, retain full-title
exclusions with their leading `X`. Never replace settings with generic examples.

### Hardware result — 2026-10-27

The operator tested the installed **SaltyNX 2.1.3-monitor.1 / FPSLocker
3.5.0-monitor.1** pair and reported: **"seems to work great."** This records
successful operator testing on the Erista Switch 1 / Smash Ultimate 13.0.5 setup
using a 120-Hz external display and 60-FPS game target, with the existing
normal-speed refresh-clock plugin retained.

Tested source commits: SaltyNX `6431462d5213cc3f5b4e99937acf8c90576ed088` and
FPSLocker `cc9eb5a0c3ccc6caabd5d8763bff9e61d31338d1`. Before this report, host
sanitizer tests and complete Switch builds passed; installation verified all seven
runtime replacements and all 68 protected files, then safely unmounted the SD card.

The report confirms successful use on that setup; it did not itemize every
monitor/resolution/link scenario below or provide latency/frametime measurements.
These remain useful scenarios for extending coverage:

1. Accept the warning, choose 120 Hz manually, reopen the overlay and verify the
   acknowledgment and 1080p preference persisted without changing the FPS target.
2. Redock and verify 120 Hz returns; repeat sleep/resume and HDMI unplug/replug
   while the console stays docked.
3. Connect a different monitor: no inherited 120-Hz preference or acknowledgment.
4. Change 720p/1080p and test independent preferences; use a lower-bandwidth dock
   and verify unsupported values are rejected/fall back rather than forced.
5. Enable Display Sync and verify it retains ownership; decline/back out of a new
   warning and confirm it is still shown next time.
6. Verify normal-speed Smash at 120 Hz/60 FPS remains intact, then check native
   60-Hz operation. The separately accepted refresh-clock plugin is not changed.

Rollback: restore the backed-up overlay and complete SaltyNX runtime, then reboot. New INI keys are
optional; restoring software does not require deleting profiles or other mods.
