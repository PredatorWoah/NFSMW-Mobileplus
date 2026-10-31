# Changelog

What changed in each SpeedBreaker release, newest first. Each release's notes on GitHub start from
its entry here, and the game shows the entry once, the first time a new version starts (offline:
it's built into the game).

## v0.1.38 (2026-11-01)

- CHEATS > Sure-shot pink slip. After you beat a Blacklist rival, the first
  marker you flip is the rival's pink slip: the hidden pink slip is swapped
  under your pick just before the game reads the card, or, if none of the
  six is a pink slip, made the way the game makes one (the car "BL<rival>").
  Your second pick is a normal marker.
- CHEATS > Plenty of upgrade markers. At least two of every part marker
  (brakes, engine, nos, induction, chassis, tires, transmission, body kit,
  hood, spoiler, rims, roof scoop, hud, vinyls, decals, paint) are kept in
  your marker inventory, refilled every few seconds after you use one.
- Both from the game's own marker code: the marker store at 82A391B0 (63
  owned markers, six pending), the marker screen's cards and its flip
  message. Apply with Save & Restart. Logged as `[cheats]`.

## v0.1.37 (2026-11-01)

- CHEATS > 2x nitro power (your car only). The engine's nitro step
  (sub_82424988) returns the torque factor: exactly 1.0, or while nitro fires
  1.0 plus the car's nitro boost. With the cheat on, that boost is doubled for
  the car whose owner reports driver class 0, the human player, asked through
  the same getter the game itself uses in that function. Rivals, cops and
  traffic keep their nitro. Apply with Save & Restart. The first nitro on
  each car is logged (`[cheats] nitro on engine ...`).

## v0.1.36 (2026-11-01)

- Remove Game Yellow Filter, for real (Settings > Display, on by default on
  Android). The race captures found it: the game's final HDR pass (five
  textures, luminance weights in c0) takes a colour curve in c6-c9 whose
  red, green and blue are one tint scaled per row, 0.85, 0.75, 0.25, the
  same in all four captures. The low blue is the yellow. With the option on,
  each of those rows gets its blue set to the mean of its red and green, for
  that one shader only; the rest of the game's look (bloom, contrast,
  vignette) stays. Off is the original Xbox 360 picture. Leave Yellow Tint
  Removal at 0 with it.
- The SAVED touch layout is gone (its stored positions are deleted): the
  layout switch toggles RACING and XBOX.

## v0.1.35 (2026-11-01)

- Log Colour Filter (diagnostic) now catches the race. The 0.1.34 log only
  held the boot frames: the option was still saved as on, so nothing was
  switched on during play. The boot frames did show the game's HDR post
  passes (luminance weights, blur kernels, tone numbers) and a two-texture
  composite pass with constants 1 1 1 1 and 2 0 0 1, neutral at the menu.
  Now switching it on arms a capture (a saved "on" at launch doesn't), and
  while it is on, a frame is logged every 30 s of racing (heavy scenes only),
  four times; shaders with the same constants as one already logged in that
  frame are skipped.

## v0.1.34 (2026-11-01)

- The memory scanner is gone. The 0.1.33 log: 733 MB scanned (virtual and
  physical memory), no known tint numbers and no bloom pair in any of it, so
  the 360's filter isn't stored like the PC's, and the scan every 5 s cost
  frames (35-58 hitches a minute with it on, 17 with it off).
- The option is now "Log Colour Filter (diagnostic)": switched on, the next
  three frames write every full-screen draw's pixel shader constants to the
  log (`[colour-pass]`: shader, texture count, each non-zero constant). It
  changes nothing in the picture and costs nothing otherwise; it is how the
  filter's real numbers and the draw that applies them get found.
  Yellow Tint Removal (0-5) stays as the working fix.

## v0.1.33 (2026-11-01)

- Remove Game Yellow Filter looks in the physical memory window too. The
  0.1.32 log: three scans of 256-282 MB, nothing found, because only the
  virtual heaps were scanned and the game keeps much of its loaded data in
  the physical window (0xA0000000-0xBFFFFFFF). Also looks for the community's
  Xbox 360-like tint presets, and logs "candidate" triples (tint-shaped,
  beside bloom-like numbers such as 1.75) with the floats around them, so
  the layout shows even if the numbers differ from the PC's.

## v0.1.32 (2026-11-01)

- Remove Game Yellow Filter, second try. The 0.1.31 log showed no match at
  all (the GPU-side guess was wrong), so it now works on the game's data, as
  PC modders do: the game ships a debug print of its `VisualLookEffect`
  (desaturation, blackbloom, colorbloom, colortint = three floats), and the
  PC defaults for the tint are 0.88, 0.80, 0.44. The runtime looks for that
  exact triple (or the colorbloom pair 0.9, 1.75 followed by a tint-shaped
  triple) in committed guest memory every 5 s, keeps at most 64 spots, and
  raises the blue to a little over the mean of red and green (0.90 on the
  defaults; the community fix is 0.95). Each find is logged with the floats
  around it (`[game-filter]`), so if the 360's numbers differ the log shows
  the layout. Turning the option off puts the blue back.

## v0.1.31 (2026-11-01)

- Settings > Display > Remove Game Yellow Filter (experimental, off by
  default): turns the game's own colour filter off at its source instead of
  correcting the picture afterwards. The filter is three numbers from the
  game's data (red 0.88, green 0.80, blue 0.44 on the PC; blue below the
  others is the yellow). They reach the colour pass's pixel shader as a
  constant; in a full-screen draw that samples two or more textures, a
  constant of that shape gets its blue raised to the mean of red and green.
  Each shader and register it touches is logged once (`[game-filter]`), so if
  nothing changes, or something odd does, the log has the numbers. Use it
  with Yellow Tint Removal at 0.

