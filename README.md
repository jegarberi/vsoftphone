# Microsip Clone

A SIP softphone built with C++17, Qt 6 / QML and PJSIP. Developed and tested
on Linux, with a cross-compiled Windows build.

## Features

- Multiple SIP accounts over UDP, TCP or TLS (server certificate verified by
  default), with registration status
- Calls: answer, hold/resume, mute, DTMF, blind transfer, conference,
  recording to WAV, ring tone, Do Not Disturb, auto answer
- Direct IP calls (dial an IP address, with or without an account)
- BroadWorks busy lamp fields: set the BLF list URI on the account and the
  monitored users appear in Contacts as Idle, Ringing or Busy
- NAT traversal per account: STUN, ICE, and TURN relay when direct media is
  not possible; optional SRTP
- Instant messages with typing indication, stored in SQLite
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
- Busy lamp fields are verified against sample NOTIFY bodies, not against a
  BroadWorks server
- Windows is cross-compiled and tested under wine only; the macOS build
  target exists but has never been built
- No in-call file transfer; no Android or iOS builds

## Licensing

This repository has no licence file yet; choosing one is up to the owner.
Whatever is chosen has to fit the libraries the binaries contain or ship:

- PJSIP: GPL v2 or later (or a commercial licence from Teluu). A binary that
  links it can only be handed to others under GPL-compatible terms
- Qt: LGPL v3 (shipped as separate shared libraries in the portable folders)
- OpenSSL: Apache 2.0; Opus: BSD

## Building

See [BUILD.md](BUILD.md).
