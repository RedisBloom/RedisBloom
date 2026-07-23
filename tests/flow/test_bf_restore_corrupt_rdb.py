import struct

from common import *
from rdb_corruption_utils import (
    RDB_ENC_LZF,
    RDB_MODULE_OPCODE_DOUBLE,
    RDB_MODULE_OPCODE_EOF,
    RDB_MODULE_OPCODE_STRING,
    RDB_MODULE_OPCODE_UINT,
    crc64_redis as _crc64_redis,
    encode_len as _encode_len,
    load_len as _load_len,
    lzf_decompress as _lzf_decompress,
    rewrite_largest_module_string,
)

# First malicious RESTORE geometry from the public P88W exploit:
# https://github.com/berabuddies/redis-poc/blob/7540fa3619f849cd16307e612bc34676dfdccf91/P88W_exploit.py
P88W_S_ODD = 216
P88W_C_ODD = 16
P88W_FAKE_CAP = 0x40000000


def _corrupt_dump_set_nth_uint(dump_payload: bytes, n: int, new_value: int) -> bytes:
    # Replace the value of the n-th (1-based) MODULE_OPCODE_UINT in the module
    # stream with `new_value` (unsigned), then fix up the trailing CRC64. Used to
    # forge an attacker-controlled count field that the loader saved via
    # RedisModule_SaveUnsigned.
    if len(dump_payload) < 10:
        raise RuntimeError("DUMP payload too small")
    value = dump_payload[:-10]
    version = dump_payload[-10:-8]

    pos = 1
    _, is_enc, pos, _ = _load_len(value, pos)  # module id
    if is_enc:
        raise RuntimeError("Unexpected encoded module-id length")

    uint_seen = 0
    while pos < len(value):
        opcode, is_enc, pos, _ = _load_len(value, pos)
        if is_enc:
            raise RuntimeError(f"Unexpected encoded opcode at pos={pos}")
        if opcode == RDB_MODULE_OPCODE_EOF:
            break
        if opcode == RDB_MODULE_OPCODE_UINT:
            val_start = pos
            _, is_enc2, pos, _ = _load_len(value, pos)
            if is_enc2:
                raise RuntimeError("Unexpected encoded value in MODULE_OPCODE_UINT")
            val_end = pos
            uint_seen += 1
            if uint_seen == n:
                new_value_bytes = value[:val_start] + _encode_len(new_value) + value[val_end:]
                new_crc = _crc64_redis(new_value_bytes + version)
                return new_value_bytes + version + struct.pack("<Q", new_crc)
            continue
        if opcode == RDB_MODULE_OPCODE_DOUBLE:
            pos += 8
            continue
        if opcode == RDB_MODULE_OPCODE_STRING:
            slen_or_enc, is_str_enc, pos, enc = _load_len(value, pos)
            if not is_str_enc:
                pos += slen_or_enc
            else:
                if enc != RDB_ENC_LZF:
                    raise RuntimeError(f"Unsupported encoded string type: {enc}")
                clen, is_enc3, pos, _ = _load_len(value, pos)
                ulen, is_enc4, pos, _ = _load_len(value, pos)
                if is_enc3 or is_enc4:
                    raise RuntimeError("Unexpected encoded compressed/uncompressed length")
                pos += clen
            continue
        raise RuntimeError(f"Unknown module opcode {opcode} at pos={pos}")

    raise RuntimeError(f"Did not find UINT #{n} to patch")


def _zigzag_encode(n: int) -> int:
    # 64-bit zigzag, compatible with Redis module signed encoding.
    #  0 -> 0, -1 -> 1, 1 -> 2, -2 -> 3, ...
    if n >= 0:
        return n << 1
    return ((-n) << 1) - 1


