create a SIP softphone using QT6 and QML, using pjsip
it must support
    several accounts
    several audio codecs
    seceral video codecs
    do not disturb
    autoanswer
    history
    audio device selection and test
    video device selection and test
    conference
    messages
    recording
    outbound proxy
    different id and login_id
    udp,tcp,tls,dtls,rtp,rtsp
    autodetect network changes and reconnect
    autodetect audio and video devices and select best
build script for linux, windows,macos, ios and android
Dockerfile based on debian to cross compile for all plattform
pjsip must support v4l2, pulse audio, jack, core audio, wasapi, directsound,ffmpeg,openh264,opus,vp8,vp9,av1 where available
implement accounts configuration window
implement settings configuration window
implement contacts window
the call must open a new window with call controls, video preview and chat with file transfer
implement save to configuration file
implement save history to sqlite
implement save messages to sqlite
main window should have 3 tabs: accounts, contacts and history
contacts should support groups
dont preload any data
in account settings show the name of the account and the status of the account
make a build script that creates static binary, dynamic binary, Release binary and Debug binary
use conan to handle all dependencies
update README.md
update BUILD.md
update Dockerfile
create a separate bash script to build for windows creatint a 32bit and 64bit binary, Debug and Release BUILD