# Gameplay protocol: entities, shooting, health, difficulty, augments and score

Status: implemented by the authoritative server in `include/server/GameRoom.hpp`
and consumed by `include/client/network/ClientNetworkManager.hpp`. The client-side
asset/configuration files are not part of the supplied source archive; a boss visual
profile must therefore be configured by the game distribution (see the boss section).

The protocol defines packets for entity spawns (`SpawnEntity`), health
(`HealthUpdate`), temporary augments (`AugmentUpdate`) and score (`ScoreUpdate`).
The server remains authoritative: clients send inputs and render the state decided by
the server.

## Why

The current protocol only knows players: `SpawnPlayer`, `DestroyEntity`,
`GameStateSync` and a 4-direction `PlayerInput`. Part 1 of the subject requires
Bydos, missiles and shooting, so the client needs to know *what* an entity is,
and the server needs to know when a player fires.

## Conventions (unchanged)

- UDP, little-endian, packed header: `magic u16 = 0x5254`, `type u8`, `sequence u32`,
  `payloadSize u16` (9 bytes).
- `NetworkId` is `u32`. Floats are IEEE 754 `f32`.

## 1. New packet: `SpawnEntity` (server -> client), type `0x12`

| Field   | Type  | Description |
|---------|-------|-------------|
| netId   | u32   | Network id of the new entity |
| type    | u8    | Entity type, see table below |
| posX    | f32   | Initial position |
| posY    | f32   | Initial position |

Payload size: 13 bytes. Sent to every client when an entity appears, and to a newly
connected client for every entity already alive (same as `SpawnPlayer` today).
Players keep using `SpawnPlayer`.

After the spawn, the entity is updated by `GameStateSyncPacked` keyframes and
`GameStateSyncDelta` updates (both carry `netId`, position and velocity), and removed
by the existing `DestroyEntity`. The snapshot formats are described in section 11.

### Entity types

| Id | Entity | Client profile (`[entity_types]` in `client.cfg`) |
|----|--------|--------------------------------------------------|
| 0  | Player (reserved, announced by `SpawnPlayer`) | - |
| 1  | Bydos slave | `bydos_slave` |
| 2  | Player missile | `player_missile` |
| 3  | Enemy missile | `enemy_missile` |
| 4  | Bonus: double shot | `pickup_double_shot` |
| 5  | Bonus: triple shot | `pickup_triple_shot` |
| 6  | Bonus: heal | `pickup_heal` |
| 7  | Bonus: damage up | `pickup_damage` |
| 8  | Player missile, damage boosted (bigger sprite) | `player_missile_heavy` |
| 9  | Stage boss | `boss` (set `boss = true` in its entity profile) |

New ids need no networking code on the client: add one line in `[entity_types]`
and one `[entity:<name>]` section (sprite, depth, sounds). For the boss, configure
entity type `9` and set `boss = true`; this flag activates the boss health bar,
warning notification and boss music in `GameScene`.

The common `PacketType` enum reserves `SpawnEntity = 0x12`, `HealthUpdate = 0x13`,
`AugmentUpdate = 0x14`, and `ScoreUpdate = 0x15`.

## 2. `PlayerInput` carries a session token and `shoot` byte (client -> server)

| Field | Type | Description |
|---|---:|---|
| `netId` | `u32` | Sender's network ID |
| `sessionToken` | `u64` | Capability returned by `ConnectAck` |
| `up` | `u8` | `0` or `1` |
| `down` | `u8` | `0` or `1` |
| `left` | `u8` | `0` or `1` |
| `right` | `u8` | `0` or `1` |
| `shoot` | `u8` | `0` or `1`; true while the fire key is held |

Payload size: 17 bytes. The server validates the endpoint, player ID, session token,
input sequence and all five boolean bytes before applying input. Diagonal movement is
normalized to the same speed as horizontal or vertical movement. The server controls
the fire rate (0.25 seconds); clients never create gameplay entities themselves.

## 3. New packet: `HealthUpdate` (server -> client), type `0x13`

| Field     | Type | Description |
|-----------|------|-------------|
| netId     | u32  | Entity whose health changed |
| health    | u8   | New health (0 = destroyed) |
| maxHealth | u8   | Maximum health, for the HUD bar |

Payload size: 6 bytes. The server sends it:

- right after `SpawnPlayer` / `SpawnEntity` for entities that have more than 1 HP
  (also to a newly connected client, for every entity already alive);
- every time health changes (hit, heal);
- with `health = 0` when an entity dies. For a player, keep the entity a short moment
  (about 1.5 s, so everybody sees the explosion) then send `DestroyEntity`.

What the client does with it: health bar of the local player ("HP 3/5", green then red
under 30%), red flash and hit sound on any entity that loses health, "Player N was
destroyed" for other players, and a "DESTROYED" screen for the local player (it then
sends neutral inputs only, to stay connected).

