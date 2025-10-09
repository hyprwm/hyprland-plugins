#!/usr/bin/env bash
set -euo pipefail

# Link local hyprexpo.so into the hyprpm-installed plugin path for quick testing.
#
# Usage:
#   ./dev-link.sh                 # auto-detect installed hyprexpo.so and symlink to local build
#   ./dev-link.sh -b              # build first, then link
#   ./dev-link.sh -t /path/to/hyprexpo.so  # specify target path explicitly
#   ./dev-link.sh -r              # restore original file from .bak and remove symlink
#
# Notes:
# - This script only affects your local user install if hyprpm installed to XDG_DATA_HOME.
# - If your hyprexpo is system-wide, you may need sudo to replace it.

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
local_so="$here/hyprexpo.so"
target_so=""
do_build=0
do_restore=0
do_list=0
do_interactive=0

msg() { echo "[dev-link] $*"; }
err() { echo "[dev-link] ERROR: $*" >&2; exit 1; }

usage() {
  sed -n '2,20p' "$BASH_SOURCE" | sed 's/^# \{0,1\}//'
}

detect_target() {
  local data_home="${XDG_DATA_HOME:-$HOME/.local/share}"
  local cache_home="${XDG_CACHE_HOME:-$HOME/.cache}"
  local candidates=(
    "$data_home/hyprpm"
    "$cache_home/hyprpm"
    "$data_home/hyprland"
    "$HOME/.local/lib/hyprland"
    "/usr/lib/hyprland"
    "/usr/lib64/hyprland"
  )

  for base in "${candidates[@]}"; do
    if [[ -d "$base" ]]; then
      local found
      # Prefer plugin tree
      found=$(find "$base" -type f -name hyprexpo.so 2>/dev/null | head -n1 || true)
      if [[ -n "${found:-}" ]]; then
        echo "$found"
        return 0
      fi
    fi
  done
  return 1
}

detect_runtime_target() {
  # Extract hyprexpo.so from the running Hyprland process mappings
  local pid
  pid="$(pidof Hyprland 2>/dev/null || pgrep -x Hyprland 2>/dev/null || true)"
  [[ -n "$pid" ]] || return 1
  local path
  # print the last column (pathname) and stop at first match
  path="$(awk '/hyprexpo\.so/ {print $NF; exit}' "/proc/$pid/maps" 2>/dev/null || true)"
  if [[ -n "$path" && -f "$path" ]]; then
    echo "$path"
    return 0
  fi
  return 1
}

while (( "$#" )); do
  case "$1" in
    -b|--build) do_build=1; shift ;;
    -t|--target) target_so="${2:-}"; shift 2 ;;
    -r|--restore) do_restore=1; shift ;;
    -l|--list) do_list=1; shift ;;
    -i|--interactive) do_interactive=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) err "Unknown arg: $1" ;;
  esac
done

if (( do_restore )); then
  if [[ -z "$target_so" ]]; then
    target_so=$(detect_target || true)
  fi
  [[ -n "$target_so" ]] || err "Could not detect hyprexpo.so. Pass -t /path/to/hyprexpo.so"

  if [[ -L "$target_so" ]]; then
    msg "Removing symlink: $target_so"
    rm -f -- "$target_so"
  fi
  if [[ -f "$target_so.bak" ]]; then
    msg "Restoring backup: $target_so.bak -> $target_so"
    mv -f -- "$target_so.bak" "$target_so"
  else
    msg "No backup found at $target_so.bak — nothing to restore"
  fi
  exit 0
fi

if (( do_build )); then
  msg "Building hyprexpo.so"
  make -C "$here" all
fi

[[ -f "$local_so" ]] || err "Local build not found: $local_so (run with -b to build)"

if [[ -z "$target_so" || $do_list -eq 1 || $do_interactive -eq 1 ]]; then
  # Prefer the path actually loaded by the running Hyprland instance
  target_so_runtime="$(detect_runtime_target || true)"
  # Fallback to filesystem-based detection
  target_so_fs="$(detect_target || true)"
  # try hyprpm output as an additional fallback
  target_so_hpm=""
  if command -v hyprpm >/dev/null 2>&1; then
    out="$(hyprpm list 2>/dev/null || true)"
    if [[ -n "$out" ]]; then
      target_so_hpm="$(printf '%s\n' "$out" | grep -i hyprexpo | grep -oE '/[^ ]*hyprexpo\.so' | head -n1 || true)"
    fi
  fi
  # last resort: shallow scan
  target_so_scan="$(find "$HOME/.local/share" "$HOME/.cache" -maxdepth 7 -type f -name hyprexpo.so 2>/dev/null | head -n1 || true)"

  # collect candidates and filter out the local build if present
  local_abs="$(readlink -f "$local_so")"
  mapfile -t candidates < <(printf '%s\n' "$target_so_runtime" "$target_so_fs" "$target_so_hpm" "$target_so_scan" | awk 'NF' | awk '!seen[$0]++')
  filtered=()
  for c in "${candidates[@]}"; do
    ca="$(readlink -f "$c" 2>/dev/null || true)"
    [[ -n "$ca" && "$ca" != "$local_abs" ]] && filtered+=("$ca")
  done

  if (( do_list )); then
    if ((${#filtered[@]} == 0)); then
      msg "No installed hyprexpo.so found (excluding local build)."
    else
      printf '%s\n' "${filtered[@]}"
    fi
    exit 0
  fi

  if (( do_interactive )); then
    if ((${#filtered[@]} == 0)); then
      err "No installed hyprexpo.so found (excluding local build)."
    fi
    echo "Select target hyprexpo.so to link:" >&2
    i=1
    for c in "${filtered[@]}"; do
      echo "  [$i] $c" >&2
      i=$((i+1))
    done
    read -rp "> " choice
    if ! [[ "$choice" =~ ^[0-9]+$ ]] || (( choice < 1 || choice > ${#filtered[@]} )); then
      err "Invalid selection"
    fi
    target_so="${filtered[$((choice-1))]}"
  else
    # pick the first filtered candidate if no explicit target provided
    if [[ -z "$target_so" && ${#filtered[@]} -gt 0 ]]; then
      target_so="${filtered[0]}"
    fi
  fi
fi

[[ -n "$target_so" ]] || err "Could not detect hyprexpo.so. Pass -t /path/to/hyprexpo.so"

msg "Target: $target_so"

local_abs="$(readlink -f "$local_so")"
target_abs="$(readlink -f "$target_so" 2>/dev/null || true)"
if [[ -n "$target_abs" && "$target_abs" == "$local_abs" ]]; then
  msg "Target is the local build; nothing to link."
  exit 0
fi

if [[ -L "$target_so" ]]; then
  current_link="$(readlink -f "$target_so")"
  if [[ "$current_link" == "$local_abs" ]]; then
    msg "Already linked to local build. Done."
    exit 0
  else
    msg "Target is a symlink to $current_link — replacing"
    rm -f -- "$target_so"
  fi
fi

if [[ -f "$target_so" ]]; then
  msg "Backing up existing file to $target_so.bak"
  cp -f -- "$target_so" "$target_so.bak"
fi

msg "Linking $target_so -> $local_so"
ln -sf "$local_so" "$target_so"

msg "Done. Restart Hyprland to load the local build."
