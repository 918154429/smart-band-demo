# Q6 resumable history sync loopback slice — 2026-07-23

## Status and scope

This Q6 slice is complete for the host-tested protocol, history session state
machines, and deterministic in-memory loopback transport. It establishes a
versioned capabilities/request/data/ACK exchange and proves resumable transfer
of the existing daily-history domain under injected transport faults.

This is only a **Q6 stage slice**, not completion of Q6 or a BLE integration
claim. The automatic timeout/retry driver, remaining sync domains, Linux
client, simulator bridge, GATT binding, and real BLE transport remain open.

## Frozen slice contracts

- The v1 envelope carries message type, flags, sequence, payload length,
  transaction ID, cursor, payload, and CRC in a bounded little-endian frame.
- Capabilities advertise the history feature and negotiated MTU. History uses
  request/data/ACK messages with transaction, cursor, total, and fixed
  little-endian daily-record fields.
- The server is stop-and-wait. It records `awaiting_ack` only after a data frame
  has encoded successfully and accepts an ACK only for the exact frame that was
  sent. A syntactically valid early ACK cannot advance the cursor.
- The client freezes `expected_total` from the first accepted data frame.
  Later frames with a changed total are rejected without changing cursor,
  record count, or the frozen total.
- Client progress is atomic with ACK construction. If the ACK output buffer is
  too small, no record, cursor, or total state is committed; the same data
  frame can be retried.
- A server transaction owns an immutable copy of up to 30 daily records.
  Reconnecting with the same transaction and acknowledged cursor resumes that
  snapshot even if the live rolling history changes. A new transaction takes a
  fresh snapshot.
- Duplicate data and ACK frames are idempotent. Out-of-order future data is
  rejected. The loopback fault adapter deterministically injects drop,
  duplicate, reorder, poll delay, and disconnect behavior; stop/disconnect
  clears queued and held reorder frames so stale delivery cannot cross a
  reconnect.
- Frozen golden vectors cover capabilities, history request, history data,
  fixed daily-record fields, and history ACK, including CRC validation and
  malformed cases.

## Verification

Implementation commit:

- `8c52e7bd552e62f8b54c790f81157b2765ee915b` — resumable daily-history sync
  service, protocol integration, loopback fault adapter, golden vectors, and
  host tests

Integration head:

- `f0f227936dda48a221f14c0ac619ba05dbc39032` — merged the current Q4
  notification branch into Q6 without changing the Q6 session contracts

Pull request: <https://github.com/918154429/smart-band-demo/pull/16>

Current checks for the integration head, queried with `gh` on 2026-07-23:

- Host run: <https://github.com/918154429/smart-band-demo/actions/runs/29972934272>
- Browser run: <https://github.com/918154429/smart-band-demo/actions/runs/29972934359>
- Host: 58/58 jobs succeeded, including sync protocol and sync service on GCC,
  Clang, and MSVC, central runtime, full UI compile smoke, and production C
  coverage.
- Browser: 1/1 Chromium job succeeded.
- PR16 is OPEN/CLEAN at the integration head; all 59/59 current checks are
  successful.

Fresh local Windows/MSVC verification on 2026-07-23 also passed:

- `python tests/test_sync_service.py` — `/std:c11 /W4 /WX /permissive-`;
  history service and faulted loopback tests passed.
- `python tests/test_sync_protocol.py` — `/std:c11 /W4 /WX /permissive-`;
  v1 envelope tests passed with 10,000 deterministic malformed frames and
  `sync_protocol.c` line coverage of 100.00%.
- `python tests/test_runtime_core.py` — `/std:c11 /W4 /WX`; production central
  runtime tests passed with the loopback source linked.
- `python tests/test_ui_compile.py` — `/std:c11 /W4 /WX`; the complete
  production UI/application source set compiled, linked, and ran with the sync
  protocol, service, and loopback sources included.

The service journeys specifically verify:

- byte-for-byte retry after dropped data and idempotent recovery after a lost
  ACK;
- duplicated data/ACK acceptance without duplicate records or cursor drift;
- rejection of a reordered future chunk, then acceptance of the expected
  chunk;
- delayed polling without consuming the queued chunk;
- disconnect/reconnect resume from the acknowledged cursor and field-by-field
  equality for all seven transferred daily records;
- rejection of early ACKs before any data frame is sent;
- first-frame total locking and rejection of later total drift;
- no client commit when ACK encoding lacks capacity;
- immutable 30-record snapshot completion while live history rolls, followed
  by a fresh snapshot for a new transaction;
- queue and held-reorder cleanup across stop and injected disconnect;
- golden capabilities, request, data, daily-record, and ACK bytes.

## Evidence boundaries and remaining work

1. There is no automatic clock-driven timeout/retry driver yet. Tests invoke
   retries and resume explicitly around the frozen state-machine APIs.
2. Daily history is the only completed sync domain in this slice. Workout
   session history, device configuration, Notification Inbox, and live metrics
   remain open.
3. The Linux client and simulator bridge remain open.
4. The loopback transport is deterministic host infrastructure, not a GATT
   service or BLE link. GATT binding and real BLE transport remain open, and no
   physical-device interoperability, throughput, range, or power result is
   claimed.
