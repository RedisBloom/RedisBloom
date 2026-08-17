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
RDB_MODULE_OPCODE_SINT = 1
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


def _murmur2(data: bytes, seed: int) -> int:
    # Port of MurmurHash2 (32-bit) from deps/murmur2/MurmurHash2.c, used by TopK.
    m = 0x5BD1E995
    r = 24
    length = len(data)
    h = (seed ^ length) & 0xFFFFFFFF
    i = 0
    while length >= 4:
        k = int.from_bytes(data[i : i + 4], "little")
        k = (k * m) & 0xFFFFFFFF
        k ^= k >> r
        k = (k * m) & 0xFFFFFFFF
        h = (h * m) & 0xFFFFFFFF
        h ^= k
        i += 4
        length -= 4
    if length == 3:
        h ^= data[i + 2] << 16
    if length >= 2:
        h ^= data[i + 1] << 8
    if length >= 1:
        h ^= data[i]
        h = (h * m) & 0xFFFFFFFF
    h ^= h >> 13
    h = (h * m) & 0xFFFFFFFF
    h ^= h >> 15
    return h & 0xFFFFFFFF


def _corrupt_dump_patch_largest_module_string(dump_payload: bytes, patch_fn) -> bytes:
    # Decode the largest MODULE_OPCODE_STRING, apply patch_fn(decoded)->bytes (same
    # length), re-emit it uncompressed, and fix up the trailing CRC64. Unlike
    # _corrupt_dump_shrink_largest_module_string this keeps the string length intact,
    # so RDB-load size validations still pass and the patched *contents* are exercised.
    if len(dump_payload) < 10:
        raise RuntimeError("DUMP payload too small")
    value = dump_payload[:-10]
    version = dump_payload[-10:-8]

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
        if opcode in (RDB_MODULE_OPCODE_SINT, RDB_MODULE_OPCODE_UINT):
            _, is_enc2, pos, _ = _load_len(value, pos)
            if is_enc2:
                raise RuntimeError("Unexpected encoded value in module integer")
            continue
        if opcode == RDB_MODULE_OPCODE_DOUBLE:
            pos += 8
            continue
        if opcode == RDB_MODULE_OPCODE_STRING:
            len_start = pos
            slen_or_enc, is_str_enc, pos, enc = _load_len(value, pos)
            if not is_str_enc:
                slen = slen_or_enc
                data_end = pos + slen
                if data_end > len(value):
                    raise RuntimeError("String overruns buffer while parsing")
                decoded = value[pos:data_end]
                pos = data_end
            else:
                if enc != RDB_ENC_LZF:
                    raise RuntimeError(f"Unsupported encoded string type: {enc}")
                clen, is_enc3, pos, _ = _load_len(value, pos)
                ulen, is_enc4, pos, _ = _load_len(value, pos)
                if is_enc3 or is_enc4:
                    raise RuntimeError("Unexpected encoded compressed/uncompressed length")
                comp_end = pos + clen
                if comp_end > len(value):
                    raise RuntimeError("Compressed string overruns buffer while parsing")
                decoded = _lzf_decompress(value[pos:comp_end], ulen)
                pos = comp_end
            strings.append({"len_start": len_start, "old_end": pos, "decoded": decoded})
            continue
        raise RuntimeError(f"Unknown module opcode {opcode} at pos={pos}")

    if not strings:
        raise RuntimeError("No MODULE_OPCODE_STRING entries found; cannot corrupt payload")

    _, target = max(enumerate(strings), key=lambda t: len(t[1]["decoded"]))
    new_data = patch_fn(target["decoded"])
    if len(new_data) != len(target["decoded"]):
        raise RuntimeError("patch_fn must preserve the string length")

    new_value = (
        value[: target["len_start"]]
        + _encode_len(len(new_data))
        + new_data
        + value[target["old_end"] :]
    )
    new_crc = _crc64_redis(new_value + version)
    return new_value + version + struct.pack("<Q", new_crc)


