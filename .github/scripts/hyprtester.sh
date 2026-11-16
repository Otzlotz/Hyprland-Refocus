#!/usr/bin/env bash
# Build Hyprland with tests and run hyprtester. Runs as root inside a privileged Arch
# container that has the host's /dev/dri (a vkms virtual display) mounted: aquamarine
# needs a DRM device for buffer allocation even in headless mode.
#
# usage: hyprtester.sh <log output dir>
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOGS="${1:?log output dir}"
TESTER=hyprtester

echo "::group::dependencies"
"$REPO/.github/scripts/setup-deps.sh" --xorg
git config --global --add safe.directory '*'
# runtime needs of the tests: software GL, a seat, the clients the tests spawn, and a
# font (kitty aborts without one)
pacman --noconfirm --noprogressbar -S --needed mesa seatd kitty xorg-xeyes wl-clipboard fontconfig ttf-dejavu
echo "::endgroup::"

echo "::group::build"
cd "$REPO"
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON \
  -DBUILD_TESTING=ON \
  -DWITH_COVERAGE=OFF
cmake --build build -j"$(nproc)"
cmake --install build
echo "::endgroup::"

# Hyprland refuses to run as root
useradd -m "$TESTER"
chmod -R a+rwX "$REPO"
chmod a+rw /dev/dri/*
ls -l /dev/dri

RUNTIME=/tmp/rt
mkdir -p "$RUNTIME" "$LOGS"
chown "$TESTER" "$RUNTIME"
chmod 700 "$RUNTIME"

mkdir -p /etc/kitty
cat >/etc/kitty/kitty.conf <<'EOF'
confirm_os_window_close 0
remember_window_size no
initial_window_width  640
initial_window_height 400
EOF

seatd -u "$TESTER" &
SEATD_PID=$!
sleep 1

STATUS=0
runuser -u "$TESTER" -- env \
  XDG_RUNTIME_DIR="$RUNTIME" \
  XDG_CACHE_HOME=/tmp \
  KITTY_CONFIG_DIRECTORY=/etc/kitty \
  LIBSEAT_BACKEND=seatd \
  AQ_TRACE=1 \
  HYPRLAND_TRACE=1 \
  HYPRLAND_NO_CRASHREPORTER=1 \
  bash -c "cd '$REPO' && build/hyprtester/hyprtester -c hyprtester/test.lua -b build/Hyprland -p hyprtester/plugin/hyprtestplugin.so" \
  2>&1 | tee "$LOGS/hyprtester.log" || STATUS=$?

kill "$SEATD_PID" || true

find "$RUNTIME" /tmp/hypr -name '*.log' -exec cp --backup=numbered {} "$LOGS/" \; 2>/dev/null || true
chmod -R a+r "$LOGS"

exit "$STATUS"
