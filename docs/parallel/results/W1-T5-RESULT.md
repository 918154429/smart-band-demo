# W1-T5 Result: Q6 Sync Protocol v1 Envelope

## Delivery

- Base: `927001a772a4431ae1cf74b745d9abdb884cd336`
- Branch: `codex/w1-q6-sync-protocol`
- Scope: stateless envelope codec only; no transport, service, BLE, or payload schema

Files delivered:

- `openvela_app/smart_band/include/smart_band_sync_protocol.h`
- `openvela_app/smart_band/services/sync_protocol.c`
- `tests/sync_protocol_test.c`
- `tests/test_sync_protocol.py`
- `tests/vectors/sync-v1-envelope.json`
- `docs/protocol/smart-band-sync-v1-envelope.md`
- `docs/parallel/results/W1-T5-RESULT.md`

No existing file was modified.

## Frozen header

All multi-byte fields are unsigned little-endian values.

| Offset | Width | Field |
| ---: | ---: | --- |
| 0 | 2 | magic `0x4253` (`53 42`) |
| 2 | 1 | protocol major `1` |
| 3 | 1 | protocol minor `0` |
| 4 | 1 | frame type |
| 5 | 1 | flags; bits 2..7 reserved zero |
| 6 | 1 | status |
| 7 | 1 | reserved zero |
| 8 | 2 | payload length, maximum 224 |
| 10 | 4 | transaction ID |
| 14 | 2 | sequence |
| 16 | 2 | chunk index |
| 18 | N | opaque payload |
| 18 + N | 2 | CRC16 trailer |

The fixed header is 18 bytes and the maximum complete frame is 244 bytes.

## CRC and compatibility

CRC-16/CCITT-FALSE uses width 16, polynomial `0x1021`, init `0xffff`, no
reflection, xorout `0x0000`, and covers the header plus payload but not the CRC
trailer. ASCII `123456789` produces the standard check value `0x29b1`.

The decoder requires major `1` and accepts minor versions up to its implemented
minor. The current 1.0 decoder therefore rejects future minor versions as well
as unknown majors. Exact frame length and all-zero reserved bits are mandatory.

## Error enum

`OK`, `INVALID_ARGUMENT`, `BUFFER_TOO_SMALL`, `BOUNDS`, `TRUNCATED`,
`TRAILING_DATA`, `BAD_MAGIC`, `BAD_VERSION`, `BAD_TYPE`, `BAD_FLAGS`,
`BAD_STATUS`, `BAD_RESERVED`, and `BAD_CRC` are distinct results.

## Golden vector

`tests/vectors/sync-v1-envelope.json` contains independently expected complete
frame bytes. The header/payload bytes were assembled from the documented table;
the CRC was cross-checked with Python `binascii.crc_hqx(data, 0xffff)`.

```text
53 42 01 00 01 03 00 00 04 00 78 56 34 12 01 02 03 04
de ad be ef c8 a6
```

## Verification

Command:

```powershell
python tests/test_sync_protocol.py
```

Result on Windows with MSVC C11 `/W4 /WX` and OpenCppCoverage 0.9.9.0:

```text
sync protocol v1 envelope tests passed (10000 malformed frames)
sync_protocol.c line coverage: 100.00%
```

The matrix includes the standard CRC check, independent golden encode/decode,
empty and maximum payloads, every truncated golden prefix, trailing data,
declared lengths that are too small/large/out of bounds, bad versions/types/
flags/status/reserved byte, every frame byte corrupted in turn, every encode
capacity below the required size, supported payload/output aliasing, zero-copy
decode view behavior, and 10,000 deterministic malformed frames.

## Not frozen

History, settings, notification, and other business payloads remain opaque.
Duplicate and out-of-order handling, transaction lifecycle, ACK/retry policy,
timeouts, fragmentation/reassembly, loopback, Linux client, GATT/BLE transport,
pairing, authentication, and encryption are not implemented or validated here.

## W1-I integration request

W1-I must add `services/sync_protocol.c` and its public header to the real build
surfaces, register `tests/test_sync_protocol.py` in host CI, and add the
production source to the shared coverage boundary. W1-I must then run the
frozen aggregate host/openvela/native gates. It should confirm that the chosen
244-byte maximum frame aligns with the selected transport MTU before adding any
fragmentation policy.

## Known risk

The envelope is syntax-only. Valid ACK, flag, status, sequence, chunk, and
transaction values do not prove that a future stateful sync service uses them
correctly. No security claim follows from CRC integrity, and the deterministic
malformed-frame matrix is not a security fuzzing result.