def _corrupt_dump_set_nth_module_string(
    dump_payload: bytes, n: int, new_data: bytes
) -> bytes:
    if n < 1:
        raise RuntimeError("n must be >= 1")
    if len(dump_payload) < 10:
        raise RuntimeError("DUMP payload too small")

    value = dump_payload[:-10]
    version = dump_payload[-10:-8]

    pos = 1
    _, is_enc, pos, _ = _load_len(value, pos)  # module id
    if is_enc:
        raise RuntimeError("Unexpected encoded module-id length")

    string_seen = 0
    while pos < len(value):
        opcode, is_enc, pos, _ = _load_len(value, pos)
        if is_enc:
            raise RuntimeError(f"Unexpected encoded opcode at pos={pos}")
        if opcode == RDB_MODULE_OPCODE_EOF:
            break
        if opcode in (RDB_MODULE_OPCODE_SINT, RDB_MODULE_OPCODE_UINT):
            _, is_enc2, pos, _ = _load_len(value, pos)
            if is_enc2:
                raise RuntimeError("Unexpected encoded value in module integer")
            continue
        if opcode == RDB_MODULE_OPCODE_DOUBLE:
            pos += 8
            continue
        if opcode == RDB_MODULE_OPCODE_STRING:
            len_start = pos
            slen_or_enc, is_str_enc, pos, enc = _load_len(value, pos)
            if not is_str_enc:
                old_end = pos + slen_or_enc
            else:
                if enc != RDB_ENC_LZF:
                    raise RuntimeError(f"Unsupported encoded string type: {enc}")
                clen, is_enc3, pos, _ = _load_len(value, pos)
                _, is_enc4, pos, _ = _load_len(value, pos)
                if is_enc3 or is_enc4:
                    raise RuntimeError("Unexpected encoded compressed/uncompressed length")
                old_end = pos + clen
            if old_end > len(value):
                raise RuntimeError("String overruns buffer while parsing")
            string_seen += 1
            if string_seen == n:
                new_value_bytes = (
                    value[:len_start] + _encode_len(len(new_data)) + new_data + value[old_end:]
                )
                new_crc = _crc64_redis(new_value_bytes + version)
                return new_value_bytes + version + struct.pack("<Q", new_crc)
            pos = old_end
            continue
        raise RuntimeError(f"Unknown module opcode {opcode} at pos={pos}")

    raise RuntimeError(f"Did not find STRING #{n} to patch")


def _corrupt_dump_set_nth_uint_after_3_doubles(
    dump_payload: bytes, n: int, new_value: int
) -> bytes:
    if n < 1:
        raise RuntimeError("n must be >= 1")

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
    uint_seen = 0

    while pos < len(value):
        opcode, is_enc, pos, _ = _load_len(value, pos)
        if is_enc:
            raise RuntimeError(f"Unexpected encoded opcode at pos={pos}")
        if opcode == RDB_MODULE_OPCODE_EOF:
            break
        if opcode in (RDB_MODULE_OPCODE_SINT, RDB_MODULE_OPCODE_UINT):
            val_start = pos
            _, is_enc2, pos, _ = _load_len(value, pos)
            if is_enc2:
                raise RuntimeError("Unexpected encoded value in module integer")
            val_end = pos
            if doubles_seen >= 3:
                uint_seen += 1
            if uint_seen == n:
                encoded_value = (
                    _zigzag_encode(new_value)
                    if opcode == RDB_MODULE_OPCODE_SINT
                    else new_value
                )
                encoded = _encode_len(encoded_value)
                new_value_bytes = value[:val_start] + encoded + value[val_end:]
                new_crc = _crc64_redis(new_value_bytes + version)
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

    raise RuntimeError(f"Did not find UINT #{n} after the first 3 doubles")


def _corrupt_dump_set_first_uint_after_3_doubles(dump_payload: bytes, new_value: int) -> bytes:
    return _corrupt_dump_set_nth_uint_after_3_doubles(dump_payload, 1, new_value)


