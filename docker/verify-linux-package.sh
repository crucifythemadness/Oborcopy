#!/usr/bin/env bash
# Smoke test of the Linux package in a clean ubuntu:24.04 container (no Qt installed).
#   docker run --rm -v <repo>/deploy-linux:/pkg:ro -v <repo>/docker:/scripts:ro ubuntu:24.04 \
#       bash /scripts/verify-linux-package.sh
set -euo pipefail
mkdir -p /t && cd /t
tar xzf /pkg/oborcopy-*-linux-x86_64.tar.gz
cd oborcopy-*-linux-x86_64

echo "== CLI on a bare system"
./oborcopy-cli --version
mkdir -p /data/src/Album /data/dst
echo "track" > /data/src/Album/01.flac
echo "old" > /data/dst/old.mp3
cat > /data/job.json <<'EOF'
{"name": "verify", "source": "/data/src", "destination": "/data/dst", "mode": "mirror",
 "safety": {"thresholdEnabled": false}}
EOF
set +e
./oborcopy-cli --job-file /data/job.json --quiet --yes; code=$?
set -e
echo "exit code: $code (expected 3 = copied + deleted)"
[ "$code" -eq 3 ] && [ -f /data/dst/Album/01.flac ] && [ ! -f /data/dst/old.mp3 ]

echo "== GUI with typical desktop libraries"
apt-get update -qq && apt-get install -y -qq libgl1 libegl1 libfontconfig1 libx11-xcb1 >/dev/null
missing="$(ldd bin/oborcopy lib/*.so* plugins/*/*.so 2>/dev/null | grep 'not found' | sort -u || true)"
if [ -n "$missing" ]; then echo "MISSING:"; echo "$missing"; exit 1; fi
echo "no missing libraries"
set +e
QT_QPA_PLATFORM=offscreen timeout 5 ./oborcopy; code=$?
set -e
echo "GUI exit code: $code (124 = still running after 5 s -> started fine)"
[ "$code" -eq 124 ]

echo "== GUI on a real X server (xcb platform plugin, via Xvfb)"
apt-get install -y -qq xvfb xauth >/dev/null
set +e
xvfb-run -a sh -c 'unset QT_QPA_PLATFORM; QT_DEBUG_PLUGINS=0 timeout 5 ./oborcopy'; code=$?
set -e
echo "GUI (xcb) exit code: $code"
[ "$code" -eq 124 ]
echo "== ALL CHECKS PASSED"