## v0.1.30 (2026-10-31)

- Yellow Tint Removal now works: 0.1.29's curve was calibrated on a dark
  safe house and lifted blue by about 11 levels in the mid-tones, where a
  daylight road (red 133, green 127, blue 94) needs about 30. The bump is now
  a parabola on blue, largest in the mid-tones and zero at black and white
  (+0.12 at the top step), with red and green trimmed by 0.025: the road
  comes out 127/121/123 and the sky stays white. Scenes already neutral or
  bluish (deep shadows) go a little cooler; pick the step you like.

## v0.1.29 (2026-10-31)

- Anti-aliasing and sharpening are off by default on Android (about 1.5 ms of
  GPU a frame back, on a GPU that sat at 16 ms of 16.7 in the city). Coming
  from an earlier version with the old defaults (AA on, sharpening 0.3), they
  go off once; any other value you set is kept. The Quality Preset puts both
  back.
- Settings > Display > Yellow Tint Removal (0-5, 0 is the original look):
  the game's picture is graded olive-yellow, blue 1.4-1.6x under red and
  green in the shadows. This lifts the blue (a power curve, mostly in the
  shadows, black and white unchanged) in the same table as Brightness, so it
  costs nothing and menus and the HUD are not touched.

## v0.1.28 (2026-10-23)

- Cheaper draws, from a profile of the command processor on the phone
  (about a quarter of its time was `memcpy`). Each draw uploaded all 512
  float shader constants (9.7 KB) though its two shaders read about 20 each;
  it now uploads only those, about 1.9 KB. Races measured 6.6 MB of ring a
  frame against about 33, and 2.13 us a draw against 2.39. This should push
  the first dip below 60 later and make later ones shallower; it is not a
  cure for heat. (Found and measured by Codex; two races, a long free-roam
  soak still to do.) New log lines: `[constants]`.

## v0.1.27 (2026-10-22)

- 60 fps again, always: Frame Rate is 60 by default on Android, a Frame
  Rate of Auto from 0.1.24-0.1.26 goes back to 60 once, and the Cool and
  Quality presets set 60. Auto's 30 felt too choppy even at 120 Hz; the work
  now goes into making frames cheaper so 60 lasts. Auto is still in
  Settings > Display for anyone who wants it.

## v0.1.26 (2026-10-22)

- The prime core, really this time: 0.1.25 reserved cpu 0, 4 and 7 together
  (ARM numbers cores within each cluster, so all three read "core 0"), and
  the draw thread could land on a 2.7 GHz core. Now just cpu 7.
