# vsoftphone

A SIP softphone built with C++17, Qt 6 / QML and PJSIP. Developed and tested
on Linux, with a cross-compiled Windows build.

## Features

- Compact window with colour themes (System, Light, Dark, Midnight, Nord,
  Solarized Light and Dark) and classic desktop themes that also redraw the
  controls (Windows 3.11, 95, 98, XP, 7 and Mac OS X), interface zoom, and an option to keep running
  in the system tray when the window is closed
- Multiple SIP accounts over UDP, TCP or TLS (server certificate verified by
  default), with registration status
- Calls: answer, hold/resume, mute, DTMF, blind transfer, conference,
  recording to WAV, ring tone, Do Not Disturb, auto answer
- Outbound proxy failover: a second proxy takes over when the first fails,
  and the first is tried again after a network change
- Direct IP calls (dial an IP address, with or without an account)
- BroadWorks busy lamp fields: set the BLF list URI on the account and the
  monitored users appear in Contacts as Idle, Ringing or Busy
- NAT traversal per account: STUN, ICE, and TURN relay when direct media is
  not possible; optional SRTP
- Instant messages with typing indication, stored in SQLite; a message to
  an IP address goes direct, with no account or server
- Video calls with H.264, VP8 and VP9 (regular build). H.265 is not
  available: PJSIP has no H.265 codec
- Contacts with groups and presence, call history with missed calls
- Audio device, volume, echo cancellation and codec selection, all persisted
- Re-registers when the network comes back

## Known limits

- Video calls are negotiated and shown in PJSIP's own SDL windows, not inside
  the call window
- SIP passwords are kept in the system password store: the desktop keyring
  on Linux (libsecret, loaded when present) and the Credential Manager on
  Windows. Where there is none they stay unencrypted in the local SQLite
  database (the file is readable by the owner only)
- "IPv6 only" accounts use IPv6 signalling over UDP only
- Busy lamp fields are tested against Asterisk's resource lists and a
  stand-in for a BroadWorks server, not against BroadWorks itself
- Windows is cross-compiled and tested under wine only; the macOS build
  target exists but has never been built
- No in-call file transfer; no iOS build

## Licensing

vsoftphone is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version (see [LICENSE](LICENSE)). The libraries it contains or ships keep their own
licences, all compatible with GPL v3:

- PJSIP: GPL v2 or later
- Qt: LGPL v3 (shipped as separate shared libraries in the portable folders)
- OpenSSL: Apache 2.0; Opus: BSD
- famfamfam Silk icons: CC BY 2.5

## Building

See [BUILD.md](BUILD.md).