def _corrupt_dump_set_nth_double(dump_payload: bytes, n: int, new_value: float) -> bytes:
    if n < 1:
        raise RuntimeError("n must be >= 1")
    if len(dump_payload) < 10:
        raise RuntimeError("DUMP payload too small")

    value = dump_payload[:-10]
    version = dump_payload[-10:-8]

    pos = 1
    _, is_enc, pos, _ = _load_len(value, pos)  # module id
    if is_enc:
        raise RuntimeError("Unexpected encoded module-id length")

    double_seen = 0
    while pos < len(value):
        opcode, is_enc, pos, _ = _load_len(value, pos)
        if is_enc:
            raise RuntimeError(f"Unexpected encoded opcode at pos={pos}")
        if opcode == RDB_MODULE_OPCODE_EOF:
            break
        if opcode in (RDB_MODULE_OPCODE_SINT, RDB_MODULE_OPCODE_UINT):
            _, is_enc2, pos, _ = _load_len(value, pos)
            if is_enc2:
                raise RuntimeError("Unexpected encoded value in module integer")
            continue
        if opcode == RDB_MODULE_OPCODE_DOUBLE:
            double_seen += 1
            if pos + 8 > len(value):
                raise RuntimeError("Double overruns buffer while parsing")
            if double_seen == n:
                new_value_bytes = value[:pos] + struct.pack("<d", new_value) + value[pos + 8 :]
                new_crc = _crc64_redis(new_value_bytes + version)
                return new_value_bytes + version + struct.pack("<Q", new_crc)
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
                _, is_enc4, pos, _ = _load_len(value, pos)
                if is_enc3 or is_enc4:
                    raise RuntimeError("Unexpected encoded compressed/uncompressed length")
                pos += clen
            continue
        raise RuntimeError(f"Unknown module opcode {opcode} at pos={pos}")

    raise RuntimeError(f"Did not find DOUBLE #{n} to patch")


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
        if opcode in (RDB_MODULE_OPCODE_SINT, RDB_MODULE_OPCODE_UINT):
            val_start = pos
            _, is_enc2, pos, _ = _load_len(value, pos)
            if is_enc2:
                raise RuntimeError("Unexpected encoded value in module integer")
            val_end = pos
            if opcode == RDB_MODULE_OPCODE_SINT:
                continue
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
        if opcode in (RDB_MODULE_OPCODE_SINT, RDB_MODULE_OPCODE_UINT):
            _, is_enc2, pos, _ = _load_len(value, pos)
            if is_enc2:
                raise RuntimeError("Unexpected encoded value in module integer")
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

    def test_restore_rejects_invalid_bloom_numeric_state(self):
        env = self.env
        env.cmd("FLUSHALL")

        key = b"bf_numeric{bf}"
        env.cmd("BF.RESERVE", key, 0.01, 1000)
        dump_payload = env.cmd("DUMP", key)

        corruptions = (
            (b"error_nan", _corrupt_dump_set_nth_double(dump_payload, 1, float("nan"))),
            (b"bpe_inf", _corrupt_dump_set_nth_double(dump_payload, 2, float("inf"))),
            (b"options_narrow", _corrupt_dump_set_nth_uint(dump_payload, 3, 1 << 32)),
            (b"hashes_narrow", _corrupt_dump_set_nth_uint(dump_payload, 6, (1 << 32) + 7)),
            (b"n2_narrow", _corrupt_dump_set_nth_uint(dump_payload, 8, (1 << 32) + 14)),
        )

        for suffix, corrupted in corruptions:
            corrupt_key = b"bf_numeric_" + suffix + b"{bf}"
            with env.assertResponseError():
                env.cmd("RESTORE", corrupt_key, 0, corrupted)
            env.assertEqual(env.cmd("EXISTS", corrupt_key), 0)

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


