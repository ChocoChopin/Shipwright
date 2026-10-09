"""Typed semantic SHA-256 v1. Historical JSONL is streamed once for migration.

Normal runs hash in the engine and never send semantic trees through Python.
"""
import hashlib
import math
import struct

FORMAT = 'semantic-sha256-v1'


def semantic_hash(value):
    h = hashlib.sha256()

    def u64(n):
        h.update(struct.pack('>Q', n))

    def text(s):
        data = s.encode('utf-8')
        u64(len(data))
        h.update(data)

    def feed(v):
        if v is None:
            h.update(b'n')
        elif type(v) is bool:
            h.update(b'b' + bytes([v]))
        elif type(v) is int:
            h.update(b'i' + bytes([v < 0]))
            u64(abs(v))
        elif type(v) is float:
            if not math.isfinite(v):
                raise ValueError('non-finite semantic number')
            h.update(b'f' + struct.pack('>d', v))
        elif type(v) is str:
            h.update(b's')
            text(v)
        elif type(v) is list:
            h.update(b'a')
            u64(len(v))
            for item in v:
                feed(item)
        elif type(v) is dict:
            h.update(b'o')
            u64(len(v))
            for key in sorted(v):
                text(key)
                feed(v[key])
        else:
            raise ValueError('unsupported semantic type')

    feed(value)
    return h.hexdigest()
