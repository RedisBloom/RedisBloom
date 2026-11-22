"""Reserve benchmark cores on the disposable nightly AWS hosts (run as root)."""

import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import time


def debug(event, **data):
    # stdout is explicitly forwarded by compare_aws, even on remote failure.
    print('AFFINITY_DEBUG ' + json.dumps(dict(event=event, monotonic=time.monotonic(), **data)), flush=True)


def debug_read(path):
    try:
        return Path(path).read_text().strip()
    except OSError as error:
        return f'unavailable: {error}'


def debug_irqs(event, targets, **data):
    interrupts = debug_read('/proc/interrupts').splitlines()
    debug(event, targets=targets, irqs={irq: {
        name: debug_read(f'/proc/irq/{irq}/{name}')
        for name in ('smp_affinity_list', 'effective_affinity_list', 'affinity_hint')
    } for irq in targets}, interrupts=[line for line in interrupts
        if ':' not in line or line.split(':', 1)[0].strip() in targets], **data)


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


def select_isolated_cores(targets, allowed, siblings, count):
    # An idle IRQ can retain its old effective CPU until its next interrupt.
    # Exclude BOTH current and requested CPUs, including their SMT siblings,
    # so a later migration cannot enter the selected benchmark cores.
    deadline = time.monotonic() + 10
    next_debug = 0
    while True:
        effective = {irq: (Path('/proc/irq') / irq / 'effective_affinity_list').read_text().strip()
                     for irq in targets}
        excluded = set(targets.values()).union(*(cpus(value) for value in effective.values()))
        now = time.monotonic()
        if now >= next_debug:
            debug_irqs('waiting', targets, excluded=sorted(excluded),
                       eligible_groups=[list(group) for group in sorted({tuple(sorted(siblings[cpu]))
                           for cpu in allowed})[1:] if not set(group).intersection(excluded)])
            next_debug = now + 1
        try:
            selected, reserved = select_cores(allowed, siblings, count, excluded)
            debug_irqs('selected', targets, selected=selected, reserved=sorted(reserved))
            return selected, reserved, effective
        except RuntimeError:
            if time.monotonic() >= deadline:
                debug_irqs('timeout', targets, excluded=sorted(excluded),
                           kernel_irq_state={irq: debug_read(f'/sys/kernel/debug/irq/irqs/{irq}')
                                             for irq in targets},
                           irqbalance_active=subprocess.run(['systemctl', 'is-active', '--quiet',
                                                             'irqbalance']).returncode == 0)
                raise RuntimeError(f'Not enough IRQ-free physical cores; requested={targets}, '
                                   f'effective={effective}, allowed={sorted(allowed)}') from None
        time.sleep(0.1)


def configure(role, clients):
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
    active = subprocess.run(['systemctl', 'is-active', '--quiet', 'irqbalance']).returncode == 0
    if active:
        subprocess.run(['systemctl', 'stop', 'irqbalance'], check=True)
    debug('host', role=role, kernel=os.uname().release, allowed=sorted(allowed),
          sibling_groups=groups, housekeeping=housekeeping, irqbalance_was_active=active,
          irqbalance_active=subprocess.run(['systemctl', 'is-active', '--quiet', 'irqbalance']).returncode == 0,
          interfaces=[dict(name=interface.name, driver=str((interface / 'device/driver').resolve()))
                      for interface in interfaces])
    targets = {irq.name: housekeeping[index % len(housekeeping)]
               for interface in interfaces
               for index, irq in enumerate(sorted((interface / 'device/msi_irqs').iterdir()))}
    debug_irqs('before', targets)
    irqs = {}
    for interface in interfaces:
        for index, irq in enumerate(sorted((interface / 'device/msi_irqs').iterdir())):
            directory = Path('/proc/irq') / irq.name
            target = housekeeping[index % len(housekeeping)]
            try:
                (directory / 'smp_affinity_list').write_text(str(target))
            except OSError as error:
                debug_irqs('write_failed', targets, irq=irq.name, error=str(error))
                raise
            requested = (directory / 'smp_affinity_list').read_text().strip()
            if cpus(requested) != {target}:
                debug_irqs('write_rejected', targets, irq=irq.name)
                raise RuntimeError(f'IRQ {irq.name} rejected CPU {target}: requested={requested}')
            irqs[irq.name] = target
        # Disable software packet steering, which can otherwise bypass IRQ affinity.
        for rps in (interface / 'queues').glob('rx-*/rps_cpus'):
            rps.write_text('0')
            if int(rps.read_text().strip().replace(',', ''), 16):
                raise RuntimeError(f'RPS still enabled: {rps}')
    debug_irqs('after_writes', irqs)
    selected, reserved, effective = select_isolated_cores(
        irqs, allowed, siblings, 2)
    data = dict(role=role, cpulist=','.join(map(str, selected)),
                reserved_cpus=sorted(reserved), nic_irqs=effective, requested_nic_irqs=irqs,
                irqbalance_stopped=active,
                clients=[wrap_client(path, selected) for path in clients])
    Path('/tmp/redisbloom-affinity.json').write_text(json.dumps(data))
    return data


if __name__ == '__main__':
    print(json.dumps(configure(sys.argv[1], sys.argv[2:])))
