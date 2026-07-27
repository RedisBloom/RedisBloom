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

# python@3.11 only when the host has no python3 (see header). Skip under list;
# print (don't run) under dry-run.
if command -v python3 >/dev/null 2>&1; then
    # present — record for the list count (macOS python isn't in BREW_BASE)
    if [ "${CHECK_DEPS:-0}" = 1 ]; then DEPS_OK="$DEPS_OK python@3.11"; fi
elif [ "${CHECK_DEPS:-0}" = 1 ]; then
    # list: record as missing so it matches dry-run and a real install
    DEPS_MISSING="$DEPS_MISSING python@3.11"
elif [ "${DRY_RUN:-0}" = 1 ]; then
    _dry_line "HOMEBREW_NO_AUTO_UPDATE=1 brew install python@3.11"
else
    HOMEBREW_NO_AUTO_UPDATE=1 _run brew install python@3.11
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

# PATH munging writes to the user's shell profiles — a mutation. Skip it in
# list/dry-run mode.
if [ "${CHECK_DEPS:-0}" != 1 ] && [ "${DRY_RUN:-0}" != 1 ]; then
    [ -f "$HOME/.bash_profile" ] && update_profile "$HOME/.bash_profile"
    [ -f "$HOME/.zshrc" ]        && update_profile "$HOME/.zshrc"
fi
true