def _corrupt_dump_set_first_uint_after_3_doubles(dump_payload: bytes, new_value: int) -> bytes:
    # Redis DUMP value format: [RDB encoded value][2-byte version][8-byte CRC64]
    if len(dump_payload) < 10:
        raise RuntimeError("DUMP payload too small")
    value = dump_payload[:-10]
    version = dump_payload[-10:-8]

    # The module value begins with a type byte, then a module-id length+value,
    # followed by a stream of module opcodes.
    pos = 1
    _, is_enc, pos, _ = _load_len(value, pos)  # module id
    if is_enc:
        raise RuntimeError("Unexpected encoded module-id length")

    doubles_seen = 0
    patched = False

    while pos < len(value):
        opcode, is_enc, pos, _ = _load_len(value, pos)
        if is_enc:
            raise RuntimeError(f"Unexpected encoded opcode at pos={pos}")
        if opcode == RDB_MODULE_OPCODE_EOF:
            break
        if opcode == RDB_MODULE_OPCODE_UINT:
            val_start = pos
            _, is_enc2, pos, _ = _load_len(value, pos)
            if is_enc2:
                raise RuntimeError("Unexpected encoded value in MODULE_OPCODE_UINT")
            val_end = pos
            if (not patched) and doubles_seen >= 3:
                encoded = _encode_len(_zigzag_encode(new_value))
                new_value_bytes = value[:val_start] + encoded + value[val_end:]
                new_crc = _crc64_redis(new_value_bytes + version)
                patched = True
                return new_value_bytes + version + struct.pack("<Q", new_crc)
            continue
        if opcode == RDB_MODULE_OPCODE_DOUBLE:
            pos += 8
            doubles_seen += 1
            continue
        if opcode == RDB_MODULE_OPCODE_STRING:
            slen_or_enc, is_str_enc, pos, enc = _load_len(value, pos)
            if not is_str_enc:
                slen = slen_or_enc
                pos += slen
            else:
                if enc != RDB_ENC_LZF:
                    raise RuntimeError(f"Unsupported encoded string type: {enc}")
                clen, is_enc3, pos, _ = _load_len(value, pos)
                if is_enc3:
                    raise RuntimeError("Unexpected encoded compressed length")
                ulen, is_enc4, pos, _ = _load_len(value, pos)
                if is_enc4:
                    raise RuntimeError("Unexpected encoded uncompressed length")
                comp_end = pos + clen
                if comp_end > len(value):
                    raise RuntimeError("Compressed string overruns buffer while parsing")
                comp = value[pos:comp_end]
                _lzf_decompress(comp, ulen)
                pos = comp_end
            continue
        raise RuntimeError(f"Unknown module opcode {opcode} at pos={pos}")

    if not patched:
        raise RuntimeError("Did not find target SINT to patch")


def _module_dump_prefix_and_version(dump_payload: bytes):
    if len(dump_payload) < 10:
        raise RuntimeError("DUMP payload too small")

    value = dump_payload[:-10]
    version = dump_payload[-10:-8]
    pos = 1
    _, is_enc, pos, _ = _load_len(value, pos)
    if is_enc:
        raise RuntimeError("Unexpected encoded module-id length")
    return value[:pos], version


def _module_double(raw: bytes) -> bytes:
    if len(raw) != 8:
        raise RuntimeError("Module double must contain exactly 8 bytes")
    return bytes([RDB_MODULE_OPCODE_DOUBLE]) + raw


def _module_uint(value: int) -> bytes:
    return bytes([RDB_MODULE_OPCODE_UINT]) + _encode_len(value)


def _build_p88w_first_restore_payload(seed_dump: bytes, fake_address: int) -> bytes:
    # Reproduce build_corrupt(S_ODD, C_ODD, closure + 32, closure + 32, 8)
    # from the published exploit. The module prefix and RDB version come from a
    # local valid DUMP so this regression remains usable across Redis versions.
    prefix, version = _module_dump_prefix_and_version(seed_dump)
    slots = [struct.pack("<Q", 0)] * (P88W_S_ODD + 4)

    slots[P88W_C_ODD + 0] = struct.pack("<d", 1.0)
    slots[P88W_C_ODD + 1] = struct.pack("<d", 0.0)
    slots[P88W_C_ODD + 2] = struct.pack("<d", 0.0)
    slots[P88W_C_ODD + 3] = struct.pack("<II", P88W_FAKE_CAP, 8)
    slots[P88W_C_ODD + 4] = struct.pack("<II", 0, 0)
    slots[P88W_C_ODD + 5] = struct.pack("<Q", 0)
    slots[P88W_C_ODD + 6] = struct.pack("<d", 0.0)
    slots[P88W_C_ODD + 7] = struct.pack("<d", 0.0)
    slots[P88W_C_ODD + 8] = struct.pack("<Q", fake_address)
    slots[P88W_C_ODD + 9] = struct.pack("<Q", fake_address)
    slots[P88W_S_ODD + 3] = struct.pack("<II", P88W_FAKE_CAP, 0)

    declared = P88W_S_ODD + 4
    value = prefix
    value += _module_double(struct.pack("<d", 1.0))
    value += _module_double(struct.pack("<d", 0.0))
    value += _module_double(struct.pack("<d", 0.0))
    value += _module_uint(declared)
    value += _module_uint(declared)
    value += _module_uint(0)
    value += _module_uint(0)
    value += _module_double(struct.pack("<d", 0.0))
    value += _module_double(struct.pack("<d", 0.0))
    value += b"".join(_module_double(slot) for slot in slots)
    value += bytes([RDB_MODULE_OPCODE_EOF])

    body = value + version
    return body + struct.pack("<Q", _crc64_redis(body))


