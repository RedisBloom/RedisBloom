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
        self.run("%s/bin/enable-utf8" % READIES, sudo=self.os != 'macos')
        self.install("git jq")

    def debian_compat(self):
        self.run("%s/bin/getgcc" % READIES)

    def redhat_compat(self):
        self.run("%s/bin/getepel" % READIES)
        self.run("%s/bin/getgcc --modern" % READIES)

    def linux_last(self):
        self.install("valgrind")

    def macos(self):
        self.install_gnu_utils()
        self.run("%s/bin/getredis" % READIES)

    def common_last(self):
        if self.dist == "arch":
            self.install("lcov-git", aur=True)
        else:
            self.install("lcov")
        self.run("{ROOT}/sbin/get-fbinfer".format(ROOT=ROOT))
        self.run("{PYTHON} {READIES}/bin/getrmpytools --reinstall --modern".format(PYTHON=self.python, READIES=READIES))
        self.run("{PYTHON} {READIES}/bin/getcmake --usr".format(PYTHON=self.python, READIES=READIES))
        self.pip_install("-r tests/flow/requirements.txt")
        self.run("{READIES}/bin/getaws".format(READIES=READIES))
        self.pip_install("pudb")

#----------------------------------------------------------------------------------------------

parser = argparse.ArgumentParser(description='Set up system for build.')
parser.add_argument('-n', '--nop', action="store_true", help='no operation')
args = parser.parse_args()

RedisBloomSetup(args).setup()
