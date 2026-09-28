"""Compare master/default/random probabilistic sketch RPS using isolated Redis processes.

Requires redis-py and matching release builds. Emits raw samples and medians as JSON.
"""
import argparse
import csv
import json
import socket
import statistics
import subprocess
import tempfile
import time
from pathlib import Path

import redis


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('server', 'benchmark', 'master', 'branch'):
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--rounds', type=int, default=5)
    parser.add_argument('--requests', type=int, default=500000)
    parser.add_argument('--unix-socket', action='store_true', help='Avoid local TCP overhead')
    parser.add_argument('--filter', choices=('BF', 'CF', 'CMS', 'TOPK'), default='BF')
    args = parser.parse_args()
    assert args.rounds > 0 and 0 < args.requests <= 1000000
    capacity = {'BF': 1000000, 'CF': 10000000, 'CMS': None, 'TOPK': None}[args.filter]
    prefill = 100 if args.filter == 'TOPK' else 500000
    operations = ('incrby', 'query-present', 'query-absent') if args.filter == 'CMS' else (
        'add', 'exists-hit', 'exists-miss')
    read_command = 'CMS.QUERY' if args.filter == 'CMS' else args.filter + '.EXISTS'
    if args.filter == 'TOPK':
        operations = ('incrby', 'query-present', 'query-absent', 'count-present', 'count-absent')
        read_command = 'TOPK.QUERY'
    samples = []
    modes = ['master', 'default', 'random']
    print(json.dumps({'config': vars(args), 'clients': 16, 'capacity': capacity,
                      'error_rate': 0.001 if args.filter in ('BF', 'CMS') else None,
                      'probability': 0.01 if args.filter == 'CMS' else None,
                      'prefill': prefill}), flush=True)
    for repeat in range(args.rounds):
        # Rotate modes so the branch is not always measured after master.
        for mode in modes[repeat % 3:] + modes[:repeat % 3]:
            module = args.master if mode == 'master' else args.branch
            with tempfile.TemporaryDirectory(prefix='filter-rps-') as directory:
                with socket.socket() as sock:
                    sock.bind(('127.0.0.1', 0))
                    port = sock.getsockname()[1]
                with open(Path(directory) / 'server.log', 'w+') as log:
                    socket_path = str(Path(directory) / 'redis.sock')
                    transport = ['--unixsocket', socket_path] if args.unix_socket else []
                    process = subprocess.Popen([
                        args.server, '--bind', '127.0.0.1', '--port', '0' if args.unix_socket else str(port),
                        '--save', '', '--appendonly', 'no', '--dir', directory,
                        '--loadmodule', str(Path(module).resolve()), *transport], stdout=log, stderr=log)
                    client = redis.Redis(unix_socket_path=socket_path, socket_timeout=5) if args.unix_socket else redis.Redis(port=port, socket_timeout=5)
                    try:
                        for _ in range(100):
                            if process.poll() is not None:
                                log.seek(0)
                                raise RuntimeError(log.read())
                            try:
                                if client.ping():
                                    break
                            except redis.ConnectionError:
                                time.sleep(0.05)
                        else:
                            raise RuntimeError('Redis did not start')

                        def reserve(key):
                            options = ['SEED', 'random'] if mode == 'random' else []
                            dimensions = [0.001, capacity] if args.filter == 'BF' else [capacity]
                            command = args.filter + '.RESERVE'
                            if args.filter == 'CMS':
                                command, dimensions = 'CMS.INITBYPROB', [0.001, 0.01]
                            elif args.filter == 'TOPK':
                                dimensions = [100]
                            assert client.execute_command(
                                command, key, *dimensions, *options) == b'OK'

                        reserve('read')
                        for start in range(0, prefill, 1000):
                            command = ['BF.MADD', 'read'] if args.filter == 'BF' else [
                                'CF.INSERT', 'read', 'NOCREATE', 'ITEMS']
                            items = [f'item:{i:012d}' for i in range(start, min(start + 1000, prefill))]
                            item_count = len(items)
                            if args.filter == 'CMS':
                                command = ['CMS.INCRBY', 'read']
                                items = [arg for item in items for arg in (item, 1)]
                            elif args.filter == 'TOPK':
                                command = ['TOPK.ADD', 'read']
                            added = client.execute_command(*command, *items)
                            assert len(added) == item_count
                            if args.filter == 'CF':
                                assert all(result == 1 for result in added)
                        present = client.execute_command(read_command, 'read', 'item:000000000042')
                        assert present[0] >= 1 if args.filter in ('CMS', 'TOPK') else present == 1
                        if args.filter == 'TOPK':
                            expected_items = {f'item:{i:012d}'.encode() for i in range(prefill)}
                            assert set(client.execute_command('TOPK.LIST', 'read')) == expected_items
                            assert client.execute_command('TOPK.QUERY', 'read', *sorted(expected_items)) == [1] * prefill

                        def bench(command, pipeline, count, keyspace):
                            result = subprocess.run([
                                args.benchmark, *(['-s', socket_path] if args.unix_socket else
                                                 ['-h', '127.0.0.1', '-p', str(port)]),
                                '-c', '16', '-n', str(count), '-P', str(pipeline),
                                '-r', str(keyspace), '--seed', '12345', '--csv',
                                *command], capture_output=True, text=True, check=True)
                            assert 'Error' not in result.stdout + result.stderr, result
                            rows = list(csv.reader(result.stdout.splitlines()))
                            rps = float(rows[-1][1])
                            assert rps > 0, 'No measurable throughput; increase --requests'
                            return rps

                        # Warm the server/client path before measuring.
                        bench([read_command, 'read', 'item:__rand_int__'], 16, 50000, prefill)
                        for pipeline in (1, 32):
                            for operation in operations:
                                if operation == operations[0]:
                                    client.delete('write')
                                    reserve('write')
                                    command = [args.filter + '.ADD', 'write', 'item:__rand_int__']
                                    if args.filter in ('CMS', 'TOPK'):
                                        command = [args.filter + '.INCRBY', 'write', 'item:__rand_int__', '1']
                                    keyspace = 1000000000
                                else:
                                    prefix = 'item' if operation in (operations[1], 'count-present') else 'miss'
                                    query = 'TOPK.COUNT' if operation.startswith('count-') else read_command
                                    command = [query, 'read', prefix + ':__rand_int__']
                                    keyspace = prefill
                                before = client.info('cpu')
                                if args.filter == 'TOPK':
                                    stats_before = client.info('commandstats').get('cmdstat_' + command[0].lower(), {}).get('calls', 0)
                                rps = bench(command, pipeline, args.requests, keyspace)
                                after = client.info('cpu')
                                cpu = sum(after[k] - before[k] for k in
                                          ('used_cpu_user', 'used_cpu_sys'))
                                if args.filter == 'CF':
                                    info = client.execute_command('CF.INFO', command[1])
                                    info = dict(zip(info[::2], info[1::2]))
                                    assert info[b'Number of filters'] == 1, info
                                    expected = args.requests if operation == 'add' else 500000
                                    assert info[b'Number of items inserted'] == expected, info
                                if args.filter == 'CMS':
                                    info = client.execute_command('CMS.INFO', command[1])
                                    info = dict(zip(info[::2], info[1::2]))
                                    expected = args.requests if operation == 'incrby' else 500000
                                    assert info[b'count'] == expected, info
                                    assert (info[b'width'], info[b'depth'], info[b'cell_size']) == (2000, 7, 4), info
                                if args.filter == 'TOPK':
                                    stats = client.info('commandstats')['cmdstat_' + command[0].lower()]
                                    assert stats['calls'] - stats_before == args.requests, stats
                                    assert stats['failed_calls'] == stats['rejected_calls'] == 0, stats
                                    info = client.execute_command('TOPK.INFO', command[1])
                                    info = dict(zip(info[::2], info[1::2]))
                                    assert (info[b'k'], info[b'width'], info[b'depth'], float(info[b'decay'])) == (100, 8, 7, 0.9), info
                                    members = client.execute_command('TOPK.LIST', command[1])
                                    assert len(set(members)) == 100
                                    if operation != 'incrby':
                                        assert set(members) == expected_items
                                sample = dict(mode=mode, repeat=repeat + 1, operation=operation,
                                              pipeline=pipeline, rps=rps,
                                              cpu_us_per_request=cpu * 1e6 / args.requests)
                                samples.append(sample)
                                print(json.dumps(sample), flush=True)
                    finally:
                        client.close()
                        process.terminate()
                        try:
                            process.wait(timeout=10)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.wait()
    for pipeline in (1, 32):
        for operation in operations:
            group = [s for s in samples if s['pipeline'] == pipeline and s['operation'] == operation]
            medians = {m: statistics.median(s['rps'] for s in group if s['mode'] == m) for m in modes}
            print(json.dumps({'summary': operation, 'pipeline': pipeline, 'median_rps': medians,
                              'change_pct': {m: 100 * (medians[m] / medians['master'] - 1)
                                             for m in modes[1:]}}), flush=True)


if __name__ == '__main__':
    main()
