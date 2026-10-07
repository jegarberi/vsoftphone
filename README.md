# Microsip Clone

A SIP softphone built with C++17, Qt 6 / QML and PJSIP. Built and tested on
Linux.

## Features

- Multiple SIP accounts over UDP, TCP or TLS, with registration status
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
- SIP passwords are stored unencrypted in the local SQLite database (the file
  is readable by the owner only)
- TLS does not verify the server certificate
- No in-call file transfer; no Windows, macOS, Android or iOS builds

## Building

See [BUILD.md](BUILD.md).
