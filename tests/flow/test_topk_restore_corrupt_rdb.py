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
    # length), re-emit it uncompressed, and fix up the trailing CRC64. Keeping the
    # string length intact means RDB-load size validations still pass and the
    # patched *contents* are exercised.
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


class testTopKRestoreCorruptRDB():
    def __init__(self):
        # We need raw bytes for DUMP/RESTORE payload manipulation
        self.env = Env(decodeResponses=False)

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
