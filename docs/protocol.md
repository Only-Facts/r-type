# R-Type network protocol reference

This document is the reference for the R-Type binary protocol. It is meant to
be complete enough to write an independent client without reading the
project's source code.

Source of truth in the code:

- [Protocol.hpp](../include/common/network/Protocol.hpp): packet types, header, entity state structs.
- [PacketStream.hpp](../include/common/network/PacketStream.hpp): byte encoding of every field.
- [ServerNetworkManager.hpp](../include/server/ServerNetworkManager.hpp): server-side dispatch and validation.
- [ReliableUdpQueue.hpp](../include/server/ReliableUdpQueue.hpp): reliable delivery.
- [ClientNetworkManager.hpp](../include/client/network/ClientNetworkManager.hpp): reference client.

The rationale behind gameplay packets is described in
[protocol-extension.md](protocol-extension.md).

## Contents

1. [Transport](#1-transport)
2. [Encoding conventions](#2-encoding-conventions)
3. [Packet header](#3-packet-header)
4. [Packet types](#4-packet-types)
5. [Connection packets](#5-connection-packets)
6. [Lobby and room packets](#6-lobby-and-room-packets)
7. [Gameplay event packets](#7-gameplay-event-packets)
8. [Inputs and snapshots](#8-inputs-and-snapshots)
9. [Ack, Ping and Pong](#9-ack-ping-and-pong)
10. [States and transitions](#10-states-and-transitions)
11. [Sequencing, reliability, timeouts and errors](#11-sequencing-reliability-timeouts-and-errors)
12. [Size limits and rejection rules](#12-size-limits-and-rejection-rules)
13. [Packet examples](#13-packet-examples)

## 1. Transport

- All traffic uses **UDP**. One datagram carries exactly one packet.
- The server listens on a single UDP port. The client sends from any port and
  only accepts packets coming from the server's port.
- There is no TCP channel, no encryption and no compression layer.
- Packets that need guaranteed delivery are acknowledged and retransmitted at
  the application level (see [section 11](#11-sequencing-reliability-timeouts-and-errors)).

## 2. Encoding conventions

| Type | Size | Encoding |
| --- | --- | --- |
| `u8` | 1 byte | Unsigned integer |
| `u16` | 2 bytes | Unsigned integer, **little-endian** |
| `u32` | 4 bytes | Unsigned integer, **little-endian** |
| `u64` | 8 bytes | Unsigned integer, **little-endian** |
| `i16` | 2 bytes | Two's complement signed integer, **little-endian** |
| `f32` | 4 bytes | IEEE-754 single precision, its 32 bits sent as a little-endian `u32` |
| `bool` | 1 byte | `0x00` = false, `0x01` = true. Any other value makes the packet invalid. |
| `NetworkId` | 4 bytes | Alias of `u32` |

Rules:

- **Byte order is little-endian on the wire**, whatever the host. The
  reference implementation writes and reads integers byte by byte, so it does
  not depend on the machine's byte order. An implementation running on a
  big-endian host must convert every multi-byte field.
- Fields are packed one after another, in the order given by each table.
  There is **no padding and no alignment**.
- Sizes in this document are always in bytes.
- A packet whose payload is shorter than its fields require is invalid and is
  dropped (see [section 12](#12-size-limits-and-rejection-rules)).

## 3. Packet header

Every packet starts with a fixed **9-byte** header, followed by
`payloadSize` bytes of payload.

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `magic` | `u16` | Always `0x5254`. On the wire: `54 52`. |
| 2 | `type` | `u8` | Packet type, see [section 4](#4-packet-types). |
| 3 | `sequence` | `u32` | Sender's sequence number, see [section 11](#11-sequencing-reliability-timeouts-and-errors). |
| 7 | `payloadSize` | `u16` | Number of payload bytes after the header. |

A receiver must drop the datagram if:

- it is shorter than 9 bytes;
- `magic` is not `0x5254`;
- `payloadSize` is not exactly `datagram size - 9`.

Example: header of a `ConnectReq` (`0x01`) with sequence `1` and an empty payload:

```text
54 52  01  01 00 00 00  00 00
magic  type sequence    payloadSize
```

## 4. Packet types

Direction: **C → S** = client to server, **S → C** = server to client.
Reliable: the receiver must answer with an `Ack`, and the sender retransmits
until it gets one (see [section 11](#11-sequencing-reliability-timeouts-and-errors)).

| Id | Name | Direction | Reliable | Payload size | Details |
| --- | --- | --- | --- | --- | --- |
| `0x01` | `ConnectReq` | C → S | no (client retries) | 0, 1, 2 or 6 | [5](#5-connection-packets) |
| `0x02` | `ConnectAck` | S → C | yes | 13 | [5](#5-connection-packets) |
| `0x03` | `Disconnect` | C → S | no | 12 | [5](#5-connection-packets) |
| `0x04` | `ConnectReject` | S → C | yes | | [5](#5-connection-packets) |
| `0x05` | `Ack` | C → S | no | 4 | [9](#9-ack-ping-and-pong) |
| `0x10` | `SpawnPlayer` | S → C | yes | 13 | [7](#7-gameplay-event-packets) |
| `0x11` | `DestroyEntity` | S → C | yes | 4 | [7](#7-gameplay-event-packets) |
| `0x12` | `SpawnEntity` | S → C | yes | 13 | [7](#7-gameplay-event-packets) |
| `0x13` | `HealthUpdate` | S → C | yes | 6 | [7](#7-gameplay-event-packets) |
| `0x14` | `AugmentUpdate` | S → C | yes | 10 | [7](#7-gameplay-event-packets) |
| `0x15` | `ScoreUpdate` | S → C | yes | 8 | [7](#7-gameplay-event-packets) |
| `0x20` | `PlayerInput` | C → S | no | 17 | [8](#8-inputs-and-snapshots) |
| `0x30` | `GameStateSync` | S → C | no | variable | [8](#8-inputs-and-snapshots) (legacy, not sent by the current server) |
| `0x31` | `GameStateSyncPacked` | S → C | no | variable | [8](#8-inputs-and-snapshots) |
| `0x32` | `GameStateSyncDelta` | S → C | no | variable | [8](#8-inputs-and-snapshots) |
| `0x33` | `InputAck` | S → C | no | | [8](#8-inputs-and-snapshots) |
| `0x34` | `SnapshotRequest` | C → S | no | 12 | [8](#8-inputs-and-snapshots) |
| `0x40` | `RoomListReq` | C → S | no | 0 | [6](#6-lobby-and-room-packets) |
| `0x41` | `RoomListAck` | S → C | yes | 2 + 8 × count | [6](#6-lobby-and-room-packets) |
| `0x42` | `RoomCreateReq` | C → S | no | 2 | [6](#6-lobby-and-room-packets) |
| `0x43` | `RoomCreateAck` | S → C | yes | 5 | [6](#6-lobby-and-room-packets) |
| `0x44` | `RoomJoinReq` | C → S | no | 10 | [6](#6-lobby-and-room-packets) |
| `0x45` | `RoomJoinAck` | S → C | yes | 5 | [6](#6-lobby-and-room-packets) |
| `0x46` | `RoomLeaveReq` | C → S | no | 12 | [6](#6-lobby-and-room-packets) |
| `0x47` | `RoomLeaveAck` | S → C | yes | 5 | [6](#6-lobby-and-room-packets) |
| `0xE0` | `Ping` | C → S | no | 8 | [9](#9-ack-ping-and-pong) |
| `0xE1` | `Pong` | S → C | no | 8 | [9](#9-ack-ping-and-pong) |

An empty "Payload size" cell is filled in by the section that documents the
packet. Any other type id is unknown: the server counts it as rejected and
ignores it.

## 5. Connection packets

A client joins a game with `ConnectReq`. The server answers with `ConnectAck`,
which gives the client its player id and a **session token**. Every later
packet that acts on the player (`PlayerInput`, `Disconnect`,
`SnapshotRequest`, `RoomLeaveReq`) must carry both values, and must come from
the same address and port as the `ConnectReq`.

### `ConnectReq` (`0x01`, C → S)

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `difficulty` | `u8` | `0` = easy, `1` = normal, `2` = hard, `0xFF` = no preference. |
| 1 | `skin` | `u8` | Ship skin index, `0xFF` = no preference. |
| 2 | `previousNetId` | `u32` | Player id from a previous game, to keep the same player number. |

The payload is **0, 1, 2 or 6 bytes** long. A field is only present if all
the fields before it are present: to send only `skin`, send
`difficulty = 0xFF` first. Any other payload size is dropped.

Server rules:

- `difficulty` above `2` (other than `0xFF`) is treated as `1`. `0xFF` means `1`.
- The first player of a room chooses the difficulty. Later players get the
  room's current difficulty, whatever they ask for. Rooms created with a
  fixed difficulty ignore the field (see [section 6](#6-lobby-and-room-packets)).
- `previousNetId` is used only if it is in the range `1000`–`1999` and no
  other player of the room uses it. Otherwise the server picks a free id.
- If the sender's address and port are already in a room, the server does not
  create a new player: it sends the same `ConnectAck` again (same id, same
  token) and resends the current world. This makes `ConnectReq` safe to repeat.
- If the sender is not in any room, the request goes to the **default room**.
  If that room is full, the server answers `ConnectReject` with reason `1`.

`ConnectReq` is not acknowledged. The client retries it itself (see
[section 11](#11-sequencing-reliability-timeouts-and-errors)).

### `ConnectAck` (`0x02`, S → C, reliable)

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `netId` | `NetworkId` | Id of the local player, in `1000`–`1999`. |
| 4 | `difficulty` | `u8` | Difficulty actually used by the room: `0`, `1` or `2`. |
| 5 | `sessionToken` | `u64` | Random secret for this player. Never `0`. |

Payload: **13 bytes**.

The client is connected once it has received a `ConnectAck` with a non-zero
token. Right after the `ConnectAck`, the server sends to the new client, in
this order:

1. `SpawnPlayer` + `HealthUpdate` for the new player;
2. `SpawnPlayer` + `HealthUpdate` for every other player of the room;
3. `ScoreUpdate` and active `AugmentUpdate` for every player;
4. `SpawnEntity` (+ `HealthUpdate` for entities with more than 1 HP) for every
   entity already in the game.

The other players of the room receive `SpawnPlayer`, `HealthUpdate` and
`ScoreUpdate` for the new player.

Because these packets travel over UDP, they can arrive **before** the
`ConnectAck`. A client should keep a `SpawnPlayer` received before the
`ConnectAck` and apply it once connected (the reference client keeps up to 4).

### `ConnectReject` (`0x04`, S → C, reliable)

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `reason` | `u8` | Why the connection was refused. |

Payload: **1 byte**.

| Reason | Meaning |
| --- | --- |
| `1` | The default room is full. |

After a `ConnectReject` the client must stop retrying its `ConnectReq`.

### `Disconnect` (`0x03`, C → S)

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `netId` | `NetworkId` | Id received in `ConnectAck`. |
| 4 | `sessionToken` | `u64` | Token received in `ConnectAck`. |

Payload: **12 bytes**.

- The server checks that the id exists, that the token matches and that the
  packet comes from the player's address and port. Otherwise it ignores the
  packet.
- On success, the player is removed, its missiles are destroyed, pending
  reliable packets for its address are cancelled, and the other players
  receive a `DestroyEntity` with the player's id.
- The server does **not** answer the leaving client. `Disconnect` is not
  acknowledged, so it can be lost; in that case the player is removed by the
  inactivity timeout below.

### Inactivity timeout

The server removes a player that has sent no valid `PlayerInput` for
**3 seconds**, exactly as if it had sent a `Disconnect`. A connected client
must therefore keep sending `PlayerInput` packets, even when the player is
dead or not moving (all buttons set to false).

## 6. Lobby and room packets

One server hosts several independent games called **rooms**. Each room has
its own players, entities and score.

- **Room `1` is the default room.** It always exists and is never deleted.
  A plain `ConnectReq` puts the player there. Its difficulty is chosen by its
  first player.
- Other rooms are created with `RoomCreateReq`. Their ids start at `2`, their
  difficulty is fixed at creation, and they are deleted after staying empty
  for **30 seconds**.
- A room holds at most **4** players. The server holds at most **16** rooms,
  the default room included. The server options `--room-capacity` (1 to 4)
  and `--max-rooms` (1 to 128) change these values.
- A client address and port can be in only one room at a time.

Every `*Ack` of this section is reliable. The requests are not: if no answer
comes, the client sends the request again. All requests can be repeated
safely (see each packet).

### `RoomListReq` (`0x40`, C → S)

Empty payload (**0 bytes**). Any other size is dropped.

### `RoomListAck` (`0x41`, S → C, reliable)

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `count` | `u16` | Number of rooms that follow. |
| 2 | `rooms` | `count` × room entry | Sorted by room id, smallest first. |

Room entry (**8 bytes**):

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `roomId` | `u32` | Room id. |
| 4 | `players` | `u8` | Players currently in the room. |
| 5 | `capacity` | `u8` | Maximum number of players. |
| 6 | `state` | `u8` | `0` = waiting (empty), `1` = running, `2` = full. |
| 7 | `difficulty` | `u8` | `0` = easy, `1` = normal, `2` = hard. |

Payload: **2 + 8 × `count` bytes**.

### `RoomCreateReq` (`0x42`, C → S)

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `capacity` | `u8` | Maximum players, `1` to `4`. |
| 1 | `difficulty` | `u8` | `0`, `1` or `2`. `0xFF` is **not** accepted here. |

Payload: **2 bytes**. Any other size is dropped.

Creating a room does **not** join it: the client must then send a
`RoomJoinReq` with the new room id. If the same client sends
`RoomCreateReq` again before joining, the server answers with the room it
already created instead of creating a second one.

### `RoomJoinReq` (`0x44`, C → S)

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `roomId` | `u32` | Room to join. |
| 4 | `difficulty` | `u8` | Same meaning as in `ConnectReq`. Ignored by rooms with a fixed difficulty. |
| 5 | `skin` | `u8` | Same meaning as in `ConnectReq`. |
| 6 | `previousNetId` | `u32` | Same meaning as in `ConnectReq`, `0` if none. |

Payload: **10 bytes**. Any other size is dropped.

When the answer is a success, the server joins the room **for** the client,
as if it had received a `ConnectReq` with the same fields. The client must
not send a `ConnectReq` itself: it receives the `RoomJoinAck`, then the
`ConnectAck` and the world, exactly as described in
[section 5](#5-connection-packets). Joining a room the client is already in
answers success again and resends the `ConnectAck` and the world.

### `RoomLeaveReq` (`0x46`, C → S)

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `netId` | `NetworkId` | Id received in `ConnectAck`. |
| 4 | `sessionToken` | `u64` | Token received in `ConnectAck`. |

Payload: **12 bytes**. Any other size is dropped.

On success, the player leaves the room exactly as with `Disconnect`, and the
server answers `RoomLeaveAck`. For **10 seconds** after a successful leave,
the same request (same address, port, id and token) gets the same success
answer again, so a lost `RoomLeaveAck` can be recovered by repeating the
request.

### `RoomCreateAck`, `RoomJoinAck`, `RoomLeaveAck` (`0x43`, `0x45`, `0x47`, S → C, reliable)

The three answers share the same format:

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `roomId` | `u32` | Room concerned, or `0` when no room applies. |
| 4 | `status` | `u8` | `0` = success, other values below. |

Payload: **5 bytes**.

| Packet | Status | Meaning | `roomId` |
| --- | --- | --- | --- |
| `RoomCreateAck` | `0` | Room created. | New room id |
| `RoomCreateAck` | `1` | Invalid `capacity` or `difficulty`, or the client is already in a room. | `0` |
| `RoomCreateAck` | `2` | Room limit reached. | `0` |
| `RoomJoinAck` | `0` | Joined (or already in this room). | Requested id |
| `RoomJoinAck` | `1` | No room with this id. | Requested id |
| `RoomJoinAck` | `2` | Room full. | Requested id |
| `RoomJoinAck` | `3` | The client is already in another room. It must leave first. | Requested id |
| `RoomLeaveAck` | `0` | Left the room. | Room left |
| `RoomLeaveAck` | `1` | Not in a room, or wrong id or token. | Current room, or `0` |

## 7. Gameplay event packets

These packets tell the client what exists in the game. Positions are moved
afterwards by snapshots (see [section 8](#8-inputs-and-snapshots)).
All of them are **reliable**.

Conventions:

- Positions are `f32` in pixels, in a **1920 × 1080** world, with the origin
  at the top-left corner and Y going down.
- Player ids are in `1000`–`1999`. Other entities use ids from `2000`.
  Ids can be reused after an entity is destroyed.
- UDP can deliver an event about an entity before its `SpawnPlayer` or
  `SpawnEntity`. A client should keep, for a short time, events about an id
  it does not know yet, and apply them once the entity is spawned.

### `SpawnPlayer` (`0x10`, S → C, reliable)

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `netId` | `NetworkId` | Player id. If it equals the id from `ConnectAck`, this is the local player. |
| 4 | `posX` | `f32` | Start position. |
| 8 | `posY` | `f32` | Start position. |
| 12 | `skin` | `u8` | Ship skin index, `0xFF` = none. |

Payload: **13 bytes**. Sent when a player joins a room, to every player of
that room.

### `SpawnEntity` (`0x12`, S → C, reliable)

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `netId` | `NetworkId` | Entity id. |
| 4 | `type` | `u8` | Entity type, see the table below. |
| 5 | `posX` | `f32` | Start position. |
| 9 | `posY` | `f32` | Start position. |

Payload: **13 bytes**.

| Type | Entity |
| --- | --- |
| `0` | Player. Reserved: players are always announced by `SpawnPlayer`. |
| `1` | Bydos (regular enemy). |
| `2` | Player missile. |
| `3` | Enemy missile. Also used for the boss shots. |
| `4` | Bonus: double shot. |
| `5` | Bonus: triple shot. |
| `6` | Bonus: heal. |
| `7` | Bonus: damage up. |
| `8` | Player missile with damage up (bigger sprite). |
| `9` | Stage boss. |

The reference client also defines type `10` (boss shot) in its configuration,
but the current server never sends it. A client should ignore unknown types.

### `DestroyEntity` (`0x11`, S → C, reliable)

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `netId` | `NetworkId` | Id of the player or entity to remove. |

Payload: **4 bytes**. Sent when an entity dies, is collected or leaves the
screen, and when a player leaves the room (`Disconnect`, `RoomLeaveReq` or
timeout). A player whose health reaches `0` is **not** destroyed: it stays in
the room (see `HealthUpdate`).

### `HealthUpdate` (`0x13`, S → C, reliable)

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `netId` | `NetworkId` | Player or entity. |
| 4 | `health` | `u8` | Current health. `0` = dead. |
| 5 | `maxHealth` | `u8` | Maximum health. |

Payload: **6 bytes**. Sent for every player when it joins and when its health
changes, and for entities with more than 1 HP (the boss, stronger Bydos).
Players start with 8 HP in easy, 5 in normal and 4 in hard.

### `AugmentUpdate` (`0x14`, S → C, reliable)

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `netId` | `NetworkId` | Player who has the augment. |
| 4 | `augment` | `u8` | Augment id, see below. |
| 5 | `level` | `u8` | Stack level. `1` for augments that do not stack. |
| 6 | `remaining` | `f32` | Seconds left. `0` = the augment ended. |

Payload: **10 bytes**. Sent to every player of the room when a player gains,
refreshes or loses an augment, and to a joining client for every active
augment. The current server always sends `remaining >= 0`; a client should
treat a negative value as "permanent".

| Augment | Meaning | Levels |
| --- | --- | --- |
| `1` | Double shot | 1 |
| `2` | Triple shot (replaces double shot) | 1 |
| `3` | Damage up | 1 to 2 |

### `ScoreUpdate` (`0x15`, S → C, reliable)

| Offset | Field | Type | Description |
| --- | --- | --- | --- |
| 0 | `netId` | `NetworkId` | Player whose score changed. |
| 4 | `score` | `u32` | New **total** score of that player (not a difference). |

Payload: **8 bytes**. Sent to every player of the room when a score changes,
and to a joining client for every player.

### Event ordering

Each event carries the server's `sequence` number. Because a retransmitted
event can arrive after a newer one, a client should apply a `HealthUpdate`,
`AugmentUpdate` or `ScoreUpdate` only if its `sequence` is newer than the last
one applied for the same player (and, for augments, the same augment id).
See [section 11](#11-sequencing-reliability-timeouts-and-errors) for how to
compare sequence numbers.

## 8. Inputs and snapshots

_To be documented: `PlayerInput`, `InputAck`, `GameStateSync`,
`GameStateSyncPacked`, `GameStateSyncDelta`, `SnapshotRequest`, position
quantization._

## 9. Ack, Ping and Pong

_To be documented._

## 10. States and transitions

_To be documented: connection, lobby, room and game state machines._

## 11. Sequencing, reliability, timeouts and errors

_To be documented: meaning of `sequence`, reliable packets, `Ack`,
retransmission timing, timeouts and error codes._

## 12. Size limits and rejection rules

_To be documented: maximum datagram size, queue limits, rate limiting and
every rule that makes a packet be dropped._

## 13. Packet examples

_To be documented: hex dumps generated from the implementation._