## 4. Authoritative game logic

The server owns positions, collisions, health, death, damage, drops, score and stage
progression. Clients only submit player inputs and render the state and events they
receive.

| Rule | Server behavior |
|------|-----------------|
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

The player picks the difficulty on the title screen. The client sends it, the server
decides and confirms it.

| Packet | New field (appended, optional) | Description |
|--------|-------------------------------|-------------|
| `ConnectReq` (client -> server) | `difficulty u8` | Requested level: 0 = easy, 1 = normal, 2 = hard. `payloadSize = 1` |
| `ConnectAck` (server -> client) | `difficulty u8`, then `sessionToken u64`, after `netId` | Level actually used by the game and the session capability. `payloadSize = 13` |

The server reads the optional byte and includes the selected difficulty and a newly
generated session token in `ConnectAck`. A room uses the first player's requested
difficulty unless the room was created with a fixed difficulty; later players inherit
the room's selected difficulty. A client that only reads `netId` can still parse the
prefix of the extended `ConnectAck`, but it cannot send authenticated gameplay input
without understanding the session token.

| Rule | Easy | Normal | Hard |
|------|------|--------|------|
| Player health | 8 | 5 | 4 |
| Bydos health | 1 | 2 | 3 |
| Enemy shots per second (per Bydos) | 0.25 | 0.5 | 0.9 |
| Enemy missile speed (px/s) | 320 | 420 | 520 |
| New Bydos every (s) | 1.6 | 1.2 | 0.8 |
| Enemy aiming | straight | straight | up to 30 degrees, only at players in front |

Labels, colors and the order of the levels on the client live in the `[difficulty]`
section of `client.cfg`; the position of a level in `levels` is the value on the wire.

## 6. Augments: bonuses picked up in game

A bonus is an ordinary entity: `SpawnEntity` (types 4 to 7), moved by `GameStateSync`,
removed by `DestroyEntity` when it is collected or leaves the screen. Its effect is
announced with the existing `HealthUpdate` (heal) or with one new packet.

### New packet: `AugmentUpdate` (server -> client), type `0x14`

| Field     | Type | Description |
|-----------|------|-------------|
| netId     | u32  | Player who has the augment |
| augment   | u8   | Augment id, see table below |
| level     | u8   | Stack level (1 for non-stacking augments) |
| remaining | f32  | Seconds left. `0` = the augment ended. Negative = permanent |

Payload size: 10 bytes. Sent to every client when a player gains an augment, when it is
refreshed or stacked, and with `remaining = 0` when it ends (timer, death).

| Id | Augment | Client profile (`[augments]` in `client.cfg`) |
|----|---------|-----------------------------------------------|
| 1  | Double shot | `double_shot` |
| 2  | Triple shot | `triple_shot` |
| 3  | Damage up | `damage_up` |

What the client shows: the local player's active augments under the health bar with a
countdown ("Triple shot 8s", "Damage x2 10s"), a floating label above the ship of any
player who picks one up, "+2 HP" above a healed ship, and sounds on gain and end.

### Augment rules implemented on the server

| Rule | Suggested value |
|------|-----------------|
| Drop chance when a Bydos is destroyed | easy 40%, normal 30%, hard 20% |
| Bonus movement | drifts left at 80 px/s, removed off screen |
| Pickup | a living player overlaps the bonus (28x28 box) |
| Heal (type 6) | +2 HP, capped at max health, send `HealthUpdate` |
| Double shot (augment 1) | 2 parallel missiles, 10 s |
| Triple shot (augment 2) | 3 missiles in a fan (-8, 0, +8 degrees), 10 s, replaces double shot |
| Double shot while triple is active | refreshes the triple shot instead (never downgrade) |
| Damage up (augment 3) | stacks up to level 2, each pickup refreshes 10 s; missiles deal 1 + level damage and use type 8 |
| Death | every active augment of the player ends (`remaining = 0`) |

Double-shot creates two parallel projectiles. Triple-shot creates three projectiles in
a fan at -8, 0 and +8 degrees. A triple-shot pickup replaces double shot; a double-shot
pickup while triple shot is active only refreshes the triple-shot duration. Damage-up
and shot-pattern augments have independent timers. The server sends a zero-remaining
`AugmentUpdate` when an effect expires or the player dies.

## 7. Skins: `ConnectReq` and `SpawnPlayer` get one byte

After the difficulty, the player picks a ship on the skin screen. The skin is purely
visual: it never changes the gameplay. The server only has to relay it to the other
players.

| Packet | New field (appended, optional) | Description |
|--------|-------------------------------|-------------|
| `ConnectReq` (client -> server) | `skin u8`, after `difficulty` | Index of the chosen ship in the client `[skins] list` (0 = first). `payloadSize = 2` |
| `SpawnPlayer` (server -> client) | `skin u8`, after `posY` | Skin of that player, copied from its `ConnectReq`. `0xFF` = none. `payloadSize = 13` |

