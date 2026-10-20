# Protocol extension proposal: typed entities, shooting, health, difficulty and augments

Status: proposal from the client side, implemented in the client and in
`tools/mock_server.py`, not yet in the server.

Summary: one new packet to show enemies and missiles (`SpawnEntity`), one to show
health (`HealthUpdate`), one optional byte to shoot (`PlayerInput`), one optional byte to
choose the difficulty (`ConnectReq` / `ConnectAck`), one to show augments (`AugmentUpdate`). Fully backward compatible with the
current protocol (`common/network/Protocol.hpp`).

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

After the spawn, the entity is updated by the existing `GameStateSync` (it is generic:
`netId, posX, posY, velX, velY`) and removed by the existing `DestroyEntity`.

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

New ids need no client code: one line in `[entity_types]` and one `[entity:<name>]`
section (sprite, depth, sounds).

Suggested server-side enum, to add to `Protocol.hpp`:

```cpp
enum class EntityType : std::uint8_t { Player = 0, BydosSlave = 1, PlayerMissile = 2, EnemyMissile = 3 };
// in PacketType:
SpawnEntity = 0x12,
```

## 2. `PlayerInput` gets a `shoot` byte (client -> server)

| Field  | Type | Description |
|--------|------|-------------|
| netId  | u32  | Sender's network id |
| up     | bool | (unchanged) |
| down   | bool | (unchanged) |
| left   | bool | (unchanged) |
| right  | bool | (unchanged) |
| shoot  | bool | **new**, true while the fire key is held |

The client already sends it. The current server reads the first 4 bools and ignores
the rest, so nothing breaks. To use it: `reader >> netId >> up >> down >> left >> right;`
then `bool shoot = false; if (reader.hasMoreData()) reader >> shoot;`.
The server decides the fire rate (cooldown) and spawns the missile: the client never
creates gameplay entities on its own.

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

## 4. Game logic expected on the server

The subject requires the server to be authoritative: every gameplay decision below is
made by the server, the client only displays the result.

| Rule | Suggested value |
|------|-----------------|
| Player health | 5 (see difficulty table) |
| Bydos slave health | 2 (see difficulty table) |
| Player missile hits a Bydos | Bydos -1 HP, missile destroyed |
| Enemy missile hits a player | Player -1 HP, missile destroyed |
| Player rams a Bydos | Player -1 HP, Bydos destroyed |
| Fire rate | 1 missile every 0.25 s while `shoot` is held |
| Dead player | ignores inputs, removed after ~1.5 s |
| Off-screen missile or enemy | `DestroyEntity` |

Collisions can use axis-aligned boxes (sprite sizes: player 33x17, Bydos 33x33,
missile 16x4). `tools/mock_server.py` implements all of this in about 60 lines
(`damage()`, `collisions()`, `update()`) and can serve as a reference.

### Enemy fire direction

Enemies shoot **forward**, to the left of the screen, like in the original R-Type.
A player who flew past an enemy (to its right) must never be hit by its shots:

- easy and normal: every enemy missile goes straight left (`velY = 0`);
- hard: the missile may tilt toward the nearest living player **in front of** the enemy
  (player `x` smaller than enemy `x`), by at most 30 degrees. Players behind are ignored.

In every mode `velX` of an enemy missile is negative. Ramming an enemy still hurts,
wherever the player comes from.

## 5. Difficulty: `ConnectReq` and `ConnectAck` get one byte

The player picks the difficulty on the title screen. The client sends it, the server
decides and confirms it.

| Packet | New field (appended, optional) | Description |
|--------|-------------------------------|-------------|
| `ConnectReq` (client -> server) | `difficulty u8` | Requested level: 0 = easy, 1 = normal, 2 = hard. `payloadSize = 1` |
| `ConnectAck` (server -> client) | `difficulty u8`, after `netId` | Level actually used by the game. `payloadSize = 5` |

Backward compatible both ways: the current server never reads the `ConnectReq`
payload, and a client that does not know the byte only reads `netId`. When the
`ConnectAck` has no difficulty byte, the client simply does not show any difficulty.

Suggested server rule: the first player who joins an empty game chooses; the players
who join later get the level already in use (it is shared by the 4 players). A command
line option such as `--difficulty hard` can force it.

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

### Augment rules expected on the server

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
| Death | every augment of the player ends (`remaining = 0`) |

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

Backward compatible both ways: the current server ignores the `ConnectReq` payload and
sends `SpawnPlayer` without the byte. In that case the local player still sees his own
ship, and the other players are drawn with the default look (`player_1` to `player_4`).
An unknown skin index is ignored the same way.

Server side, this is the whole change:

```cpp
std::uint8_t skin = 0xFF;
if (header.payloadSize >= 2) { std::uint8_t difficulty; reader >> difficulty >> skin; }
client.skin = skin;
// in every SpawnPlayer: writer << header << netId << posX << posY << client.skin;
```

## 8. Points to look at on the server side

These are observations from testing the client against `ServerNetworkManager.hpp`:

1. **Player numbers.** The client shows "Player N" from the network id (first id =
   Player 1). That works while only players get ids. Once enemies and missiles share
   the same id counter, players will get scattered ids. Suggestion: add a `slot u8`
   (0..3) at the end of `SpawnPlayer`, or keep a separate id range for players.
2. **Duplicate `ConnectReq`.** Each `ConnectReq` creates a new player. If the `ConnectAck`
   is lost (UDP), a client retrying would create a ghost player for 3 seconds.
   Suggestion: if the endpoint is already connected, resend its `ConnectAck` instead.
3. **One sequence counter for all clients.** Fine as long as every packet a client
   receives comes from the same counter (the client drops `GameStateSync` whose
   sequence is not newer than the last one). Keep enemies and missiles on the same
   counter.
4. **Snapshot size.** `GameStateSync` is 20 bytes per entity. With many missiles,
   keep a datagram under ~1200 bytes (about 55 entities) or split it in several packets.

## Testing

`tools/mock_server.py` implements this extension (Bydos spawning on the right,
forward fire, missiles, fire cooldown, collisions, health, death, difficulty, bonuses,
augments and skins). Run it
instead of the server to test the client:

```
python3 tools/mock_server.py 4242                     # difficulty chosen by the first player
python3 tools/mock_server.py 4242 --difficulty hard   # difficulty forced
./r-type_client
```
