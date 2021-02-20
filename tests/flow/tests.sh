#!/bin/bash

[[ $VERBOSE == 1 ]] && set -x
[[ $IGNERR == 1 ]] || set -e

error() {
	echo "There are errors:"
	gawk 'NR>L-4 && NR<L+4 { printf "%-5d%4s%s\n",NR,(NR==L?">>> ":""),$0 }' L=$1 $0
	exit 1
}

[[ -z $_Dbg_DEBUGGER_LEVEL ]] && trap 'error $LINENO' ERR

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
. $HERE/../../deps/readies/shibumi/functions

export ROOT=$(realpath $HERE/../..)

#----------------------------------------------------------------------------------------------

help() {
	cat <<-END
		Run Python tests.

		[ARGVARS...] tests.sh [--help|help] [<module-so-path>]

		Argument variables:
		VERBOSE=1     Print commands
		IGNERR=1      Do not abort on error

		GEN=0|1         General tests
		AOF=0|1         Tests with --test-aof
		SLAVES=0|1      Tests with --test-slaves
		CLUSTER=0|1     Tests with --env oss-cluster

		TEST=test        Run specific test (e.g. test.py:test_name)
		VALGRIND|VGD=1   Run with Valgrind
		CALLGRIND|CGD=1  Run with Callgrind

	END
}

#----------------------------------------------------------------------------------------------

check_redis_server() {
	if ! command -v redis-server >/dev/null; then
		echo "Cannot find redis-server. Aborting."
		exit 1
	fi
}

#----------------------------------------------------------------------------------------------

valgrind_config() {
	export VG_OPTIONS="
		-q \
		--leak-check=full \
		--show-reachable=no \
		--track-origins=yes \
		--show-possibly-lost=no"

	# To generate supressions and/or log to file
	# --gen-suppressions=all --log-file=valgrind.log

	VALGRIND_SUPRESSIONS=$ROOT/tests/redis_valgrind.sup

	TEST_ARGS+="\
		--no-output-catch \
		--use-valgrind \
		--vg-verbose \
		--vg-suppressions $VALGRIND_SUPRESSIONS"

}

#----------------------------------------------------------------------------------------------

run_tests() {
	local title="$1"
	[[ ! -z $title ]] && {
		$ROOT/deps/readies/bin/sep -0
		printf "Tests with $title:\n\n"
	}
	cd $ROOT/tests/flow
	$OP python3 -m RLTest --clear-logs --module $MODULE $TEST_ARGS
}

#----------------------------------------------------------------------------------------------

[[ $1 == --help || $1 == help ]] && {
	help
	exit 0
}

GEN=${GEN:-1}
SLAVES=${SLAVES:-1}
AOF=${AOF:-1}
CLUSTER=${CLUSTER:-1}

GDB=${GDB:-0}

OP=""
[[ $NOP == 1 ]] && OP="echo"

MODULE=${MODULE:-$1}
[[ -z $MODULE || ! -f $MODULE ]] && {
	echo "Module not found at ${MODULE}. Aborting."
	exit 1
}

[[ $VALGRIND == 1 || $VGD == 1 ]] && valgrind_config

if [[ ! -z $TEST ]]; then
	TEST_ARGS+=" --test $TEST -s"
	export PYDEBUG=${PYDEBUG:-1}
fi

[[ $VERBOSE == 1 ]] && TEST_ARGS+=" -v"
[[ $GDB == 1 ]] && TEST_ARGS+=" -i --verbose"

#----------------------------------------------------------------------------------------------

cd $ROOT/tests/flow

check_redis_server

[[ $GEN == 1 ]] && run_tests
[[ $CLUSTER == 1 ]] && TEST_ARGS+=" --env oss-cluster --shards-count 1" run_tests "--env oss-cluster"
[[ $SLAVES == 1 ]] && TEST_ARGS+=" --use-slaves" run_tests "--use-slaves"
[[ $AOF == 1 ]] && TEST_ARGS+=" --use-aof" run_tests "--use-aof"

exit 0
