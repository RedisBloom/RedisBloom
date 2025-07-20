#!/bin/bash

set -e  # Exit on any error


if ! which brew &> /dev/null; then
    echo "Brew is not installed. Install from https://brew.sh"
    exit 1
fi

export HOMEBREW_NO_AUTO_UPDATE=1

LLVM_VERSION="18"

echo "Installing dependencies via Homebrew..."
brew update
brew install coreutils
brew install make
brew install openssl
brew install llvm@$LLVM_VERSION

BREW_PREFIX=$(brew --prefix)
GNUBIN=$BREW_PREFIX/opt/make/libexec/gnubin
LLVM="$BREW_PREFIX/opt/llvm@$LLVM_VERSION/bin"
COREUTILS=$BREW_PREFIX/opt/coreutils/libexec/gnubin

# Verify that the required tools are installed
if [[ ! -d "$LLVM" ]]; then
    echo "Error: LLVM $LLVM_VERSION not found at $LLVM"
    exit 1
fi

if [[ ! -d "$COREUTILS" ]]; then
    echo "Error: coreutils not found at $COREUTILS"
    exit 1
fi

if [[ ! -d "$GNUBIN" ]]; then
    echo "Error: GNU make not found at $GNUBIN"
    exit 1
fi

update_profile() {
    local profile_path=$1
    local newpath="export PATH=$COREUTILS:$LLVM:$GNUBIN:\$PATH"
    grep -qxF "$newpath" "$profile_path" || echo "$newpath" >> "$profile_path"
    echo $newpath
    # Only source if we're in an interactive shell
    if [[ -t 0 ]]; then
        source $profile_path
    fi
    # Only update GITHUB_PATH if we're in a GitHub Actions environment
    if [[ -n "$GITHUB_PATH" ]]; then
        echo "export PATH=$COREUTILS:$LLVM:$GNUBIN:\$PATH" >> $GITHUB_PATH
    fi
}

# Update profile files if they exist
[[ -f ~/.bash_profile ]] && update_profile ~/.bash_profile
[[ -f ~/.zshrc ]] && update_profile ~/.zshrc

# Export PATH for current session
export PATH=$COREUTILS:$LLVM:$GNUBIN:$PATH

echo "Setup complete! The following tools are now available:"
echo "  - LLVM $LLVM_VERSION: $LLVM"
echo "  - GNU coreutils: $COREUTILS"
echo "  - GNU make: $GNUBIN"
echo ""
echo "PATH has been updated for this session and saved to your shell profile."
