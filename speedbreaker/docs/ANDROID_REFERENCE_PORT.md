# Vivo reference port, October 21, 2026

Baseline: SpeedBreaker android/vivo-x300-pro-usa at c1e7556. Reference:
PredatorWoah/NFSMW-Mobileplus origin/main bd7d1701927ceaf072067c310a05d0b9036aea9a,
embedded speedbreaker/ after its original handoff import 0dcebce. Code was
reviewed against this runtime rather than merging unrelated SDK backends.

## Android controls and session diagnostics

Ported Racing and Xbox layouts, launcher-accessible game-free editor, icon,
Downloads logging, diagnostic package suffix/name and explicit game-free label.
The existing native SDL/XInput snapshot path and software overlay are retained.
Both landscape directions remain requested. The new test package has its own
task affinity, avoiding affinity collisions with the game installation.

Existing users get a third SAVED layout backed by their untouched
`touch_layout_v1` file, using the exact old control IDs/default geometry and
normalized coordinates. No rename-based migration discards controls. Racing
and Xbox have separate preference files. The active choice persists; switching
layouts does not write positions. Editor SAVE still uses synchronous commit.
Save data, disc grants and signing identity are retained by in-place updates.

Downloads logs include the native session log, app-visible logcat, historical
exit evidence and direct ten-second samples: Android thermal status/headroom,
battery temperature/charge, process RSS, free RAM, display refresh and available
CPU frequencies. They do not measure GPU/CPU temperature directly. On this Vivo,
Java informational logcat messages were absent from the copied log despite the
copy worker running. Samples and thermal status events now write directly to
the session file. Device capture confirms these records, including a brief
SEVERE thermal event returning to moderate; battery was 37.9 C in that interval.
This is not a racing thermal peak or a sustained-performance result.

Session streams are capped at 16 MiB per file and 64 MiB total. Existing user
logs are not deleted. Shutdown runs off the UI thread with a 1.5-second process
exit deadline; logcat pipes cannot indefinitely delay Activity teardown.

The 60 Hz/lowest-multiple display request has a launcher switch for comparison.
Device samples still reported 90 Hz with the initial request. A follow-up applies
the request immediately to an already-valid SDL surface, on surface change and
resume, and sets preferredRefreshRate as well as mode ID. The follow-up still reports 90 Hz in the final device session; the request
is not effective under the current Android/vendor policy.
Do not claim a display pacing or FPS improvement merely because the API accepts it.
The reference broadening of Mali workarounds to untested families/drivers was
not adopted; the measured device/driver scope remains.

## Cheats UI and guarded USA hooks

CHEATS is at the top right of the in-game touch toolbar. Published title-screen
bonus sequences may be entered individually or from a saved selection. Sequence
effects are not yet verified here. Entry is manual at Press Start: never guessed
from a startup delay. Selections and startup hook settings use AtomicFile,
explicit sync and readback verification; Save & Restart runs only after success.
Pending injected input is cancelled and released on focus loss/pause/destroy.
Turning a selection off does not reverse unlocks/balances already saved by the game.

The reference's broad live memory scanner/writer is withheld: scanning about
2.48 GiB, finding <=8 matching integers and writing them all does not establish
that they are money, nor synchronize guest allocation or GPU ownership. No PC
addresses or raw PPC instruction patches are applied to compiled host functions.

Two optional startup choices (off unless selected) are implemented:

* Black Edition bonus cars/events: USA data flag 0x82A2CE04 -> 0x100, as documented
  by Xenia Canary game-patches, title 454107D9, tuhana. Own original USA image
  contains zero there. Check exact supported executable SHA-256 and accept only
  original zero/already-enabled 0x100. User now reports the reference option worked
  and requests it here. This enables bonus content in the same USA game, not a
  replacement executable. Final device startup confirms the persisted black_edition=1 choice and the
  guarded write of 0x100. Bonus-content availability remains based on the user
  report; a content-by-content unlock audit has not been performed.
* Experimental career payout cash top-up to 100,000,000. Static read-only trainer
  research identified a payout site; all ten original instructions from
  0x8236488C through the conversion/add/store match this USA image. Own generated
  code shows sub_82337760 returns a reward in f1; caller 0x82364890 truncates it,
  adds profile cash at r31+12 and performs the normal game store. A narrow strong
  override calls the original function, then adjusts only that caller's reward,
  after checking the live committed writable profile allocation. No arbitrary
  scan or external trainer execution. Finite/sign/overflow checks retain abnormal
  cases. Exact executable identity and instruction fingerprint are mandatory.
  This is not unlimited spending: it tops up on a career payout and remains
  device-unverified. Default-off startup was confirmed in the device log.

The supported executable SHA-256 is
`aebdf3c14d1e4e4ed2c130d94bc1a0e97bd6d5bf39bdfdef33c3909c9cc4ba4c`.
No code or data from generated PPC, trainers, saves or executable images is
committed; private analysis and source/binary provenance stay outside Git.

Unlimited nitro, SpeedBreaker, all cars and challenge golds remain unresolved.
A private heuristic narrowed global-byte/branch/subtract/store patterns to 32
candidates; examination rejects several as cameras, UI or non-global register
reuse. These are search leads, not established cheat addresses. Do not toggle
candidate bytes speculatively in a user's active profile.