When the client sends a skin, the difficulty byte is always present: `0xFF` there means
"no preference" (the server keeps its own choice).

### Keeping the player number: `previousNetId`

The client shows "Player N" from its network id. After a game over the player goes back to
the menu and reconnects, so without help it would get a new id and a new number. When it
reconnects, the client appends the id it had in its previous game:

| Packet | New field (appended, optional) | Description |
|--------|-------------------------------|-------------|
| `ConnectReq` (client -> server) | `previousNetId u32`, after `skin` | Id of the client's previous game. `payloadSize = 6`; `difficulty` and `skin` are then always present (`0xFF` = no preference) |

The server restores `previousNetId` when that ID is not already used in the room;
otherwise it allocates another player ID. The server relays the skin byte in every
`SpawnPlayer`. A skin value of `0xFF` means no preference. Unknown indices are retained
as metadata; the client falls back to its configured default appearance.

## 8. Stage progression and boss

A room starts at stage 1. After ten Bydos are eliminated during the current stage,
the server stops regular Bydos spawning, removes any remaining regular Bydos and
spawns one boss as entity type `9`. The boss is a normal synchronized entity, with
`SpawnEntity`, `GameStateSync`, `HealthUpdate` and `DestroyEntity` updates.

Boss health scales with room size, difficulty and stage:

```text
boss_hp = 45 + (players * 8) + (stage * 12) + (difficulty * 12)
```

The boss moves into the arena and oscillates vertically. It fires a three-projectile
spread every second. Its collision bounds are larger than a regular Bydos. Destroying
the boss awards 1000 points, advances the stage, resets the per-stage Bydos counter
and resumes regular enemy spawning. The next stage gradually increases Bydos speed,
spawn rate and health. Killing a regular Bydos with a player missile awards 100 points.

To render the boss in the client, the game asset configuration must contain an entry
for entity type `9` and an entity profile with `boss = true`, a valid sprite asset,
label, and color. That profile is what activates the client's existing boss HUD and
music handling. The supplied ZIP does not contain the runtime asset/config files, so
those visual values cannot be edited or validated from this source archive.

## 9. Score: new packet `ScoreUpdate` (server -> client), type `0x15`

| Field | Type | Description |
|-------|------|-------------|
| netId | u32  | Player whose score changed |
| score | u32  | New total score of that player |

Payload size: 8 bytes. The server keeps one score per player and sends it to a newly connected client for
every player already in the room, and to every room member whenever a score changes.
The player whose missile destroys an enemy receives its points: Bydos 100, boss 1000.
Ramming an enemy gives no points. The client displays the local player's score in its
existing score HUD.

## 10. Server implementation notes

- `Disconnect` (`0x03`) carries `netId u32` followed by `sessionToken u64` (12-byte
  payload), and is accepted only from the endpoint bound to that session.
- Player IDs and entity IDs use separate counters/ranges.
- A repeated `ConnectReq` from an already connected endpoint resends the connection
  acknowledgement and current room state instead of creating another player.
- Server packets share a monotonically increasing outgoing sequence counter, including
  packets sent by different rooms. Each room broadcasts identical snapshot datagrams
  to its own recipients.
- The server rejects malformed headers and payload lengths before dispatching packets.
- Snapshot keyframes and deltas each fit in one UDP datagram; the server does not
  rely on IP fragmentation.


## 11. Snapshot keyframes and deltas

The server sends packed entity states using 12-byte records (`netId`, quantized
position, and quantized velocity). `GameStateSyncPacked` (`0x31`) is a full
keyframe. `GameStateSyncDelta` (`0x32`) contains only entities whose packed
state changed since the previous snapshot. Deltas are intentionally unreliable;
a full keyframe is sent at least every 60 server snapshot updates so packet loss
cannot leave a client permanently out of sync. Entity creation and destruction
remain separate reliable events.

Both packet types start with a `u16` state count followed by that many packed
records. A delta is applied as a set of updates, not as a replacement for the
client's complete entity list. Clients discard stale snapshot sequence numbers.

## 12. Reliable event delivery

The server reliably transmits connection/lobby responses, entity spawn and
destruction events, health, augment, and score updates. The receiver responds
with an `Ack` (`0x05`) whose four-byte payload is the acknowledged packet's
sequence number. Reliable packets are retried with exponential backoff and a
bounded retry count; duplicate packets are acknowledged but not applied twice.
`PlayerInput` and snapshots remain unreliable because newer state supersedes
older state.

## Testing

The runtime server implements the same authoritative gameplay rules. The optional
`tools/mock_server.py` reference implementation, when present in your working copy,
can still be used to test client rendering independently:

```
python3 tools/mock_server.py 4242                     # difficulty chosen by the first player
python3 tools/mock_server.py 4242 --difficulty hard   # difficulty forced
./r-type_client
```
