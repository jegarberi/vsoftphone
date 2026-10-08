#!/bin/sh
# Prints what to enter in the softphone, then runs coturn in the foreground.
set -e

random_password() { tr -dc 'a-z0-9' < /dev/urandom | head -c 16; }
USER_NAME="${TURN_USER:-softphone}"
PASSWORD="${TURN_PASSWORD:-$(random_password)}"
REALM="${TURN_REALM:-microsip}"
# With host networking this is the machine's own address
ADDRESS="$(hostname -I 2>/dev/null | cut -d' ' -f1)"
ADDRESS="${ADDRESS:-127.0.0.1}"

cat <<INFO
=== Softphone settings (Settings > Network) ===
  TURN            ticked
  TURN server     ${EXTERNAL_IP:-$ADDRESS}:3478
  TURN username   $USER_NAME
  TURN password   $PASSWORD
  To force every call through it, also tick
  "Always send media through the TURN relay".
===============================================
INFO

# Behind NAT (a cloud machine with a private address) the relay has to hand
# out the public address: EXTERNAL_IP=<public address>
exec turnserver -c /turn/turnserver.conf \
    --realm="$REALM" --user="$USER_NAME:$PASSWORD" \
    ${EXTERNAL_IP:+--external-ip="$EXTERNAL_IP"}
