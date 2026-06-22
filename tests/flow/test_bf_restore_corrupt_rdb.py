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

class testBFRestoreCorruptRDB():
    def __init__(self):
        # We need raw bytes for DUMP/RESTORE payload manipulation
        self.env = Env(decodeResponses=False)

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
