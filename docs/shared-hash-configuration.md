# Shared hashing configuration for OSS Cluster

RedisBloom can use one explicitly provisioned seed for Bloom filters, Cuckoo
filters, Count-Min sketches and Top-K sketches. All cluster nodes, including
replicas and future nodes, must use the same configuration. Merge compatibility
then remains independent of the node where a sketch was created.

Seeded mode requires Redis core's `RedisModule_SetReplicationCompatibility` API
and fails to load without it. HLL, Enterprise provisioning and the Enterprise
Replica Of syncer are outside this implementation.

## Provisioning

Generate a seed **once** for the deployment, for example with `openssl rand -hex 8`.
Install that same value in every node's Redis configuration:

```conf
loadmodule /path/to/redisbloom.so
bf-hash-seed <the-same-16-hex-digits-on-every-node>
```

Do not generate a seed independently per node or at every restart. Back up the
configuration with the data. The setting is immutable at runtime; seeded
RedisBloom must be loaded at startup, not through `MODULE LOAD`.

An empty seed (the default) keeps legacy MurmurHash mappings. An explicit seed,
including `0000000000000000`, selects mapping version 1: XXH3-64 with derived seeds
for Bloom's two hashes and CMS/Top-K rows. CMS and Top-K retain 32-bit hash and
fingerprint outputs. The vendored xxHash version is 0.8.3.

XXH3 is non-cryptographic. Randomizing the mapping is hardening, not a claim of
cryptographic protection against adaptive accuracy attacks. No command-throughput
improvement is claimed without workload benchmarks.

## Replication and migration

Redis compares a canonical SHA-256 fingerprint of algorithm, version and seed
before full or partial replication and before ASM exports data. Mismatched seeds,
seeded versus legacy mode, a missing module, and unsupported peers fail closed.
Reconnection repeats the check; the separate RDB channel inherits validation from
the main channel.

`INFO modules` exposes `module_replication_compatibility` for comparing nodes.
This is an aggregate fingerprint of all registered module configurations, not a
raw seed. A mismatch produces `MODULECONFIG`. Align persisted configurations and
restart the affected node before retrying. A rejected ASM import leaves source
ownership intact; replication rejection does not flush the destination.

The handshake checks compatibility, not authentication. Seeds are retained in
configuration and serialized sketch data and are available to users who can read
those resources. The feature does not distribute configuration automatically or
change hash-slot restrictions on multi-key commands.

## Persistence and existing data

Each sketch stores its mapping version and seed. RDB loading, `RESTORE`, and
`BF.LOADCHUNK`/`CF.LOADCHUNK` reject incompatible configurations. CMS also checks
compatibility before modifying a merge destination. Rename preserves the mapping.

Old RDB records and chunk headers represent legacy mode and load only in legacy
mode. New RDB records use newer encoding versions: older RedisBloom versions
cannot read them, even for legacy-mode data. Legacy chunk headers retain their
wire representation; seeded headers append 16 bytes of version and seed metadata.

Unrewritten AOFs include a `BF._HASHCHECK` assertion before sketch creation. A
mismatch during AOF loading stops startup because AOF replay otherwise discards
command errors. Rewritten AOFs preserve configuration in serialized objects.
`BF._HASHCHECK` is an implementation command, not an application interface.

Changing the setting cannot rehash existing sketches. Rebuild from original
inputs into a fresh, consistently configured deployment. Historical command-only
AOFs without configuration metadata cannot identify their original mapping;
restore those in legacy mode and rebuild explicitly.

## Validation

Build the matching Redis core change and RedisBloom, then run:

```sh
REDIS_SERVER=/path/to/patched/redis-server \
REDISBLOOM_MODULE=/path/to/redisbloom.so \
python -m unittest discover -s tests/qa -p test_shared_hash.py -v
```

Requires redis-py. Covers persistence, incompatible imports, AOF replay and
rewrite, replication, promotion, configuration parsing, and ASM followed by CMS
merge on the destination. Redis core also has independent module-API and cluster
tests for its compatibility handshake.