- Frame Rate Auto comes back to 60 on Android: after 3 minutes at 30 with
  the phone cool it tries 60 again (backing off if 60 still can't hold).
  It waited for the draw thread to look fast at 30, but Android lowers the
  clocks at 30, so it never did and Auto stayed at 30 for good.

## v0.1.25 (2026-10-21)

- The command processor gets the phone's fastest core to itself again. On a
  chip with one prime core (the vivo X300 Pro's 4.21 GHz cpu 7) the rule
  that reserves it gave up, so the game's draw thread ran on the 3.5 GHz
  cores, which heat caps first (19 ms a frame and 30 fps after 6 minutes)
  while the prime core idled. The log says where it went (`[cpu]`).

## v0.1.24 (2026-10-21)

- Long sessions no longer eat the phone's memory. The texture cache kept
  every texture the game ever streamed in: 44 minutes of driving reached
  6,950 textures in 3.8 GB (the app 6.5 GB). Now, over 1.5 GB, textures
  nothing drew for 30 s are let go, the oldest first; one that comes back
  loads again. The log says each time (`[texture-cache]`).
- Less stutter once the phone is hot: Frame Rate is Auto on Android (60,
  or a steady 30 while the phone is hot or can't hold 60), and a saved 60
  becomes Auto once with this update (set 60 again in Settings > Display if
  you prefer it). On a panel stuck at 90 Hz, 45-55 fps shows frames for one
  refresh or two at random, which looks like stutter even with the counter
  at 50-60; 30 shows every frame for exactly three. The Quality Preset uses
  Auto too.

## v0.1.23 (2026-10-21)

- Stall reports: when frames keep coming but nothing is read from the disc and
  no audio is decoded for 30 s (a load stopped behind its animated loading
  screen), the watchdog writes a hang report with every thread's stack,
  quietly. Bug reports also tell the real time since the last disc read.
- Save backups: each start copies the save folder to save-backups (the newest
  five, none when nothing changed); the launcher's Garage > Restore save puts
  one back.
- Frame Rate Auto works on Android: it reads the phone's thermal status and
  drops to 30 at severe heat.
- Settings > Graphics on Android: Cool Preset (anti-aliasing off, 2x
  filtering, no sharpening, Frame Rate Auto) and Quality Preset. Fullscreen,
  which does nothing on a phone, is hidden.
- The launcher says when the phone kept the screen at 90 Hz last time; the
  CHEATS menu is black and gold like the launcher.

## v0.1.22 (2026-10-21)

- Challenge Series: all completed now really completes them: each event the
  series screen lists gets the game's own completed bit in its record, and the
  game recounts. Locks ("beat challenge 3 or 4") and the count follow when the
  screen is opened again, and the game saves them like a win.
- When the panel refuses 60 Hz, the surface also votes for 120 Hz instead of
  60 fps content (the vivo X300 Pro took 120 and dropped back to 90).

## v0.1.21 (2026-10-21)

- Challenge Series: all completed now turns on: one of its code guards had a
  wrong word (the branch at 8256AAF0), which left it off on the USA game.
- Save Bug Report adds reports/threads-now.txt: every thread's stack when it
  was saved. A load that stops while its loading screen still animates never
  trips the hang watchdog, so this is how such a stall can be seen.
- scripts/android/check_guards.sh checks every code guard in runtime/game
  against default.xex.

## v0.1.20 (2026-10-21)

- Haptics: a light tick when a finger lands on a touch button (and when the
  thumb slides onto the other steering arrow), and the game's own rumble on the
  phone's vibration motor through the touch pad's SDL rumble, in 16 strengths.
  Both can be turned off in the Garage; Settings > Vibration still applies.
- The 60 Hz request is checked 2 s later. If the panel stayed at a rate that
  60 doesn't divide (the vivo X300 Pro's 90 Hz), the 120 Hz mode is asked
  instead: each frame then shows for exactly two refreshes. Each step and every
  panel rate change goes to the log as [display].

## v0.1.19 (2026-10-21)

- The game's cover art in HD, built into the owner's private APK from
  private/cover.jpg (git-ignored; prepare_disc_icon.sh): the app icon is its top
  square (the logo and the M3 GTR), the launcher shows that square in the gold
  frame and the whole cover behind everything, faded and darker toward the
  buttons. A home-screen shortcut already pinned takes the new icon.

## v0.1.18 (2026-10-21)

- The game APK's app icon is now the game's own dashboard icon, made from the
  player's default.xex at build time (scripts/android/prepare_disc_icon.sh) and
  never committed; builds without it (the CI test APK) keep the speedometer.

## v0.1.17 (2026-10-21)

- A game-like launcher, black and gold: the disc's own icon and title, a big
  RACE (RESUME while the game runs) button, RESTART, GARAGE and CONTROLS, and an
  update badge. The Garage holds the GPU driver, logs, 60 Hz and updates.
- The game saves its dashboard icon and title from your default.xex (XDBF) for
  the launcher; Garage > Add to home screen pins a shortcut with that icon that
  goes straight to the race. Nothing of the game ships in the app.
- Gamepad works in the launcher: d-pad to move, A to press, B to go back.
- App icon restyled in black and gold.

## v0.1.16 (2026-10-21)

- CHEATS: Unlimited money (cash topped back up to 100,000,000 each frame on
  the game's render thread), Unlock all cars (the game's unlock-all flag plus
  the car query answering "unlocked") and Challenge Series: all completed (the
  series screen and its reward check see every event completed; no event record
  is written). Each checks the USA game's code first.

## v0.1.15 (2026-10-21)

- CHEATS: Unlimited nitro and Unlimited SpeedBreaker. The USA game's own meter
  code reads two data flags (82A2CE71, 82A2D1C6); when set, an active meter
  keeps its charge. Written at startup after checking the code around both
  loads; apply with Save & Restart.

## v0.1.14 (2026-10-21)

- Power Hints follow the Frame Rate setting for their target instead of the
  measured swaps: loads and hitches made them aim for 30 fps while the game ran
  at 60, so Android slowed the CPU down exactly when frames were late.
- Includes 0.1.13's fix for the freeze after a race.

## v0.1.13 (2026-10-21)

- Fix for the freeze after a race: a game thread looped forever in a named
  memory pool lookup (sub_8215E5E8) whose block chain or free-block rover had
  gone bad. Each lookup now checks the pool first, puts a stray rover back at
  the pool's start, and answers "not found" for a broken chain instead of never
  returning. Every problem goes to the log as [pool-guard].

## v0.1.12 (2026-10-21)

- Android power hints (ADPF): the CPU runs at the lowest clocks that still make
  60 fps, for less heat. Settings > Advanced > Power Hints turns it off.
- Firmer 60 Hz display request; the launcher's toggle can turn it off.
- Hang and crash reports and Save Bug Report zips also go to
  Download/SpeedBreaker-logs.
- Guarded USA cheats: Black Edition content and an experimental career payout
  top-up, both off unless chosen in CHEATS; a SAVED touch layout keeps the
  original custom one.
- The launcher checks for updates and opens the private release to download.
- Mali-G1 workarounds stay on after a GPU driver update.

## v0.1.11 (2026-10-16)

- Android GPU driver launch selector: import ARM64 Turnip ZIPs, retain multiple
  drivers, restore System, resume or explicitly restart the game.
- Generation-safe shader predicate coalescing and safe no-color depth-stage
  reduction, with independent restart rollback controls in Advanced.
- Position-only depth vertex outputs; alpha-tested casters and shader side
  effects retained. Pass timing includes stage eligibility and geometry counts.
- Custom loader dispatch covers rendering, UI and Android surface recreation;
  probe failures fall back to System and logs identify the actual Vulkan driver.

## v0.1.10 (Android GPU measurements and presentation fix)

- Stop rebuilding usable Android swapchains repeatedly for persistent SUBOPTIMAL
  reports caused by compositor rotation; retain resize, surface-loss and
  OUT_OF_DATE recovery.
- Automatically sample GPU intervals for individual render passes, depth/color
  resolves, texture conversion, transfers and front-buffer conversion while racing.
  Store bounded CSVs under logs, without image readbacks or extra GPU waits.
- Record shader queue/compile/wait times, loading waits, skipped draws and the
  active asynchronous-shader setting. Preserve the player's shader preference.
- Collector verifies pass timing output and performs the Home/background check.

Device speed and frame-pacing improvements remain to be tested.

## v0.1.9 (Android GPU work reduction)

- Adapt native-renderer early depth rejection for read-only depth-tested draws;
  retain late tests for depth/stencil writes, depth exports, memory exports and queries.
- Supply actual sampled image reciprocals to offset texture samples, including
  scaled render targets; retain guest sizes for unnormalized 2D coordinates.
- Specialize 1x 32-bit resolves and dispatch lanes in tiled-word order, preserving
  all guest memory writes and cached texture stores.
- Advanced > GPU Work Reduction switches these paths off after restart for testing.
- Pause on Android window focus loss as well as Activity background events;
  avoid resuming a background Activity on focus alone. Park queued GPU batches.

No confirmed RP6 FPS improvement yet. No shadow, motion blur or resolution reduction.

## v0.1.8 (Android performance trial)

- Android GPU Scheduling: Overlap trials guarded vertex-stage overlap without
  reducing resolution, shadows or effects. Conservative restores prior barriers;
  Compare alternates both policies every 480 frames for testing.
- Port upstream v0.1.3 queued-frame game-time correction for periodic driving hitches.
- Automatic visual image capture is off for this performance build. Existing
  capture code remains available through NFSMW_VISUAL_CAPTURE=1.
- Bounded performance CSVs record CP-swap interval percentiles, policy and
  completed GPU work; transition windows are marked. No images/game data dumped.
- Correct hardware timestamp arithmetic across counter wrap.

- Merge upstream through v0.1.4: gamma/brightness, nitrous trails, tuning labels,
  low-FPS game timing, resolution and presentation fixes. Preserve Android ISO,
  audio, lifecycle and capture paths. Official desktop update action is hidden
  on Android.

This is an on-device performance experiment, not a verified FPS improvement.
## v0.1.4

### What's new

- The picture now matches a real Xbox 360 on a TV: slightly deeper shadows and slightly brighter
  mid-tones. The game sets a gamma ramp for the console's TV output, and SpeedBreaker never
  applied it until now. The new Settings > Display > Xbox 360 Display Gamma is on by default, also
  after an update: to get the previous look back, turn it off.
- Settings > Display > Brightness: lighter or darker shadows and mid-tones, from -5 to +5, a
  gentle step at a time, like a TV's brightness. Black and white stay put, and only the game's
  picture changes, never the menus. At 0, the default, it leaves the picture as it is.
- The game's own Brightness slider (pause menu > Options > Video) now changes the picture as you
  move it, as on an Xbox 360: the game writes a new gamma ramp for each step, and SpeedBreaker
  applies it. It needs Settings > Display > Xbox 360 Display Gamma, which is on by default; with
  that off it does nothing, as before. If you moved the slider in an earlier version and the game
  has saved since, that brightness now takes effect: a slider left at maximum gives a much
  brighter, washed-out picture, as a 360 with that save would show. Set the slider back to the
  middle, or press Y (Defaults) and choose Yes. Thanks to m9cy884mv6-a11y for the report (GitHub
  issue #18).
- Keyboard: the right stick has keys, O and L for up and down, K and ; for left and right. The
  game shifts gears with the right stick, so you can now shift from the keyboard and finish a
  drag race. Thanks to love-gamer, who wrote it (GitHub pull request #11).
- Settings > Quit Game, at the bottom of the menu after Close: quits SpeedBreaker with a controller,
  the mouse or the keyboard, so a fullscreen game no longer needs a keyboard shortcut to leave. It
  asks first, with Cancel chosen, so no single press quits. Not on iPhone and iPad, where apps are
  closed from the app switcher.
- Settings > Display > Fullscreen now decides how the game starts, every time. On a Mac, the
  window's green button (or Ctrl+Cmd+F) switches fullscreen until you quit, so the game no longer
  sometimes starts fullscreen and sometimes not.
- Mac: the mouse pointer no longer sits over the game. It hides after two seconds without moving
  while you play, and comes back as soon as you move the mouse; in the settings menu and the
  installer it stays. Thanks to DoubleDips for the report (GitHub issue #15).
- Mac: smoother frame pacing. The game's 60 Hz clock now keeps time on macOS (it ran up to 9 ms
  late), and on a MacBook Pro's 120 Hz ProMotion screen each frame now follows the last by 1/60 s
  instead of arriving whenever the GPU finished it. Other screens show frames as before, and the
  timing turns itself off if a screen doesn't keep it.
- On slow GPUs, below 15 fps, the game no longer runs a few percent slow: its clock now keeps up
  with the screen (a laptop with Intel graphics at Internal Resolution 1.5x, about 12 fps, ran 4%
  slow).
- Changing the window's shape while a movie plays (for example the green button during the
  opening movie) no longer shows a stray "EVENT HANDLER" label above the picture.

### More about it

- The gamma ramp: the game sets it when it starts, and the console applies it to every frame on
  its way to the TV. Two real Xbox 360 recordings of the intro movie and the title screen match it
  to within half a level, against about two levels without it. With Xbox 360 Display Gamma off
  and Brightness at 0, the picture is exactly the one of earlier versions.
- The game's slider: each step writes a new ramp, which shows on that frame. At its lowest, its
  default and its highest, the picture's average brightness moves by about -13, +1 and +24 levels
  (of 255) against Xbox 360 Display Gamma off. The game keeps the slider in your alias's save the
  next time it saves the profile; without an alias it starts at the middle every time.
- The right-stick keys: holding both keys of a pair centres the stick. No other key changed.
- Fullscreen: a switch the game didn't ask for (on a Mac, the green button or Ctrl+Cmd+F) lasts
  until you quit and never changes the setting. While the window differs from the setting,
  Settings > Display > Fullscreen says NOW FULLSCREEN or NOW WINDOWED; choosing On or Off there
  saves it and switches the window.
- The pointer hides only over the game's window, so in a window it still shows over the desktop,
  and the mouse is never captured.
- The pacing: macOS let the thread that drives the game's 60 Hz clock wake up to 9 ms late; it
  now runs with real-time priority. And a ProMotion screen shows each frame as soon as it's ready,
  so frames that took a little longer to draw arrived unevenly: SpeedBreaker now asks for each
  frame to be shown at a set time after its tick. Measured on a MacBook Pro (M1 Pro) in
  fullscreen: 99.9% of frames 1/60 s apart in menus and about 94% while driving (99-100% on the
  open road, less in the heaviest scenes, where a frame takes longer than the time allowed),
  against about a quarter without it, for about 5 ms more latency.
- The slow clock: when a frame took longer than six screen refreshes, SpeedBreaker started its
  count over and dropped up to three refreshes of game time each time; below 15 fps that happened
  every second or so. That time is now given back over the next frames. Above 20 fps nothing
  changes. Checked by replaying frame timings recorded on that laptop: game time went from 96% to
  99.9% of real time.
- The label: it belongs to a menu the game keeps just above the picture. After a window change
  mid-movie the game kept the old window's layout for it, so the label slipped into view; it now
  gets the new layout at once.
- Updating keeps your saves, settings and the installed game, and needs no disc image: on a Mac,
  drag the new app into Applications and choose Replace, then Open Anyway once more; on SteamOS,
  unpack the new download over the old folder.

## v0.1.3

### What's new

- Smoother driving: no more small hitch every few seconds (the car jumped ahead, then stood
  still for a frame or two), which frame-time graphs didn't show (GitHub issue #9). Thanks to
  m9cy884mv6-a11y for the careful report.
- Sun shadows of cars, walls and tunnel roofs: cars cast their hard shadow in sunlight again, and
  walls, barriers and tunnel roofs shade the road and the cars, as on the Xbox 360. Inside tunnels
  the car no longer flickers between sunlight and tree shadows (GitHub issue #3). Thanks to the
  players who reported the flicker on GitHub and the missing car shadows on Reddit.
- Nitrous light trails are as long as on the Xbox 360 again: at 60 fps they were half as long, and
  looked brighter (GitHub issue #10). Thanks to jaswinprakash for the report, and to victorgbd,
  whose Android port of another Most Wanted recompilation found the cause.
- Pause menu > Performance Tuning: each slider has its own name again (Steering, Handling, Brakes,
  Ride Height, Aerodynamics, Nitrous, Turbo or Supercharger) instead of "Steering" on every row
  (GitHub issue #8).

### More about it

- The hitch: the game's main loop runs a couple of frames ahead of the screen. When one of its
  frames took longer, SpeedBreaker gave it two or three steps of game time and the next ones
  none, while the screen still showed one frame per refresh. A frame the screen shows for one
  refresh now runs one step. The game's speed is unchanged.
- The shadows: the game draws cars, walls and tunnel roofs into its sun shadow map as plain solid
  shapes, which on the Xbox 360 run no pixel shader. SpeedBreaker ran the trees' leaf-cutting
  shader on them too, and it threw them all away. Checked against real Xbox 360 footage of the
  same race. Sunlit roads are as bright as in v0.1.1 and v0.1.2.
- The light trails: the game draws them from where the car was over its last two frames, and the
  Xbox 360 races at 30 fps, so at 60 fps they covered half the distance. They now span the same
  time as on the 360. At Frame Rate 30 nothing changes.
- The tuning labels: the recompiled code looked up each row's name in a table that had lost all
  but its first entry.
- Updating keeps your saves, settings and the installed game, and needs no disc image: on a Mac,
  drag the new app into Applications and choose Replace, then Open Anyway once more; on SteamOS,
  unpack the new download over the old folder.

## v0.1.2

### What's new

- Internal Resolution 1.5x (1080p), in Settings > Graphics: a much sharper picture than 1x, with
  a little over half the pixels of 2x: for a GPU with room above 1x but not for 2x. Thanks to the
  Reddit player who asked for it.
- Laptops with two GPUs: the game now runs on the dedicated graphics card (NVIDIA or AMD), not on
  the one built into the processor. On Linux with NVIDIA's driver, the launcher also turns on
  NVIDIA's PRIME render offload: thanks to lydacious for the idea and the first version (GitHub
  pull request #6).
- Portable mode on Steam Deck, Steam Machine and Linux: put a file named `portable.txt` next to
  `speedbreaker.sh`, and SpeedBreaker keeps your settings, saves, logs and caches in that folder,
  and installs the game there too. Move or copy the whole folder to another machine and play on
  (GitHub issue #1).
- Taller and wider screens: the sun's glare is round again (it was stretched up to 1.33 times
  taller on the Steam Deck, 4:3 and 3:2, and wider on ultrawides); no faint line of a letterboxed
  movie shows in the black past the 16:9 picture; and the race results screen is black past its
  16:9 picture, as on a 16:9 TV, instead of showing the road behind it.
- iPhone: after a screenshot, the picture no longer shrinks into a corner of the screen with the
  settings menu blown up.
- Experimental: an IPA for iPhone and iPad, to sideload with your own Apple Account.

### More about it

- 1.5x renders 2.25 times the pixels of 1x, where 2x renders 4 times. Auto still chooses between
  whole scales (1x, 2x or 3x) as before.
- The game picks a dedicated (discrete) GPU over a built-in (integrated) one, and of two alike the
  one with more video memory. The log's `[video] Vulkan devices:` line lists every GPU it found,
  the one it uses, and why. To use another, put `NFSMW_GPU=<its number or part of its name>` in
  the launch options, e.g. `NFSMW_GPU=intel %command%`. On the Steam Deck, Steam Machine, Steam
  Frame, a Mac, an iPhone or an iPad (one GPU each) nothing changes. The launcher sets
  `__NV_PRIME_RENDER_OFFLOAD=1` only on a PC where NVIDIA's driver runs one GPU beside another and
  NVIDIA's Vulkan driver is installed, and keeps a value you set yourself.
- `speedbreaker.sh --portable on` copies what you have now (settings, saves and the installed game)
  into the folder and turns portable mode on; `--portable off` copies them back. Both copy and
  never delete: your old files stay until you remove them. Not on the Mac, where an app can't keep
  its data inside itself.
- The IPA: the game asks iOS for two memory entitlements, and a free Apple Account can give at most
  one of them, with some tools only: with a free account it may not start, or iOS may close it in
  busy scenes. scripts/ios/README.md says which tools keep what. When iOS refuses the memory, the
  game now says why on screen instead of closing.
- iPhone and iPad: on iOS 27 the screenshot screen could leave the game laid out as if the phone
  were held upright, until the game was closed. The game now checks its picture against the
  screen after coming back from the screenshot screen, the app switcher or Control Center, and
  puts it right if they disagree; an iPhone, which plays only in landscape, never draws upright.
- iPhone and iPad: Internal Resolution's Auto now decides chip by chip, and the log says what it
  chose and why. Every chip still gets 1x (720p) under Auto, the iPhone 18 Pro Max included: in a
  long race on a charger it got hot even at 1x. 1.5x and 2x are in Settings > Graphics > Internal
  Resolution. The log also records the battery level and charging state.
- After a macOS, SteamOS or graphics driver update, the first drive can stutter for a moment while
  the game rebuilds its shaders for the new system. It happens once, and the game's log now says
  so.
- Updating keeps your saves, settings and the installed game, and needs no disc image: on a Mac,
  drag the new app into Applications and choose Replace, then Open Anyway once more; on SteamOS,
  unpack the new download over the old folder.

## v0.1.1

### What's new

- The Steam Deck and 16:10 Macs fill the screen: a slightly taller view, with the HUD kept at
  16:9. Settings > Display > Aspect Ratio > 16:9 brings the bars back.
- Sunlit roads and correct shadows. A bug in the shadow map left every road in shade, on every
  device: the roads are now as bright as on the Xbox 360, and the line between shade and sun that
  followed the car is gone (GitHub issue #3). Thanks to the Reddit tester who spotted it.
- Cutscene bars: the police video camera in the career intro, and the intro's black frames, now
  cover the whole screen on taller and wider screens.
- macOS 15 Sequoia support: the Mac app runs on macOS 15 or later (v0.1.0 needed macOS 26). On
  macOS 26 it runs exactly as before.
- Linux startup crash fixes: no crash at startup on graphics cards without Resizable BAR (NVIDIA),
  and none expected on Intel's Linux driver (GitHub issue #2).
- Choose a folder... in the installer: put the game in any folder you like (GitHub issue #1).
- Check for Updates, in Settings > Advanced.
- A cleaner Linux folder: speedbreaker.sh is the only thing at the top to run, and the program
  moved into lib/.

### More about it

- Choose a folder... is on the installer's last page. An empty folder is used as it is; one that
  already holds files gets a new SpeedBreaker Game folder inside. The game remembers the folder,
  and says so if its drive isn't connected. On SteamOS the folder picker opens only in Desktop
  Mode: in Game Mode, switch to Desktop Mode and choose the folder there, or run
  `speedbreaker.sh --install <image> --dest <folder>` from Konsole in Desktop Mode (or over SSH).
- Check for Updates asks GitHub whether a newer SpeedBreaker is out. If one is, it shows what
  changed, opens the download page, and shows that page as a QR code to scan with a phone (for
  Steam Deck Game Mode and the Steam Frame, where no browser may open). Nothing is checked online
  unless you press Check for Updates.
- Steam Deck, Steam Machine, Steam Frame and Linux: always start the game with speedbreaker.sh, as
  your Steam shortcut does. The program in lib/ can't pick its libraries on its own. Unpacked over
  v0.1.0, the first start removes v0.1.0's program from the top of the folder.
- Updating keeps your saves, settings and the installed game, and needs no disc image: on a Mac,
  drag the new app into Applications and choose Replace, then Open Anyway once more; on SteamOS,
  unpack the new download over the old folder.

## v0.1.0

The first public release of SpeedBreaker: an unofficial static recompilation of the Xbox 360
version of Need for Speed: Most Wanted (2005), running natively. Bring a backup of your own USA
Xbox 360 disc: the installer checks it and installs the game.

### Where it runs

- Steam Deck, Steam Machine and Linux PCs (x86-64).
- Steam Frame, as a big flat screen in the headset (not VR).
- Mac with Apple silicon, on macOS 26 or later.
- iPhone and iPad: there's no download, you build it yourself with Xcode.
- Windows: coming soon.

### What you get

- Up to 4K: the 3D scene renders at 1x, 2x or 3x the console's 720p, and Auto picks one for your
  screen and device.
- Ultrawide screens get a wider view instead of a stretched one, and 4:3 and 3:2 screens a taller
  one, with the HUD and the rear-view mirror where they belong.
- Up to 60 fps on a 60 Hz display, a steady 30 at full game speed, or Auto, which drops to 30 only
  while the device is hot or can't hold 60.
- The original look: the low sun's glare, correct colours on Apple devices, proper car paint and
  mirror reflections, and distant textures that don't shimmer.
- A guided installer: it finds your disc image, checks that it's the right game and verifies all
  49 files as it copies them.
- Controllers work as an Xbox 360 pad, with hot-plugging and rumble. The keyboard works too.
- A settings menu that pauses the game: F1, Back + Start (View + Menu on the Steam Deck and Steam
  Frame), or a three-finger tap on iPhone and iPad. Every change applies at once.
- On iPhone and iPad, switch away and the game freezes; come back and it carries on.
- Settings > Advanced > Save Bug Report puts the logs, your settings and a screenshot in one zip
  file. Nothing is uploaded: you decide what to send.
- From this version on, the first start after an update shows what changed, once. Nothing is
  checked online.

### Good to know

- Mac: the app isn't notarized by Apple, so the first time, allow it in System Settings > Privacy
  & Security > Open Anyway.
- Steam Deck OLED: set the screen to 60 Hz.
- Steam Frame: Display > Frame Rate 30 fps is the smooth choice in the headset.

SpeedBreaker is an unofficial fan project, not affiliated with Electronic Arts or Valve. No game
files are included.