class testTopKRestoreCorruptRDB():
    def __init__(self):
        # We need raw bytes for DUMP/RESTORE payload manipulation
        self.env = Env(decodeResponses=False)

    def test_restore_rejects_corrupted_topk_heap(self):
        env = self.env
        env.cmd("FLUSHALL")

        key = b"topk_poc{topk}"
        corrupt_key = b"topk_poc_corrupt{topk}"

        # k=3, width=1, depth=1 → heap buffer is 3*sizeof(HeapBucket)=72 bytes,
        # which is larger than the data buffer (1*1*sizeof(Bucket)=8 bytes) and
        # item strings (1 byte each). _corrupt_dump_shrink_largest_module_string
        # will therefore target and truncate the heap buffer.
        env.cmd("TOPK.RESERVE", key, 3, 1, 1, 0.9)
        dump_payload = env.cmd("DUMP", key)
        corrupted = _corrupt_dump_shrink_largest_module_string(dump_payload)

        # Without the fix: TopK_Destroy is invoked via errdefer with topk->heap
        # still pointing at the undersized buffer, iterating all k entries and
        # reading past the end of the allocation (OOB / UB / crash).
        # With the fix: the mismatch branch frees and NULLs topk->heap before
        # returning, so TopK_Destroy skips the heap loop entirely.
        with env.assertResponseError():
            env.cmd("RESTORE", corrupt_key, 0, corrupted)

    def test_restore_topk_inflated_itemlen_is_safe(self):
        # Robustness test: a restored TopK whose heap blob carries an `itemlen`
        # that does not match the stored item buffer must still behave correctly.
        # TopKRdbLoad only validates the total heap blob size, so a payload with a
        # valid size but a larger-than-real `itemlen` would otherwise leave the
        # bucket length out of sync with its item buffer. The fix derives `itemlen`
        # from the loaded buffer, so item comparisons during queries stay
        # consistent. Here we load such a payload and confirm TOPK queries keep
        # working and the server stays healthy.
        env = self.env
        env.cmd("FLUSHALL")

        key = b"topk_il{topk}"
        corrupt_key = b"topk_il_corrupt{topk}"

        # k=1,width=1,depth=1 -> the heap blob (one HeapBucket) is the largest
        # module string, so the patch helper targets it.
        env.cmd("TOPK.RESERVE", key, 1, 1, 1, 0.9)
        env.cmd("TOPK.ADD", key, b"a")  # heap[0].item -> "a" (2-byte allocation)

        dump_payload = env.cmd("DUMP", key)

        GA = 1919  # fingerprint seed used by TopK (see topk.c)
        probe = b"A" * 200  # query whose length we set as the bucket's itemlen
        probe_fp = _murmur2(probe, GA)

        def patch_heap(decoded: bytes) -> bytes:
            # HeapBucket layout: uint32 fp; uint32 itemlen; char *item; uint32 count
            b = bytearray(decoded)
            b[0:4] = struct.pack("<I", probe_fp)     # fp matches our probe
            b[4:8] = struct.pack("<I", len(probe))   # itemlen larger than the stored item
            return bytes(b)

        corrupted = _corrupt_dump_patch_largest_module_string(dump_payload, patch_heap)

        # The heap blob size is valid, so the payload loads; the fix realigns
        # `itemlen` with the stored item buffer.
        env.cmd("RESTORE", corrupt_key, 0, corrupted)

        # These queries compare the probe against the (short) stored item; with the
        # fix they complete normally and the server stays healthy.
        env.cmd("TOPK.QUERY", corrupt_key, probe)
        env.cmd("TOPK.COUNT", corrupt_key, probe)
        env.cmd("TOPK.ADD", corrupt_key, probe)
        env.assertEqual(env.cmd("PING"), True)

    def test_restore_rejects_invalid_topk_item_buffers(self):
        env = self.env
        env.cmd("FLUSHALL")

        key = b"topk_item{topk}"
        env.cmd("TOPK.RESERVE", key, 1, 1, 1, 0.9)
        env.cmd("TOPK.ADD", key, b"a")
        dump_payload = env.cmd("DUMP", key)

        # The first two strings are the data and heap blobs; the third is heap[0].item.
        for suffix, item_buffer in (
            (b"zero_length", b""),
            (b"unterminated", b"AA"),
        ):
            corrupted = _corrupt_dump_set_nth_module_string(dump_payload, 3, item_buffer)
            corrupt_key = b"topk_item_" + suffix + b"{topk}"
            with env.assertResponseError():
                env.cmd("RESTORE", corrupt_key, 0, corrupted)
            env.assertEqual(env.cmd("EXISTS", corrupt_key), 0)

        env.assertEqual(env.cmd("PING"), True)


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

    def test_restore_rejects_invalid_tdigest_compression(self):
        env = self.env
        env.cmd("FLUSHALL")

        key = b"td_compression{td}"
        env.cmd("tdigest.create", key, "compression", 10)
        dump_payload = env.cmd("DUMP", key)

        for suffix, compression in (
            (b"nan", float("nan")),
            (b"inf", float("inf")),
            (b"negative", -1.0),
            (b"oversized", 357_913_940.0),
        ):
            corrupted = _corrupt_dump_set_nth_double(dump_payload, 1, compression)
            corrupt_key = b"td_compression_" + suffix + b"{td}"
            with env.assertResponseError():
                env.cmd("RESTORE", corrupt_key, 0, corrupted)
            env.assertEqual(env.cmd("EXISTS", corrupt_key), 0)

        env.assertEqual(env.cmd("PING"), True)

    def test_restore_rejects_tdigest_node_counts_before_narrowing(self):
        env = self.env
        env.cmd("FLUSHALL")

        key = b"td_nodes{td}"
        env.cmd("tdigest.create", key, "compression", 10)
        env.cmd("tdigest.add", key, 1.0)
        dump_payload = env.cmd("DUMP", key)

        # After compression/min/max, the UINTs are cap, merged_nodes, and
        # unmerged_nodes. 2**32 narrows to zero on common 32-bit int targets, so
        # validating only after assignment could accept the crafted count.
        for nth, corrupt_key in (
            (2, b"td_merged_corrupt{td}"),
            (3, b"td_unmerged_corrupt{td}"),
        ):
            corrupted = _corrupt_dump_set_nth_uint_after_3_doubles(
                dump_payload, nth, 1 << 32
            )
            with env.assertResponseError():
                env.cmd("RESTORE", corrupt_key, 0, corrupted)
            env.assertEqual(env.cmd("EXISTS", corrupt_key), 0)

        env.assertEqual(env.cmd("PING"), True)

    def test_restore_rejects_invalid_tdigest_numeric_state(self):
        env = self.env
        env.cmd("FLUSHALL")

        key = b"td_numeric{td}"
        env.cmd("tdigest.create", key, "compression", 10)
        env.cmd("tdigest.add", key, 1.0)
        dump_payload = env.cmd("DUMP", key)

        corruptions = (
            (b"merged_weight_inf", _corrupt_dump_set_nth_double(dump_payload, 4, float("inf"))),
            (b"merged_weight_fraction", _corrupt_dump_set_nth_double(dump_payload, 4, 1.5)),
            (b"merged_weight_mismatch", _corrupt_dump_set_nth_double(dump_payload, 4, 2.0)),
            (b"mean_nan", _corrupt_dump_set_nth_double(dump_payload, 6, float("nan"))),
            (b"node_weight_inf", _corrupt_dump_set_nth_double(dump_payload, 7, float("inf"))),
            (
                b"compressions_max",
                _corrupt_dump_set_nth_uint_after_3_doubles(
                    dump_payload, 4, (1 << 63) - 1
                ),
            ),
            (
                b"unmerged_nodes",
                _corrupt_dump_set_nth_uint_after_3_doubles(dump_payload, 3, 1),
            ),
        )

        for suffix, corrupted in corruptions:
            corrupt_key = b"td_numeric_" + suffix + b"{td}"
            with env.assertResponseError():
                env.cmd("RESTORE", corrupt_key, 0, corrupted)
            env.assertEqual(env.cmd("EXISTS", corrupt_key), 0)

        env.assertEqual(env.cmd("PING"), True)

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


class testCMSRestoreCorruptRDB():
    def __init__(self):
        # We need raw bytes for DUMP/RESTORE payload manipulation
        self.env = Env(decodeResponses=False)

    def test_restore_rejects_corrupted_cms_width_depth(self):
        env = self.env
        env.cmd("FLUSHALL")

        key = b"cms_poc{cms}"
        corrupt_key = b"cms_poc_corrupt{cms}"
        corrupted = b'\x07\x81\x08\xc4\xa4\xf96\x0f\x10\x00\x02\x81@\x00\x00\x00\x00\x00\x00\xaf\x02\x01\x02\x00\x05B\xbcXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\x00\xff\x0c\x00\xdf\xf7\xa1w\xbf\x95\xb9\x14'
        with env.assertResponseError():
            env.cmd("RESTORE", corrupt_key, 0, corrupted)
