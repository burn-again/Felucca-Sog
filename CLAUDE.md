# FM-1 firmware (Felucca-Sog, a fork of Felucca)

Custom firmware for the M-VAVE FM-1, forked from Felucca by Hügelton Instruments
(GPL-3.0-only). Chip: JieLi AC791N/WL82 with a pi32v2 CPU, built with JieLi's clang 4.0.1
and three AC79 SDK files.

## Reading
- Read BUILDING.md. README.md is very long: open only the section you need.
- Don't explore the repo. Use the Map below. If something isn't listed, search for
  the exact name with rg instead of reading whole folders.

## Map
(to be filled in)

## Scope
- One task per session. Don't add extras I didn't ask for.
- When the change is pushed to sandbox (or the PR is open), stop.

## Tests
- Run tests only through tests/run_tests.sh, or the exact command it uses for one suite.
  Never hand-compile a test.
- If a chained command fails partway, everything after it is stale. Re-run before reading output.
- Don't run the full tests/run_tests.sh in the session; CI runs it on the PR to main.

## Sandbox (the default)
- Work on the `sandbox` branch: commit and push there, no PR per change. Each push builds the
  firmware and the emulator and deploys them to the site (no test suite, about 5 minutes), so
  the user tries the change at /webapp/try/ and on the FM-1.
- In the session: ./build.sh, then only the one suite that covers the change.
- Open a PR from sandbox to main only when the user says the device is ready; the full
  suite runs there once for the whole batch.

## Ask first
- Before changing any saved format (settings, projects, presets, backups), stop and explain
  the change and what happens on downgrade. Wait for my OK.

## Never
- Never modify firmware/loader/ (the update loader), the update entry (firmware/src/ota.c),
  the flash map, or anything that decides how the FM-1 boots or accepts updates. These keep
  a bad build recoverable from the web installer. If a task seems to need it, stop and ask.
- Never run tools/fm1_install.py or anything that writes to a device. The user flashes by hand.
- Never remove licence texts, credits or attributions. Add new ones for anything you port.
- Never commit SDK or toolchain files. They are fetched at build time.

## Repo layout
- firmware/engines/ is a git submodule (Felucca-Engines, MIT). If it is empty, run
  `git submodule update --init`. Don't edit files inside it. New engines go in
  firmware/src/eng_*.c like the others. Ask before changing ANALOG, LOFI, TRIO, WHEEL or NOISE.
- The package identity is PRODUCT in tools/build.py (FM-1_990). Keep it FM-1_9xx:
  web/make_site.py rejects anything else.

## The chip
- No hardware floating point and no libgcc. Prefer integer / fixed-point math in the audio
  path. No 64-bit division and no calls into the chip ROM. Follow the existing engines' style.
- The audio callback has a hard time budget. Check CPU cost against tests/target_budget.txt
  and tests/cpu_baseline.txt, and say in the PR how much CPU a change adds.
- Memory is small (about 512 KB RAM, 1 MB flash). Report image and RAM size in the PR.

## Every change
1. Work on `sandbox` (see Sandbox), never on main.
2. ./build.sh must succeed.
3. Run only the tests that cover what you changed (the regression renders in
   tests/regress.c for sound or engine changes, the UI tests for screen changes). If the
   sound changed on purpose, update the golden hashes for those renders and say so.
4. Keep the browser emulator (web/emu/) building. If you can't build it in this VM,
   say so and let CI build it.
5. Tell the user what changed, how to try it in the emulator (which buttons), CPU and RAM
   impact, and anything unverified. The sandbox to main PR collects the same.

## Style
- Small, focused changes. Match the existing code and naming.
- Saved projects and settings must keep loading. If you change a saved format, add a
  version number and keep loading the old one.
- Update README.md's feature list when you add something a player would notice.
- Don't re-verify the toolchain, download hosts or GitHub Action versions, and don't
  install extra tools (Emscripten, linters) unless the task needs them. CI builds the
  emulator.

## Borrowing from other firmwares
- GPL-3.0 code (ChoralRoot, X0X, Melodee, Jangada and other Felucca forks) may be ported.
  Keep its copyright and SPDX lines and add the project to CREDITS.md.
- MIT, BSD or Apache-2.0 code may be ported; put its licence text in LICENSES/ and list
  it in LICENSING.md.
- GPL-2.0-only code, code with no licence, and vendor firmware: ideas only. Reimplement
  from a description; never copy.
- Never port another firmware's loader, update entry (ota) or installer code.
- Before porting, report the files, dependencies, CPU / RAM / flash cost and licences,
  and wait for a go-ahead.
