"""Reserve benchmark cores on the disposable nightly AWS hosts (run as root)."""

from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import shlex
import shutil
import socket
import subprocess
import sys
import time


def cpus(value):
    result = set()
    for part in value.strip().split(','):
        first, _, last = part.partition('-')
        result.update(range(int(first), int(last or first) + 1))
    return result


def select_cores(allowed, siblings, count, excluded=()):
    groups = sorted({tuple(sorted(siblings[cpu])) for cpu in allowed})
    # Leave the first physical core for housekeeping; reserve SMT siblings too.
    chosen = [group for group in groups[1:] if not set(group).intersection(excluded)][:count]
    if len(chosen) != count:
        raise RuntimeError('Not enough physical cores for benchmark and housekeeping')
    selected = [min(set(group) & allowed) for group in chosen]
    reserved = set().union(*map(set, chosen))
    return selected, reserved


def validate_client(path):
    if not path:
        raise RuntimeError('Benchmark client not found in PATH; install the client tools before configuring affinity')
    path = Path(path).resolve(strict=True)
    if not path.is_file() or not os.access(path, os.X_OK):
        raise RuntimeError(f'Benchmark client must be an executable file: {path}')
    return path


def wrap_client(path, selected):
    path = validate_client(path)
    original = path.with_name(path.name + '.redisbloom-original')
    if original.exists():
        raise RuntimeError(f'Client already wrapped: {path}')
    taskset = shutil.which('taskset')
    if not taskset:
        raise RuntimeError('taskset is required')
    wrapper = '#!/bin/sh\nexec ' + shlex.join([
        taskset, '-c', ','.join(map(str, selected)), str(original)
    ]) + ' "$@"\n'
    path.rename(original)
    path.write_text(wrapper)
    path.chmod(0o755)
    return str(path)


def debug(event, **data):
    with open('/tmp/redisbloom-affinity-debug.jsonl', 'a') as log:
        log.write(json.dumps(dict(event=event, monotonic=time.monotonic(), **data)) + '\n')


def snapshot(event, targets, effective=None, **data):
    effective = effective if effective is not None else {irq: (Path('/proc/irq') / irq / 'effective_affinity_list').read_text().strip()
                 for irq in targets}
    interrupts = [line for line in Path('/proc/interrupts').read_text().splitlines()
                  if line.split(':')[0].strip() in targets]
    debug(event, requested=targets, effective=effective, interrupts=interrupts, **data)


def network_probe(peer):
    # Different source ports exercise multiple receive queues; no SSH authentication.
    def connect(_):
        try:
            with socket.create_connection((peer, 22), timeout=1) as connection:
                connection.recv(256)
            return 'ok'
        except OSError as error:
            return type(error).__name__ + ': ' + str(error)

    debug('probe_start', peer=peer, connections=128)
    with ThreadPoolExecutor(max_workers=16) as pool:
        results = list(pool.map(connect, range(128)))
    debug('probe_end', results={result: results.count(result) for result in set(results)})


def select_isolated_cores(targets, allowed, siblings, count, peer):
    # Keep the original quiet wait free of probe traffic and periodic diagnostics.
    for phase in ('quiet', 'after_probe'):
        if phase == 'after_probe':
            snapshot('before_probe', targets)
            network_probe(peer)
            snapshot('after_probe', targets)
        start = time.monotonic()
        deadline = start + 30
        while True:
            effective = {irq: (Path('/proc/irq') / irq / 'effective_affinity_list').read_text().strip()
                         for irq in targets}
            # Exclude current and requested destinations, including their SMT siblings.
            excluded = set(targets.values()).union(*(cpus(value) for value in effective.values()))
            try:
                selected, reserved = select_cores(allowed, siblings, count, excluded)
            except RuntimeError:
                if time.monotonic() >= deadline:
                    snapshot('timeout', targets, effective=effective, phase=phase,
                             elapsed=time.monotonic() - start)
                    break
            else:
                snapshot('selected', targets, effective=effective, phase=phase,
                         elapsed=time.monotonic() - start, selected=selected, reserved=sorted(reserved))
                return selected, reserved, effective
            time.sleep(0.1)
    raise RuntimeError(f'Not enough IRQ-free physical cores after quiet wait and network probe; '
                       f'requested={targets}, effective={effective}, allowed={sorted(allowed)}')


def configure(role, peer, clients):
    Path('/tmp/redisbloom-affinity-debug.jsonl').write_text('')
    # Validate every client before changing IRQ settings or wrapping either tool.
    clients = [validate_client(path) for path in clients]
    allowed = set(os.sched_getaffinity(0))
    siblings = {cpu: cpus(Path(f'/sys/devices/system/cpu/cpu{cpu}/topology/thread_siblings_list').read_text())
                for cpu in allowed}
    groups = sorted({tuple(sorted(siblings[cpu])) for cpu in allowed})
    # Keep IRQ destinations on two physical cores, leaving room to choose
    # benchmark cores around any interrupt migrations that remain pending.
    housekeeping = [min(set(group) & allowed) for group in groups[:2]]
    interfaces = [path for path in Path('/sys/class/net').iterdir()
                  if (path / 'device/msi_irqs').is_dir()]
    if not interfaces:
        raise RuntimeError('No NIC MSI interrupts found; cannot verify isolation')
    debug('host', role=role, peer=peer, kernel=os.uname().release,
          allowed=sorted(allowed), sibling_groups=groups,
          interfaces=[path.name for path in interfaces])
    before = {irq.name: (Path('/proc/irq') / irq.name / 'smp_affinity_list').read_text().strip()
              for interface in interfaces for irq in (interface / 'device/msi_irqs').iterdir()}
    snapshot('before_writes', before)
    active = subprocess.run(['systemctl', 'is-active', '--quiet', 'irqbalance']).returncode == 0
    if active:
        subprocess.run(['systemctl', 'stop', 'irqbalance'], check=True)
    irqs = {}
    for interface in interfaces:
        for index, irq in enumerate(sorted((interface / 'device/msi_irqs').iterdir())):
            directory = Path('/proc/irq') / irq.name
            target = housekeeping[index % len(housekeeping)]
            (directory / 'smp_affinity_list').write_text(str(target))
            requested = (directory / 'smp_affinity_list').read_text().strip()
            if cpus(requested) != {target}:
                raise RuntimeError(f'IRQ {irq.name} rejected CPU {target}: requested={requested}')
            irqs[irq.name] = target
        # Disable software packet steering, which can otherwise bypass IRQ affinity.
        for rps in (interface / 'queues').glob('rx-*/rps_cpus'):
            rps.write_text('0')
            if int(rps.read_text().strip().replace(',', ''), 16):
                raise RuntimeError(f'RPS still enabled: {rps}')
    selected, reserved, effective = select_isolated_cores(
        irqs, allowed, siblings, 2, peer)
    data = dict(role=role, cpulist=','.join(map(str, selected)),
                reserved_cpus=sorted(reserved), nic_irqs=effective, requested_nic_irqs=irqs,
                irqbalance_stopped=active,
                clients=[wrap_client(path, selected) for path in clients])
    Path('/tmp/redisbloom-affinity.json').write_text(json.dumps(data))
    return data


if __name__ == '__main__':
    print(json.dumps(configure(sys.argv[1], sys.argv[2], sys.argv[3:])))
