# Multi-instance server

## Overview

The server owns one UDP socket and a registry of independent `GameRoom`
instances. Each room owns its players, entities, simulation timers, difficulty,
and player/entity identifiers. A packet received from a connected endpoint is
routed only to the room that owns that endpoint.

The room with ID `1` is the compatibility room. Existing clients can continue
to send the original `ConnectReq` packet and are automatically connected to
that room. A room created through the lobby is joined explicitly with
`RoomJoinReq`.

The default room is never automatically destroyed. Empty user-created rooms
are removed after 30 seconds. The maximum room count includes the default room.

## UDP packet header

All packets use the existing packed 9-byte header:

| Field | Type | Description |
|---|---:|---|
| `magic` | `u16` | `0x5254` |
| `type` | `u8` | Packet type |
| `sequence` | `u32` | Server/client packet sequence |
| `payloadSize` | `u16` | Payload size in bytes |

A packet is discarded if the magic is invalid, the payload size does not match
the datagram length, or a required field is truncated. The fields use the same
serialization convention as the existing `PacketStreamWriter` implementation.
That implementation currently writes native-endian values; the Linux and
Windows targets used by the project are little-endian. A portable explicit
endianness conversion should be handled as a shared protocol-hardening change.

## Lobby packet types

All sizes below exclude the 9-byte packet header. Integer fields follow the
existing packet stream representation.

### `RoomListReq` — `0x40` (client to server)

No payload. Requests the currently available rooms.

### `RoomListAck` — `0x41` (server to client)

| Field | Type | Description |
|---|---:|---|
| `count` | `u16` | Number of entries |
| `rooms[count].id` | `u32` | Room identifier |
| `rooms[count].players` | `u8` | Connected players |
| `rooms[count].capacity` | `u8` | Maximum players |
| `rooms[count].state` | `u8` | `0` waiting, `1` running, `2` full |
| `rooms[count].difficulty` | `u8` | `0` easy, `1` normal, `2` hard |

Rooms are returned in ascending ID order. The default room is always present.

### `RoomCreateReq` — `0x42` (client to server)

| Field | Type | Description |
|---|---:|---|
| `capacity` | `u8` | Maximum players, between 1 and 4 |
| `difficulty` | `u8` | `0` easy, `1` normal, `2` hard |

### `RoomCreateAck` — `0x43` (server to client)

| Field | Type | Description |
|---|---:|---|
| `roomId` | `u32` | Created room ID, or `0` on failure |
| `status` | `u8` | `0` success, `1` invalid settings or already connected, `2` room limit reached |

Creating a room does not automatically join it. The client must send
`RoomJoinReq` after receiving a successful response. Repeated create requests
from the same endpoint before it joins are idempotent: the server returns the
same room identifier instead of allocating another room.

### `RoomJoinReq` — `0x44` (client to server)

| Field | Type | Description |
|---|---:|---|
| `roomId` | `u32` | Target room |
| `difficulty` | `u8` | Requested difficulty (`0xFF` means no preference) |
| `skin` | `u8` | Skin identifier (`0xFF` means unspecified) |
| `previousNetId` | `u32` | Previous player ID, or `0` |

On success, the server sends `RoomJoinAck` followed by the room's usual
`ConnectAck`. The `ConnectAck` remains the source of the assigned player ID.

### `RoomJoinAck` — `0x45` (server to client)

| Field | Type | Description |
|---|---:|---|
| `roomId` | `u32` | Requested room ID |
| `status` | `u8` | `0` success, `1` room not found, `2` room full, `3` already joined to another room |

A client can be connected to only one room at a time. Repeating a join request
for the room that already owns the endpoint is idempotent: the server repeats
the success response and connection acknowledgement.

### `RoomLeaveReq` — `0x46` (client to server)

| Field | Type | Description |
|---|---:|---|
| `playerId` | `u32` | Player ID assigned by `ConnectAck` |

The endpoint and player ID must both match the active session. A repeated leave
request returns status `1` because that session is no longer active. `Disconnect`
(`0x03`) remains supported for clients using the original protocol.

### `RoomLeaveAck` — `0x47` (server to client)

| Field | Type | Description |
|---|---:|---|
| `roomId` | `u32` | Room that was left, or `0` if unknown |
| `status` | `u8` | `0` success, `1` invalid session |

### `ConnectReject` — `0x04` (server to client)

| Field | Type | Description |
|---|---:|---|
| `reason` | `u8` | `1` default room is full |

## Server runtime

The UDP receive worker only receives datagrams and appends them to a bounded
queue (up to 4096 packets and 4 MiB). The simulation thread drains that queue,
validates and routes packets, updates every room, and sends the resulting
snapshots. This keeps packet reception independent from simulation updates and
prevents one room from consuming another room's datagrams.

The server exposes these command-line settings:

- `--port <n>`: UDP listen port (default `4242`).
- `--max-rooms <n>`: maximum simultaneous rooms, including room `1` (default
  `16`, allowed range `1..256`).
- `--room-capacity <n>`: default room capacity (default `4`, allowed range
  `1..4`).

User-created room capacity is selected in `RoomCreateReq` and cannot exceed
four players, matching the current game client/gameplay model.

## Running the server tests

The room-manager tests can be configured without downloading SFML or CPM:

```sh
cmake -S . -B build-tests \
  -DRTYPE_BUILD_APPLICATIONS=OFF \
  -DBUILD_TESTING=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Use the default `RTYPE_BUILD_APPLICATIONS=ON` for the regular client/server
build. That path continues to fetch SFML through CPM as before.
