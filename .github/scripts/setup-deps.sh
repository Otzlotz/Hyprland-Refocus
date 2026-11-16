#!/usr/bin/env bash
# Install Hyprland build dependencies on Arch Linux.
# Hypr libraries are built at the revisions pinned in flake.lock: upstream CI uses their
# latest main, which drifts away from this fork's upstream base.
#
# usage: setup-deps.sh [--xorg]
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

sed -i 's/SigLevel    = Required DatabaseOptional/SigLevel    = Optional TrustAll/' /etc/pacman.conf
pacman --noconfirm --noprogressbar -Syyu
pacman --noconfirm --noprogressbar -S --needed \
  base-devel \
  cairo \
  clang \
  cmake \
  git \
  glaze \
  glm \
  glslang \
  gtest \
  hyprland-protocols \
  jq \
  libcanberra \
  libdisplay-info \
  libdrm \
  libei \
  libepoxy \
  libglvnd \
  libheif \
  libinput \
  libjxl \
  libliftoff \
  librsvg \
  libspng \
  libwebp \
  libxcursor \
  libxkbcommon \
  libzip \
  lua \
  muparser \
  ninja \
  pango \
  pixman \
  pkgconf \
  pugixml \
  re2 \
  seatd \
  systemd \
  tomlplusplus \
  wayland \
  wayland-protocols \
  xcb-util \
  xcb-util-errors \
  xcb-util-image \
  xcb-util-renderutil \
  xcb-util-wm

if [[ "${1:-}" == "--xorg" ]]; then
  pacman --noconfirm --noprogressbar -S --needed \
    xorg-server-common \
    xorg-setxkbmap \
    xorg-xkbcomp \
    xorg-xwayland
fi

LOCK="$REPO/flake.lock"
mkdir -p /tmp/hypr-deps
cd /tmp/hypr-deps

build() {
  local name="$1" target="${2:-all}"
  local rev
  rev="$(jq -er ".nodes[\"$name\"].locked.rev" "$LOCK")"
  echo "::group::$name @ $rev"
  git clone --recursive "https://github.com/hyprwm/$name" "$name"
  git -C "$name" checkout --recurse-submodules "$rev"
  cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:PATH=/usr -S "$name" -B "$name/build"
  cmake --build "$name/build" --config Release --target "$target" -j"$(nproc)"
  cmake --install "$name/build"
  echo "::endgroup::"
}

# order matters: later libraries depend on earlier ones
build hyprwayland-scanner
build hyprutils hyprutils
build hyprlang hyprlang
build hyprcursor
build hyprwire
build hyprgraphics hyprgraphics
build aquamarine aquamarine