def _corrupt_dump_shrink_largest_module_string(dump_payload: bytes) -> bytes:
    def shrink_to_quarter(decoded: bytes) -> bytes:
        return decoded[: max(1, len(decoded) // 4)]

    return rewrite_largest_module_string(dump_payload, shrink_to_quarter)


class testBFRestoreCorruptRDB():
    def __init__(self):
        # We need raw bytes for DUMP/RESTORE payload manipulation
        self.env = Env(decodeResponses=False)

    def test_restore_rejects_corrupted_bloom_bitset(self):
        env = self.env
        env.cmd("FLUSHALL")

        # Use a hash-tag so keys land in same slot on cluster runs
        key = b"bf_poc{bf}"
        corrupt_key = b"bf_poc_corrupt{bf}"

        env.cmd("BF.RESERVE", key, 0.01, 1000)
        dump_payload = env.cmd("DUMP", key)
        corrupted = _corrupt_dump_shrink_largest_module_string(dump_payload)

        # With the fix, the module should reject the crafted payload during load.
        with env.assertResponseError():
            env.cmd("RESTORE", corrupt_key, 0, corrupted)

        # Ensure the server/module remains healthy
        env.cmd("BF.ADD", key, b"sanity")

    def test_restore_rejects_oversized_nfilters(self):
        env = self.env
        env.cmd("FLUSHALL")

        key = b"bf_nf{bf}"
        corrupt_key = b"bf_nf_corrupt{bf}"

        env.cmd("BF.RESERVE", key, 0.01, 1000)
        dump_payload = env.cmd("DUMP", key)
        # nfilters is the 2nd unsigned saved by BFRdbSave (size, nfilters, ...).
        corrupted = _corrupt_dump_set_nth_uint(dump_payload, 2, 0xFFFFFFFFFFFFFFFF)

        with env.assertResponseError():
            env.cmd("RESTORE", corrupt_key, 0, corrupted)

        # Ensure the server/module remains healthy
        env.cmd("BF.ADD", key, b"sanity")
        env.assertEqual(env.cmd("PING"), True)


class testCFRestoreCorruptRDB():
    def __init__(self):
        # We need raw bytes for DUMP/RESTORE payload manipulation
        self.env = Env(decodeResponses=False)

    def test_restore_rejects_corrupted_cuckoo_filter_buffer(self):
        env = self.env
        env.cmd("FLUSHALL")

        # Use a hash-tag so keys land in same slot on cluster runs
        key = b"cf_poc{cf}"
        corrupt_key = b"cf_poc_corrupt{cf}"

        env.cmd("CF.RESERVE", key, 1000)
        dump_payload = env.cmd("DUMP", key)
        corrupted = _corrupt_dump_shrink_largest_module_string(dump_payload)

        # With the fix, the module should reject the crafted payload during load.
        with env.assertResponseError():
            env.cmd("RESTORE", corrupt_key, 0, corrupted)

        # Ensure the server/module remains healthy
        env.cmd("CF.ADD", key, b"sanity")


class testTDigestRestoreCorruptRDB():
    def __init__(self):
        # We need raw bytes for DUMP/RESTORE payload manipulation
        self.env = Env(decodeResponses=False)

    def test_restore_rejects_corrupted_tdigest_cap(self):
        env = self.env
        env.cmd("FLUSHALL")

        key = b"td_poc{td}"
        corrupt_key = b"td_poc_corrupt{td}"

        # Small compression so allocated cap is small; corruption inflates cap.
        env.cmd("tdigest.create", key, "compression", 10)
        env.cmd("tdigest.add", key, 1.0)

        dump_payload = env.cmd("DUMP", key)
        corrupted = _corrupt_dump_set_first_uint_after_3_doubles(dump_payload, 1_000_000)

        with env.assertResponseError():
            env.cmd("RESTORE", corrupt_key, 0, corrupted)

        # Ensure the server/module remains healthy
        env.cmd("tdigest.add", key, 2.0)

    def test_restore_rejects_p88w_twitter_poc(self):
        env = self.env
        env.cmd("FLUSHALL")

        seed_key = b"td_p88w_seed{td}"
        corrupt_key = b"m:td{td}"

        env.cmd("TDIGEST.CREATE", seed_key, "COMPRESSION", 1)
        seed_dump = env.cmd("DUMP", seed_key)

        # The PoC uses the Lua CClosure address as its first memory-read target.
        closure_reply = env.cmd("EVAL", "return tostring(string.format)", 0)
        closure = int(closure_reply.decode().split("0x", 1)[1], 16)
        payload = _build_p88w_first_restore_payload(seed_dump, closure + 32)

        with env.assertResponseError():
            env.cmd("RESTORE", corrupt_key, 0, payload, "REPLACE")

        # The exploit must not establish its read/write primitive or damage Redis.
        env.assertEqual(env.cmd("EXISTS", corrupt_key), 0)
        env.assertEqual(env.cmd("PING"), True)
        env.cmd("TDIGEST.ADD", seed_key, 1.0)
