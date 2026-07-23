import struct

from common import *

# RDB module opcodes / encodings (see Redis RDB format)
RDB_6BITLEN = 0
RDB_14BITLEN = 1
RDB_ENC_INT8 = 0
RDB_ENC_INT16 = 1
RDB_ENC_INT32 = 2
RDB_ENC_LZF = 3
RDB_MODULE_OPCODE_EOF = 0
RDB_MODULE_OPCODE_UINT = 2
RDB_MODULE_OPCODE_DOUBLE = 4
RDB_MODULE_OPCODE_STRING = 5

CRC64_POLY = 0xAD93D23594C935A9

# First malicious RESTORE geometry from the public P88W exploit:
# https://github.com/berabuddies/redis-poc/blob/7540fa3619f849cd16307e612bc34676dfdccf91/P88W_exploit.py
P88W_S_ODD = 216
P88W_C_ODD = 16
P88W_FAKE_CAP = 0x40000000


def _crc_reflect(data: int, bits: int) -> int:
    out = 0
    for i in range(bits):
        if data & (1 << i):
            out |= 1 << (bits - 1 - i)
    return out


def _crc64_redis(data: bytes) -> int:
    crc = 0
    for b in data:
        for i in range(8):
            bit = crc & 0x8000000000000000
            if b & (1 << i):
                bit = 0 if bit else 1
            crc = (crc << 1) & 0xFFFFFFFFFFFFFFFF
            if bit:
                crc ^= CRC64_POLY
    return _crc_reflect(crc, 64) & 0xFFFFFFFFFFFFFFFF


def _encode_len(n: int) -> bytes:
    if n < (1 << 6):
        return bytes([n & 0x3F])
    if n < (1 << 14):
        return bytes([0x40 | ((n >> 8) & 0x3F), n & 0xFF])
    if n < (1 << 32):
        return bytes([0x80, (n >> 24) & 0xFF, (n >> 16) & 0xFF, (n >> 8) & 0xFF, n & 0xFF])
    return bytes([0x81]) + n.to_bytes(8, "big")


def _load_len(buf: bytes, pos: int):
    first = buf[pos]
    typ = (first & 0xC0) >> 6
    if typ == RDB_6BITLEN:
        return first & 0x3F, False, pos + 1, None
    if typ == RDB_14BITLEN:
        val = ((first & 0x3F) << 8) | buf[pos + 1]
        return val, False, pos + 2, None
    if typ == 2:
        enc = first & 0x3F
        if enc == RDB_ENC_INT8:
            val = int.from_bytes(buf[pos + 1 : pos + 5], "big")
            return val, False, pos + 5, None
        if enc == RDB_ENC_INT16:
            val = int.from_bytes(buf[pos + 1 : pos + 9], "big")
            return val, False, pos + 9, None
        return enc, True, pos + 1, enc
    enc = first & 0x3F
    return enc, True, pos + 1, enc


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


def _lzf_decompress(data: bytes, out_len: int) -> bytes:
    i = 0
    out = bytearray()
    while i < len(data):
        ctrl = data[i]
        i += 1
        if ctrl < 32:
            length = ctrl + 1
            out.extend(data[i : i + length])
            i += length
        else:
            length = ctrl >> 5
            ref = len(out) - ((ctrl & 0x1F) << 8) - 1
            if length == 7:
                length += data[i]
                i += 1
            ref -= data[i]
            i += 1
            length += 2
            for _ in range(length):
                out.append(out[ref])
                ref += 1
    if len(out) != out_len:
        raise RuntimeError(f"LZF output length mismatch (got {len(out)}, expected {out_len})")
    return bytes(out)


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

    strings = []
    while pos < len(value):
        opcode, is_enc, pos, _ = _load_len(value, pos)
        if is_enc:
            raise RuntimeError(f"Unexpected encoded opcode at pos={pos}")
        if opcode == RDB_MODULE_OPCODE_EOF:
            break
        if opcode == RDB_MODULE_OPCODE_UINT:
            _, is_enc2, pos, _ = _load_len(value, pos)
            if is_enc2:
                raise RuntimeError("Unexpected encoded value in MODULE_OPCODE_UINT")
            continue
        if opcode == RDB_MODULE_OPCODE_DOUBLE:
            pos += 8
            continue
        if opcode == RDB_MODULE_OPCODE_STRING:
            len_start = pos
            slen_or_enc, is_str_enc, pos, enc = _load_len(value, pos)
            if not is_str_enc:
                slen = slen_or_enc
                data_start = pos
                data_end = pos + slen
                if data_end > len(value):
                    raise RuntimeError("String overruns buffer while parsing")
                decoded = value[data_start:data_end]
                old_end = data_end
                pos = data_end
                strings.append({"len_start": len_start, "old_end": old_end, "decoded": decoded, "enc": None})
            else:
                if enc != RDB_ENC_LZF:
                    raise RuntimeError(f"Unsupported encoded string type: {enc}")
                clen, is_enc3, pos, _ = _load_len(value, pos)
                if is_enc3:
                    raise RuntimeError("Unexpected encoded compressed length")
                ulen, is_enc4, pos, _ = _load_len(value, pos)
                if is_enc4:
                    raise RuntimeError("Unexpected encoded uncompressed length")
                comp_start = pos
                comp_end = pos + clen
                if comp_end > len(value):
                    raise RuntimeError("Compressed string overruns buffer while parsing")
                comp = value[comp_start:comp_end]
                decoded = _lzf_decompress(comp, ulen)
                old_end = comp_end
                pos = comp_end
                strings.append({"len_start": len_start, "old_end": old_end, "decoded": decoded, "enc": "lzf"})
            continue
        raise RuntimeError(f"Unknown module opcode {opcode} at pos={pos}")

    if not strings:
        raise RuntimeError("No MODULE_OPCODE_STRING entries found; cannot corrupt payload")

    _, max_entry = max(enumerate(strings), key=lambda t: len(t[1]["decoded"]))
    decoded = max_entry["decoded"]
    old_len = len(decoded)
    new_len = max(1, old_len // 4)
    new_data = decoded[:new_len]
    len_start = max_entry["len_start"]
    old_end = max_entry["old_end"]

    new_value = value[:len_start] + _encode_len(new_len) + new_data + value[old_end:]
    new_crc = _crc64_redis(new_value + version)
    return new_value + version + struct.pack("<Q", new_crc)


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
