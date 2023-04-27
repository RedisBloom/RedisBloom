#!/usr/bin/env python3

import sys
import os
import argparse

HERE = os.path.abspath(os.path.dirname(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
READIES = os.path.join(ROOT, "deps/readies")
sys.path.insert(0, READIES)
import paella

#----------------------------------------------------------------------------------------------

class RedisBloomSetup(paella.Setup):
    def __init__(self, args):
        paella.Setup.__init__(self, args.nop)

    def common_first(self):
        self.install_downloaders()
        self.run(f"{READIES}/bin/enable-utf8", sudo=self.os != 'macos')
        self.install("git jq")

    def debian_compat(self):
        self.run(f"{READIES}/bin/getgcc")

    def redhat_compat(self):
        self.install("which")
        self.run(f"{READIES}/bin/getepel")
        self.run(f"{READIES}/bin/getgcc --modern")

    def linux_last(self):
        self.install("valgrind")

    def macos(self):
        self.install_gnu_utils()
        self.run(f"{READIES}/bin/getredis")

    def common_last(self):
        if self.dist == "arch":
            self.install("lcov-git", aur=True)
        else:
            self.install("lcov")
        self.run(f"{ROOT}/sbin/get-fbinfer")
        self.run(f"{self.python} {READIES}/bin/getrmpytools --reinstall --modern --redispy-version=v5.0.0b1 --rltest-version=github:rafi-resp3-1 --ramp-version=github:rafi-resp3-1")
        self.run(f"{self.python} {READIES}/bin/getcmake --usr")
        self.pip_install("-r tests/flow/requirements.txt")
        self.run(f"{READIES}/bin/getaws")
        self.pip_install("pudb")

#----------------------------------------------------------------------------------------------

parser = argparse.ArgumentParser(description='Set up system for build.')
parser.add_argument('-n', '--nop', action="store_true", help='no operation')
args = parser.parse_args()

RedisBloomSetup(args).setup()
