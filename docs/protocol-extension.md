# Gameplay protocol: entities, shooting, health, difficulty, augments and score

> **The byte-level reference is [protocol.md](protocol.md).** It gives the
> fields, sizes and byte order of every packet, the state machines, the
> reliability rules and checked packet examples. This document explains
> **why** the gameplay packets exist and the **game rules** the server
> applies. When the two documents disagree, `protocol.md` and the code are
> right.

Status: implemented by the authoritative server in `include/server/GameRoom.hpp`
and consumed by `include/client/network/ClientNetworkManager.hpp`. The client
visuals of every entity type are configured in `assets/config/client.cfg`.

The protocol defines packets for entity spawns (`SpawnEntity`), health
(`HealthUpdate`), temporary augments (`AugmentUpdate`) and score (`ScoreUpdate`).
The server remains authoritative: clients send inputs and render the state decided by
the server.

## Why

The first version of the protocol only knew players: `SpawnPlayer`,
`DestroyEntity`, `GameStateSync` and a 4-direction `PlayerInput`. Part 1 of the
subject requires Bydos, missiles and shooting, so the client needs to know *what* an
entity is, and the server needs to know when a player fires. The packets below were
added for that, and later extended with authentication, rooms, compressed snapshots
and reliable delivery.

## 1. `SpawnEntity` (server -> client), type `0x12`

