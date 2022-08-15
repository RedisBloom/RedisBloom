#!/usr/bin/env python3

import sys
import os
import argparse

ROOT = HERE = os.path.abspath(os.path.dirname(__file__))
READIES = os.path.join(ROOT, "deps/readies")
sys.path.insert(0, READIES)
import paella

#----------------------------------------------------------------------------------------------

class RedisTimeSeriesSetup(paella.Setup):
    def __init__(self, nop=False):
        paella.Setup.__init__(self, nop)

    def common_first(self):
        self.pip_install("wheel")
        self.pip_install("setuptools --upgrade")

        self.install("git jq curl")

    def debian_compat(self):
        if self.osnick == 'buster':
            self.run("%s/bin/getgcc" % READIES)
        else:
            self.run("%s/bin/getgcc --modern" % READIES)

    def redhat_compat(self):
        self.run("%s/bin/getepel" % READIES)
        self.run("%s/bin/getgcc --modern" % READIES)

    def macos(self):
        self.run("%s/bin/getredis" % READIES)

    def common_last(self):
        if self.dist in ["centos", 'ol'] and self.ver == "8":
            self.install("https://pkgs.dyn.su/el8/base/x86_64/lcov-1.14-3.el8.noarch.rpm")
        elif self.dist == "arch":
            self.install("lcov-git", aur=True)
        else:
            self.install("lcov")
        self.run("python3 {READIES}/bin/getcmake".format(READIES=READIES))
        self.pip_install("-r tests/flow/requirements.txt")

#----------------------------------------------------------------------------------------------

parser = argparse.ArgumentParser(description='Set up system for build.')
parser.add_argument('-n', '--nop', action="store_true", help='no operation')
args = parser.parse_args()

RedisTimeSeriesSetup(nop = args.nop).setup()
