#!/usr/bin/env bash
# macOS (homebrew). Two macOS-only concerns the cross-distro defaults can't
# express:
#
#   1. Conditional python install. Most macOS hosts already provide a
#      python3 (Apple's framework, Xcode CLT, GH Actions runners). Running
#      `brew install python@3.11` on those hosts fails at the link step
#      because Apple's framework already owns /usr/local/bin/python3.11.
#      We only install brew python when there is genuinely no python3 on
#      PATH — which is also when nothing owns those symlinks, so brew's
#      link succeeds.
#
#   2. PATH munging. Apple ships BSD make, BSD coreutils, and
#      Apple/clang-15 at /usr/bin/. We want GNU make, GNU coreutils, and
#      llvm@18's clang in front so `make build` / `clang` behave the way
#      they do on Linux. Append a profile snippet so subsequent shells
#      pick up the new PATH.

# shellcheck source=../lib/sets.sh
. "$LIB/sets.sh"

if ! command -v brew >/dev/null 2>&1; then
    echo "macos.sh: brew is not installed; install from https://brew.sh" >&2
    exit 1
fi

brew_default_install

if ! command -v python3 >/dev/null 2>&1; then
    echo "==> [redisbloom] python3 not on PATH; installing brew python@3.11"
    HOMEBREW_NO_AUTO_UPDATE=1 brew install python@3.11
fi

LLVM_VERSION="18"
BREW_PREFIX="$(brew --prefix)"
GNUBIN="$BREW_PREFIX/opt/make/libexec/gnubin"
LLVM="$BREW_PREFIX/opt/llvm@$LLVM_VERSION/bin"
COREUTILS="$BREW_PREFIX/opt/coreutils/libexec/gnubin"

update_profile() {
    local profile_path=$1
    local newpath="export PATH=$COREUTILS:$LLVM:$GNUBIN:\$PATH"
    grep -qxF "$newpath" "$profile_path" 2>/dev/null \
        || printf '%s\n' "$newpath" >> "$profile_path"
    if [ -n "${GITHUB_PATH:-}" ]; then
        printf '%s\n' "$newpath" >> "$GITHUB_PATH"
    fi
}

[ -f "$HOME/.bash_profile" ] && update_profile "$HOME/.bash_profile"
[ -f "$HOME/.zshrc" ]        && update_profile "$HOME/.zshrc"
true