Layout: [protocol.md, section 7](protocol.md#spawnentity-0x12-s--c-reliable).

Sent to every client when an entity appears, and to a newly connected client for
every entity already alive. Players keep using `SpawnPlayer`. After the spawn, the
entity is moved by snapshots (section 11) and removed by `DestroyEntity`.

### Entity types

| Id | Entity | Client profile (`[entity_types]` in `client.cfg`) |
| --- | --- | --- |
| 0 | Player (reserved, announced by `SpawnPlayer`) | - |
| 1 | Bydos slave | `bydos_slave` |
| 2 | Player missile | `player_missile` |
| 3 | Enemy missile, also used for the boss shots | `enemy_missile` |
| 4 | Bonus: double shot | `pickup_double_shot` |
| 5 | Bonus: triple shot | `pickup_triple_shot` |
| 6 | Bonus: heal | `pickup_heal` |
| 7 | Bonus: damage up | `pickup_damage` |
| 8 | Player missile, damage boosted (bigger sprite) | `player_missile_heavy` |
| 9 | Stage boss | `boss_mecha` (with `boss = true`) |
| 10 | Boss shot (client profile only, never sent by the current server) | `boss_mecha_shot` |

New ids need no networking code on the client: add one line in `[entity_types]`
and one `[entity:<name>]` section (sprite, depth, sounds). The `boss = true` flag
of an entity profile activates the boss health bar, warning notification and boss
music in `GameScene`.

## 2. `PlayerInput` carries a session token and a `shoot` byte (client -> server)

Layout and server checks: [protocol.md, section 8](protocol.md#playerinput-0x20-c--s).

The input only says which buttons are held. The server validates the endpoint,
player id, session token, input sequence and the five boolean bytes before applying
it. Diagonal movement is normalized to the same speed as horizontal or vertical
movement. The server controls the fire rate (0.25 seconds); clients never create
gameplay entities themselves.

## 3. `HealthUpdate` (server -> client), type `0x13`

Layout: [protocol.md, section 7](protocol.md#healthupdate-0x13-s--c-reliable).

The server sends it:

- right after `SpawnPlayer` / `SpawnEntity` for entities that have more than 1 HP
  (also to a newly connected client, for every entity already alive);
- every time health changes (hit, heal);
- with `health = 0` when an entity dies. A dead player stays in the room for 1.5 s,
  so everybody sees the explosion, then is removed and the others receive
  `DestroyEntity`.

What the client does with it: health bar of the local player ("HP 3/5", green then red
under 30%), red flash and hit sound on any entity that loses health, "Player N was
destroyed" for other players, and a "DESTROYED" screen for the local player.

## 4. Authoritative game logic

The server owns positions, collisions, health, death, damage, drops, score and stage
progression. Clients only submit player inputs and render the state and events they
receive.

| Rule | Server behavior |
| --- | --- |
| Player health | Easy 8, normal 5, hard 4 |
| Bydos health | Easy 1, normal 2, hard 3; increases by one every three completed stages, capped at 8 |
| Player missile hits Bydos/boss | Apply the missile's damage, destroy the missile, update target health |
| Enemy missile hits player | Player -1 HP, missile destroyed |
| Player rams Bydos | Player -1 HP, Bydos destroyed; no score |
| Fire rate | One trigger every 0.25 seconds while `shoot` is held |
| Dead player | Ignores movement/fire, loses active augments and is removed after 1.5 seconds |
| Off-screen missile, enemy or bonus | `DestroyEntity` |
| Player bounds | Server clamps player position to the 1920x1080 game area |

Collisions use axis-aligned bounds. The server also applies a short hit cooldown so a
player overlapping a boss or projectile is not damaged on every simulation tick.

### Enemy fire direction

Enemies shoot **forward**, to the left of the screen, like in the original R-Type.
A player who flew past an enemy (to its right) must never be hit by its shots:

- easy and normal: every enemy missile goes straight left (`velY = 0`);
- hard: the missile may tilt toward the nearest living player **in front of** the enemy
  (player `x` smaller than enemy `x`), by at most 30 degrees. Players behind are ignored.

In every mode `velX` of an enemy missile is negative. Ramming an enemy still hurts,
wherever the player comes from.

## 5. Difficulty and authenticated connection handshake

Layout: [protocol.md, section 5](protocol.md#5-connection-packets).

The player picks the difficulty on the title screen. The client sends it in
`ConnectReq`, the server decides and confirms it in `ConnectAck`, together with a
newly generated session token. A room uses the first player's requested difficulty
unless the room was created with a fixed difficulty; later players inherit the
room's selected difficulty. Every later packet that acts on the player must carry
the session token.

| Rule | Easy | Normal | Hard |
| --- | --- | --- | --- |
| Player health | 8 | 5 | 4 |
| Bydos health | 1 | 2 | 3 |
| Enemy shots per second (per Bydos) | 0.25 | 0.5 | 0.9 |
| Enemy missile speed (px/s) | 320 | 420 | 520 |
| New Bydos every (s) | 1.6 | 1.2 | 0.8 |
| Enemy aiming | straight | straight | up to 30 degrees, only at players in front |

Labels, colors and the order of the levels on the client live in the `[difficulty]`
section of `client.cfg`; the position of a level in `levels` is the value on the wire.

## 6. Augments: bonuses picked up in game

Layout of `AugmentUpdate` (`0x14`):
[protocol.md, section 7](protocol.md#augmentupdate-0x14-s--c-reliable).

A bonus is an ordinary entity: `SpawnEntity` (types 4 to 7), moved by snapshots,
removed by `DestroyEntity` when it is collected or leaves the screen. Its effect is
announced with `HealthUpdate` (heal) or `AugmentUpdate`, sent to every client when a
player gains an augment, when it is refreshed or stacked, and with `remaining = 0`
when it ends (timer, death).

| Id | Augment | Client profile (`[augments]` in `client.cfg`) |
| --- | --- | --- |
| 1 | Double shot | `double_shot` |
| 2 | Triple shot | `triple_shot` |
| 3 | Damage up | `damage_up` |

What the client shows: the local player's active augments under the health bar with a
countdown ("Triple shot 8s", "Damage x2 10s"), a floating label above the ship of any
player who picks one up, "+2 HP" above a healed ship, and sounds on gain and end.

### Augment rules implemented on the server

| Rule | Value |
| --- | --- |
| Drop chance when a Bydos is destroyed | easy 40%, normal 30%, hard 20% |
| Bonus movement | drifts left at 80 px/s, removed off screen |
| Pickup | a living player overlaps the bonus (28x28 box) |
| Heal (type 6) | +2 HP, capped at max health, send `HealthUpdate` |
| Double shot (augment 1) | 2 parallel missiles, 10 s |
| Triple shot (augment 2) | 3 missiles in a fan (-8, 0, +8 degrees), 10 s, replaces double shot |
| Double shot while triple is active | refreshes the triple shot instead (never downgrade) |
| Damage up (augment 3) | stacks up to level 2, each pickup refreshes 10 s; missiles deal 1 + level damage and use type 8 |
| Death | every active augment of the player ends (`remaining = 0`) |

Damage-up and shot-pattern augments have independent timers.

## 7. Skins and player number

Layout: `skin` and `previousNetId` in
[`ConnectReq`](protocol.md#connectreq-0x01-c--s) and `skin` in
[`SpawnPlayer`](protocol.md#spawnplayer-0x10-s--c-reliable).

After the difficulty, the player picks a ship on the skin screen. The skin is purely
visual: it never changes the gameplay. The server only relays it to the other
players in every `SpawnPlayer`. `0xFF` means no preference. Unknown indices are kept
as is; the client falls back to its configured default appearance.

### Keeping the player number: `previousNetId`

The client shows "Player N" from its network id. After a game over the player goes
back to the menu and reconnects, so without help it would get a new id and a new
number. When it reconnects, the client sends the id it had in its previous game. The
server restores it when no other player of the room uses it; otherwise it allocates
another player id.

## 8. Stage progression and boss

A room starts at stage 1. After ten Bydos are eliminated during the current stage,
the server stops regular Bydos spawning, removes any remaining regular Bydos and
spawns one boss as entity type `9`. The boss is a normal synchronized entity, with
`SpawnEntity`, snapshots, `HealthUpdate` and `DestroyEntity`.

Boss health scales with room size, difficulty and stage:

```text
boss_hp = 45 + (players * 8) + (stage * 12) + (difficulty * 12)
```

The boss moves into the arena and oscillates vertically. It fires a three-projectile
spread every second (entity type `3`). Its collision bounds are larger than a regular
Bydos. Destroying the boss awards 1000 points, advances the stage, resets the
per-stage Bydos counter and resumes regular enemy spawning. The next stage gradually
increases Bydos speed, spawn rate and health. Killing a regular Bydos with a player
missile awards 100 points.

On the client, the `[entity:boss_mecha]` profile of `client.cfg` (entity type `9`,
`boss = true`) gives the boss its sprite, label and color, and activates the boss HUD
and music.

## 9. Score: `ScoreUpdate` (server -> client), type `0x15`

Layout: [protocol.md, section 7](protocol.md#scoreupdate-0x15-s--c-reliable).

The server keeps one total score per player and sends it to a newly connected client
for every player already in the room, and to every room member whenever a score
changes. The player whose missile destroys an enemy receives its points: Bydos 100,
boss 1000. Ramming an enemy gives no points. The client displays the local player's
score in its score HUD.

## 10. Server implementation notes

- `Disconnect` (`0x03`) is accepted only from the endpoint bound to the session,
  with the right session token.
- Player ids (`1000`-`1999`) and entity ids (from `2000`) use separate counters.
- A repeated `ConnectReq` from an already connected endpoint resends the connection
  acknowledgement and current room state instead of creating another player.
- Server packets share one increasing outgoing sequence counter, including packets
  sent by different rooms. Each room broadcasts identical snapshot datagrams to its
  own recipients.
- The server rejects malformed headers and payload lengths before dispatching
  packets. The full list of rules is in
  [protocol.md, section 12](protocol.md#12-size-limits-and-rejection-rules).
- Snapshot keyframes and deltas each fit in one UDP datagram; the server does not
  rely on IP fragmentation.

## 11. Full and differential snapshots

Layouts, decoding steps and validation rules:
[protocol.md, section 8](protocol.md#snapshots).

Entity states are quantized (`packEntityState`): position in 1/8 px and velocity in
1/4 px/s, stored as `i16`. Every snapshot carries the server `tick`, the number of
fixed simulation steps (60 per second) run by the room since its last reset, so two
snapshots are always ordered and `(tickB - tickA) / 60` seconds apart.

Design choices:

- A delta only carries the **fields** that changed (`mask`), so an enemy flying
  horizontally only sends `posX`. Encoding, decoding and application are shared by
  the server and the client (`include/common/network/SnapshotDelta.hpp`).
- The base of a delta is always the last snapshot the server sent to every client
  of the room, not a base acknowledged by each client. When nothing changed, nothing
  is sent and the base stays the same.
- The client keeps the last 64 reconstructed states by tick. A delta whose base is
  unknown waits up to 100 ms (it may have overtaken its base on the network); when
  the base arrives it is applied, otherwise the client asks for a full state with
  `SnapshotRequest`.
- Unchanged entities are still part of the reconstructed state, so every remote
  entity gets one interpolation sample per snapshot.
- Creation and destruction of entities keep their reliable `SpawnEntity` /
  `DestroyEntity` events (type, sound, explosion). Snapshots only carry positions.
- A full state is sent to a client when it joins, when it asks for one, and to
  everybody every 60 snapshots as a safety net. When only one client needs a full
  state, the others still receive a delta (possibly empty) for the same tick, so
  every client keeps the same base.

### Measured bandwidth

`server_gameplay_test` (`testSnapshotBandwidthOnRepresentativeScene`) plays 15
seconds with 4 players (two moving and shooting, one shooting in place, one idle)
with the normal enemy waves, missiles and bonuses, and counts the snapshot bytes
sent to the 4 clients:

| Snapshots | Bandwidth |
| --- | --- |
| Full state every tick | about 100 to 110 KiB/s |
| Previous delta (whole 12-byte record of every changed entity) | about 95 to 104 KiB/s |
| Field delta with base, removals and periodic full state | about 56 to 65 KiB/s, 41 % less than full |

Most entities move every tick, so the gain comes from the fields: an enemy flying
horizontally only sends `posX` (7 bytes instead of 12), and a constant velocity is
never repeated. The server log shows the live split every 10 s
(`snapshots(full=... KiB/s, delta=... KiB/s, requests=...)`), and the client debug
overlay (F3) shows the snapshot bandwidth it receives.

## 12. Reliable event delivery

Rules and timings:
[protocol.md, section 11](protocol.md#11-sequencing-reliability-timeouts-and-errors).

The server reliably transmits connection and lobby answers, entity spawn and
destruction events, health, augment and score updates: these change the game state
once and are never repeated by a newer packet. `PlayerInput` and snapshots remain
unreliable because a newer input or snapshot replaces an older one.

## 13. Input steps and `InputAck` (server -> client), type `0x33`

Layouts: [protocol.md, section 8](protocol.md#8-inputs-and-snapshots).

The header `sequence` of a `PlayerInput` packet is its input number, and each
`PlayerInput` is one movement step of `1/60` second. The client sends them at a fixed
60 Hz rate, whatever its frame rate. The server queues them per player (8 at most)
and applies at most 2 per simulation tick, in order, so a client cannot move faster
by sending more inputs. A tick without a queued input does not move the player. The
movement itself (speed 300 px/s, normalized diagonal, world bounds) is the shared
`gameplay::applyMovement` (`include/common/gameplay/PlayerMovement.hpp`), used by
both the server and the client prediction.

After every simulation update the server sends each player, unreliably, the result of
its own inputs in an `InputAck`. The client predicts its own movement immediately,
then uses this packet to replace its position with the confirmed one and replay the
inputs the server has not applied yet. Collisions, damage and death stay decided by
the server.

## Testing

The runtime server implements the same authoritative gameplay rules. The optional
`tools/mock_server.py` reference implementation, when present in your working copy,
can still be used to test client rendering independently:

```text
python3 tools/mock_server.py 4242                     # difficulty chosen by the first player
python3 tools/mock_server.py 4242 --difficulty hard   # difficulty forced
./r-type_client
```

The packet examples of `protocol.md` are checked by `rtype_protocol_examples_test`.
