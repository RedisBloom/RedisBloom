
import sys
import time
import os
from RLTest import Env, Defaults
from redis import ResponseError
from packaging import version


#if sys.version_info > (3, 0):
#    Defaults.decode_responses = True

try:
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), "../../deps/readies"))
    import paella
except:
    pass

ONLY_STABLE = os.getenv('ONLY_STABLE', '0') == '1'
SANITIZER = os.getenv('SANITIZER', '')
VALGRIND = os.getenv('VALGRIND', '0') == '1'
CODE_COVERAGE = os.getenv('CODE_COVERAGE', '0') == '1'

OSNICK = paella.Platform().osnick
OS = paella.Platform().os
ARCH = paella.Platform().arch


def numver_to_version(numver):
    v = numver
    v = "%d.%d.%d" % (int(v/10000), int(v/100)%100, v%100)
    return version.parse(v)

module_ver = None
def module_version_at_least(env, ver):
    global module_ver
    if module_ver is None:
        v = env.execute_command('MODULE LIST')[0][3]
        module_ver = numver_to_version(v)
    if not isinstance(ver, version.Version):
        ver = version.parse(ver)
    return module_ver >= ver

def module_version_less_than(env, ver):
    return not module_version_at_least(env, ver)

server_ver = None
def server_version_at_least(env, ver):
    global server_ver
    if server_ver is None:
        v = env.execute_command('INFO')['redis_version']
        server_ver = version.parse(v)
    if not isinstance(ver, version.Version):
        ver = version.parse(ver)
    return server_ver >= ver

def server_version_less_than(env, ver):
    return not server_version_at_least(env, ver)

def wait_for_no_bgsave(env, timeout=60):
    # Redis < 7 defaults repl-diskless-sync to no, so attaching a replica triggers a
    # disk-backed BGSAVE. While that fork is alive, SAVE and DEBUG RELOAD fail with
    # "Background save already in progress" -- and under a sanitizer the fork is slow
    # enough that tests reliably race it.
    deadline = time.time() + timeout
    while time.time() < deadline:
        if env.execute_command('INFO', 'persistence')['rdb_bgsave_in_progress'] == 0:
            return
        time.sleep(0.1)
    raise Exception('timed out waiting for BGSAVE to finish')

def dump_and_reload(env, **kwargs):
    # RLTest's dumpAndReload() issues SAVE before DEBUG RELOAD NOSAVE, so every call site is
    # exposed to the BGSAVE race above. Route them all through here rather than guarding the
    # ones that happen to lose it.
    wait_for_no_bgsave(env)
    env.dumpAndReload(**kwargs)
