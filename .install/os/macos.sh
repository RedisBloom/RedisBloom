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

# shellcheck source=../lib/packages.sh
. "$LIB/packages.sh"

if ! command -v brew >/dev/null 2>&1; then
    echo "macos.sh: brew is not installed; install from https://brew.sh" >&2
    exit 1
fi

brew_default_install

# python@3.11 only when the host has no python3 (see header). Honor the
# non-installing modes: print under dry-run, skip under check-deps.
if ! command -v python3 >/dev/null 2>&1; then
    if [ "${DRY_RUN:-0}" = 1 ]; then
        _dry "brew install python@3.11"
    elif [ "${CHECK_DEPS:-0}" != 1 ]; then
        echo "==> [redisbloom] python3 not on PATH; installing brew python@3.11"
        HOMEBREW_NO_AUTO_UPDATE=1 brew install python@3.11
    fi
fi

LLVM_VERSION="18"
BREW_PREFIX="$(brew --prefix)"
GNUBIN="$BREW_PREFIX/opt/make/libexec/gnubin"
LLVM="$BREW_PREFIX/opt/llvm@$LLVM_VERSION/bin"
COREUTILS="$BREW_PREFIX/opt/coreutils/libexec/gnubin"
NEWPATH="export PATH=$COREUTILS:$LLVM:$GNUBIN:\$PATH"

update_profile() {
    local profile_path=$1
    grep -qxF "$NEWPATH" "$profile_path" 2>/dev/null \
        || printf '%s\n' "$NEWPATH" >> "$profile_path"
    if [ -n "${GITHUB_PATH:-}" ]; then
        printf '%s\n' "$NEWPATH" >> "$GITHUB_PATH"
    fi
}

# PATH munging writes to the user's shell profiles — a mutation. It must NOT
# run in check-deps mode, and must only be PRINTED (not run) under dry-run.
if [ "${CHECK_DEPS:-0}" = 1 ]; then
    :   # read-only check: leave shell profiles untouched
elif [ "${DRY_RUN:-0}" = 1 ]; then
    _dry_line macos "append to ~/.bash_profile and ~/.zshrc:  $NEWPATH"
else
    [ -f "$HOME/.bash_profile" ] && update_profile "$HOME/.bash_profile"
    [ -f "$HOME/.zshrc" ]        && update_profile "$HOME/.zshrc"
fi
true