## Reflection investigation

Competing causes: texture sign/gamma/exponent, render-target/resolve exponent or
gamma handling, cube content/orientation, guest shader arithmetic/constants and
post-process exposure. First bounded metadata capture contains 31 sampled states
with cube-using shaders: all signs unsigned and exponent zero. Extended capture
also observes render-target format 0 and exponent zero. Missing fetch conversion
is real source incompleteness but is not supported as the explanation for these
states. Actual car draw identification and equivalent scene comparison remain
needed before treating the hypothesis as universally falsified.

A bounded metadata-only probe records up to 192 distinct cube-using material
states and 96 resolve states, including fetch words and render-target registers.
No images, readbacks, extra GPU waits or shading changes. NFSMW_REFLECTION_LOG=0
rolls it back. Other hosts are off by default; Android bring-up enables it.
No arbitrary reflection/brightness multiplier or quality reduction was applied.
Next test: same affected car/scene, capture metadata/constants and compare the
cube/resolve path with the existing staging path in isolation.

## Validation and remaining checks

Host: 72 payout boundary and six edition guard cases pass; all ten cash guard
instructions match the own original USA image. Existing 67 CF cases, predicate
suite and BC decoder boundaries pass. Java and full optimized ARM64 builds pass;
all 56,146 guest exports, signature identity, ZIP and 16 KiB ELF alignment audited.

Device: isolated Java port reused byte-identical working native libraries and
installed over the game. Original save/preferences matched the backup then.
Later gameplay changed ERIC normally; a fresh backup was taken and ERIC was
byte-identical across the final diagnostic installation. The original layout
file still matches the first backup exactly (68 geometry fields). Game-free
editor and SAVED geometry were visually verified during a hands-off interval,
and the user's preceding Racing selection restored. No save was restored over
newer progress. Full native cleanup/probe reaches menu/material draws; optional
cash hook is confirmed off. Direct thermal records are confirmed.

Still required: failed-settings-write UI path, cancellation during title-code
entry, actual code unlocks, Black Edition disable/restart, payout behavior in a
backed-up test profile, sustained Racing/Xbox multitouch and both rotations,
background/resume, completed consecutive races and controlled reflection trial.
Preserved private APKs/symbols/source manifests permit rollback. Nothing pushed.


### Final bonus update installed

Clean code source 016a9df, signed full APK SHA-256
`4a90508ed209d64d5599deebf1b121a3a6babc86c3065ab5e0c240b7f1f80ff1`.
All 56,146 guest exports and certificate/ZIP/16 KiB alignment pass. After USB
reconnection it installed in place; fresh before/after archive hashes confirm
current ERIC files and the original layout file unchanged. Startup confirms
Black Edition bonus flag applied, career payout hook off. Saved startup choices
are black_edition=1 and career_cash_payout=0. Native logs and original control
geometry are preserved privately. The user is operating the phone, so UI checks
abort on foreground/dialog changes rather than sending touches into another screen.

Direct telemetry continues to work; the latest sampled session reports Android
SEVERE thermal status, battery 38.1 C, RSS about 3.4 GiB and display 90 Hz. These
are interval observations, not a controlled racing peak, leak diagnosis or direct
SoC temperature. Cool the device before equivalent-scene performance comparisons.
No forced refresh settings or quality changes were used to override vendor policy.

## NFSMW-Mobileplus publication integration

Integrated the primary working branch through 92300e6 onto NFSMW-Mobileplus main
a8bf72e (including its concurrent ADPF update). Preserved that repository's newer SETTINGS toolbar action, direct display
notes and Downloads hang/crash/bug-report export. SETTINGS is moved beside the
SAVED selector to avoid intercepting its touches. Report copies respect the session
budget, retain complete ZIPs and are marked delivered only after successful copy.

The imported cheats/renderer native files match the privately built/device-started
primary source. Newer upstream ADPF code remains separate and retains its existing
Power Hints switch; it has not been device-validated by this publication task. The merged 19 Java/SDL source files compile against Android API 35.
The game-free `.test` diagnostic APK builds successfully, has zero guest function
exports and passes 16 KiB ELF alignment checks. This validates the merged source
and probe packaging, not a new full-game device run of these additional UI/report
integrations. Payout/edition boundary tests pass. Existing CF, predicate and BC
regressions pass against the unchanged primary implementations.

Only source, tests and engineering findings are published. Original discs/XEX,
generated PPC, full game APKs, raw diagnostic archives, saves and signing material
stay local. The user separately requested a private local signing-key delivery;
no signing secret or archive is part of this repository.

Concurrent main update a8bf72e is preserved: Android performance hint sessions,
Power Hints setting, optional GPU clock reporting and a stronger surface refresh
request. The refresh integration keeps the launcher rollback switch, callback
registration once, immediate valid-surface request and focus/pause cancellation.
No new device performance or thermal improvement is claimed for ADPF. Historical
90 Hz readings above describe the installed primary build, not a measurement of
this newly combined publication tree.
