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
| `0x41` | `RoomListAck` | S → C | yes | variable | [6](#6-lobby-and-room-packets) |
| `0x42` | `RoomCreateReq` | C → S | no | | [6](#6-lobby-and-room-packets) |
| `0x43` | `RoomCreateAck` | S → C | yes | | [6](#6-lobby-and-room-packets) |
| `0x44` | `RoomJoinReq` | C → S | no | | [6](#6-lobby-and-room-packets) |
| `0x45` | `RoomJoinAck` | S → C | yes | | [6](#6-lobby-and-room-packets) |
| `0x46` | `RoomLeaveReq` | C → S | no | 12 | [6](#6-lobby-and-room-packets) |
| `0x47` | `RoomLeaveAck` | S → C | yes | | [6](#6-lobby-and-room-packets) |
| `0xE0` | `Ping` | C → S | no | 8 | [9](#9-ack-ping-and-pong) |
| `0xE1` | `Pong` | S → C | no | 8 | [9](#9-ack-ping-and-pong) |

An empty "Payload size" cell is filled in by the section that documents the
packet. Any other type id is unknown: the server counts it as rejected and
ignores it.

## 5. Connection packets

_To be documented: `ConnectReq`, `ConnectAck`, `ConnectReject`, `Disconnect`,
session token, `previousNetId`._

## 6. Lobby and room packets

_To be documented: `RoomListReq` / `RoomListAck`, `RoomCreateReq` /
`RoomCreateAck`, `RoomJoinReq` / `RoomJoinAck`, `RoomLeaveReq` /
`RoomLeaveAck`, result codes._

## 7. Gameplay event packets

_To be documented: `SpawnPlayer`, `SpawnEntity`, `DestroyEntity`,
`HealthUpdate`, `AugmentUpdate`, `ScoreUpdate`, entity and augment ids._

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
