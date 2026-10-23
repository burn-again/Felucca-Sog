# Felucca

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)

This is **Felucca-Sog**, a fork of [Felucca](https://github.com/hugelton/Felucca) by Hügelton Instruments.

What Felucca-Sog adds:

- **COHERE** (КОГЕРЕНТ), the coherence engine: a stack of 2 to 5 detuned saws per voice that falls apart on every note
  (the spread opens, samples hold, bits drop, short dropouts stutter), then a Kuramoto pull hauls the phases back into step
  and lets go, and the drift apart is the chorus. EDIT: OSC SPRD HIT REC / PULL DRFT WEAR DROP; HOME: HIT REC PULL WEAR.
  Settings saved by Felucca (or older Felucca-Sog) load as they were: the MIDI LEARN map moves to its new place.
- **POLV**, a Polivoks-style filter for ANALOG (EDIT > FILTER > TYPE), the default on power-on and for new sounds:
  saturating integrators, so high RES screams and bites instead of ringing clean. Saved projects keep their filter.
- **DIST is Chebyshev**: turning DST up adds the 3rd and 5th harmonics in their exact orders (and a touch of the 2nd)
  instead of a generic soft clip. With POLV, ANALOG's **DRV** drives into the filter itself: more DRV, more grit.
- **CHEB**, an INSERT type: DRIVE, ORDER (which harmonic: 2nd .. 7th, swept smoothly), LEVEL; MIX brings the
  fundamental back.

## Maintenance Notice

What began as a personal experiment grew far beyond my expectations. I’m grateful to everyone who contributed, supported the project, and made music with Felucca.

I’ve decided to step back from active development to focus on creating new instruments and music.

Felucca will remain available with occasional maintenance, but new features, issue responses, and pull request reviews are not guaranteed.

Thank you for everything. I hope you’ll look forward to my next creations.

## Overview

Multi-engine synthesizer firmware for the M-VAVE FM-1, version 1.6.1. Connect the FM-1 to a computer by USB, open the
[web installer](https://hugelton.github.io/Felucca/) in Chrome or Edge and press Install; no extra hardware is needed.
Installing is at your own risk: M-VAVE's updater or the installer's **Return to official V15** takes you back.
[Try it in your browser](https://hugelton.github.io/Felucca/webapp/try/), no FM-1 needed.

- Install: [web installer](https://hugelton.github.io/Felucca/)
- Edit: [web editor](https://hugelton.github.io/Felucca/webapp/editor/) ([Felucca-WebApp](https://github.com/hugelton/Felucca-WebApp))
- Read: the manual and the release notes are in the [wiki](https://github.com/hugelton/Felucca/wiki/Home)
- Build: [BUILDING.md](BUILDING.md)
- Related: [Felucca-Engines](https://github.com/hugelton/Felucca-Engines) (the ANALOG, LOFI, TRIO, WHEEL and NOISE engines, MIT; still maintained) ·
  [felucca-bsp](https://github.com/hugelton/felucca-bsp) (the FM-1 hardware layer)

## AI disclaimer

Felucca is developed with the assistance of AI coding agents (code, tests, documentation, translation, maintenance). The
design, features, sound design and direction are the maintainer's or the community's, and no generative AI is used for music,
icons or artwork. See [On AI-Assisted Development and Responsibility](https://github.com/hugelton/Felucca/discussions/166).

## Credits and licence

Free software: [GPL-3.0-only](LICENSE); the Hügelton Sample Pack is CC0 1.0; the engines in `firmware/engines` are MIT
([Felucca-Engines](https://github.com/hugelton/Felucca-Engines)); the bundled fonts and ported DSP keep their own licences
([LICENSES/](LICENSES/), [LICENSING.md](LICENSING.md)). Thanks to everyone whose work and contributions are in it: [CREDITS.md](CREDITS.md).

M-VAVE and FM-1 are trademarks of their respective owners. Felucca is not affiliated with or endorsed by them.

Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
