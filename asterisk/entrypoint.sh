#!/bin/sh
# Writes the Asterisk configuration for two test accounts, prints what to
# enter in each softphone, then runs Asterisk in the foreground.
set -e

PORT="${SIP_PORT:-5080}"
random_password() { tr -dc 'a-z0-9' < /dev/urandom | head -c 12; }
PASS_1001="${PASSWORD_1001:-$(random_password)}"
PASS_1002="${PASSWORD_1002:-$(random_password)}"
# With host networking this is the machine's own address
ADDRESS="$(ip -4 route get 1.1.1.1 2>/dev/null | sed -n 's/.* src \([0-9.]*\).*/\1/p')"
ADDRESS="${ADDRESS:-127.0.0.1}"

cd /etc/asterisk

# Everything loads by default; these would also listen on the host's ports
cat > modules.conf <<CONF
[modules]
autoload = yes
noload = chan_sip.so
noload = chan_iax2.so
noload = chan_mgcp.so
noload = chan_skinny.so
noload = chan_unistim.so
noload = pbx_dundi.so
noload = res_ari.so
CONF

cat > rtp.conf <<CONF
[general]
rtpstart = 10000
rtpend = 10100
CONF

account() { # number, name, password, the other account
cat <<CONF

[$1](phone)
auth = $1
aors = $1
callerid = "$2" <$1>

[$1]
type = auth
auth_type = userpass
username = $1
password = $3

[$1]
type = aor
max_contacts = 3
remove_existing = yes

; The softphone's "BLF list URI": one subscription for the other account's lamp
[blf-$1]
type = resource_list
event = dialog
list_item = $4
full_state = yes
CONF
}

cat > pjsip.conf <<CONF
[udp]
type = transport
protocol = udp
bind = 0.0.0.0:$PORT

[tcp]
type = transport
protocol = tcp
bind = 0.0.0.0:$PORT

[phone](!)
type = endpoint
context = phones
message_context = phone-messages
disallow = all
allow = opus,g722,ulaw,alaw
direct_media = no
rtp_symmetric = yes
force_rport = yes
rewrite_contact = yes
$(account 1001 Alice "$PASS_1001" 1002)
$(account 1002 Bob "$PASS_1002" 1001)
CONF

cat > extensions.conf <<CONF
[phones]
; The accounts call each other; the hints feed presence and the busy lamps
exten => 1001,hint,PJSIP/1001
exten => 1002,hint,PJSIP/1002
exten => _100X,1,Dial(PJSIP/\${EXTEN},30)
 same => n,Hangup()
; 600 repeats what you say, 601 plays a steady tone
exten => 600,1,Answer()
 same => n,Echo()
exten => 601,1,Answer()
 same => n,Milliwatt()

[phone-messages]
exten => _100X,1,MessageSend(pjsip:\${EXTEN},\${MESSAGE(from)})
 same => n,Hangup()
CONF

cat <<INFO

=== Softphone settings =========================================
Asterisk is listening on $ADDRESS:$PORT (UDP and TCP).
In each softphone, add an account with:

  Softphone 1:  Username 1001   Password $PASS_1001   BLF list blf-1001
  Softphone 2:  Username 1002   Password $PASS_1002   BLF list blf-1002
  Both:         Domain $ADDRESS:$PORT   Transport UDP (or TCP)

Leave "Use STUN" off: phone and server are on the same network.
Dial 1001 or 1002 to call the other phone, 600 for an echo test,
601 for a steady tone.
================================================================

INFO

exec asterisk -f
