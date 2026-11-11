# Client engine architecture

The client is split in three layers. Each layer only depends on the one below it.

```
+-----------------------------------------------------------+
|  game/            R-Type: TitleScene, SkinScene,          |
|                   LobbyScene, GameScene, Hud,             |
|                   StarfieldSpawner,                       |
|                   SkinCatalog, EntityCatalog              |
|                   asks for assets by id, actions by name  |
+-----------------------------------------------------------+
|  engine/          generic, knows nothing about R-Type     |
|   core/           Clock, EngineContext, FrameStats,       |
|                   TextTemplate                            |
|   config/         Config (INI parser, no dependency)      |
|   ecs/            Registry, Components, ISystem,          |
|                   SystemPipeline, EntityFactory           |
|   systems/        Movement, Parallax, Animation,          |
|                   Interpolation, ScreenAnchor, Lifetime,  |
|                   Follow, Render (sprites + texts)        |
|   resources/      ResourceManager + asset manifest        |
|   audio/          IAudioManager, AudioService             |
|   tools/          DebugOverlay                            |
|   input/          IInputManager, KeyCodes, ActionMap      |
|   scene/          IScene, AScene, SceneManager            |
|   platform/       IBackend, IWindow, BackendFactory       |
+-----------------------------------------------------------+
|  backend/sfml/    the ONLY code that includes SFML        |
+-----------------------------------------------------------+
```

`network/ClientNetworkManager.hpp` sits next to the game layer: it turns packets into
entities and notifies the game through spawn / destroy callbacks. It does not render
anything. Malformed packets are dropped (never crash the client), packets that do not
come from the server port are ignored. Every packet is described
in `docs/protocol.md`; typed entities and the shoot byte are explained in
`docs/protocol-extension.md`.

## Rules

- Only `backend/` may include a third-party library header. Anything else uses
  `engine::` interfaces (`IWindow`, `ISprite`, `IInputManager`, `INetworkSocket`...).
- Components are plain data. Logic goes into systems.
- Nothing is hard-coded in the game: file paths live in `assets/config/assets.cfg`,
  keys in `[input]`, tuning values in `assets/config/client.cfg`.

## Frame loop

```
GameEngine::run
  input.update()          clear last frame's key edges
  backend.pollEvents()    OS events -> window + input
  quit action?            [engine] quit_action
  mute action?            [engine] mute_action
  scene.update(dt)        AScene: onUpdate(dt), then every system in order
  audio.update()          release finished sounds
  window.clear()
  scene.render(window)    RenderSystem: sort by zIndex, draw
  window.display()
```

`dt` comes from `engine::Clock` (std::chrono), clamped by `[engine] max_frame_time`,
so movement never depends on CPU speed.

## How-to

### Add an asset

Add a section to `assets/config/assets.cfg`. The section name is the id.

```ini
[bydo_slave]
path = assets/raw/r-typesheet5.gif
rect = 0 0 33 36
frames = 8
frame_time = 0.08
```

Then, in a scene: `_factory.createSprite("bydo_slave", {x, y}, zIndex);`
The animation component is added automatically because `frames > 1`.

### Add a sound or a music

```ini
[sfx_explosion]
type = sound
path = assets/sounds/explosion.wav
volume = 80
```

Then: `_context.audio.play("sfx_explosion");` or `_context.audio.playMusic("music_stage");`
Final volume = asset volume x `[audio] master_volume` x `music_volume` / `sfx_volume`.
The `toggle_mute` action (M by default) mutes everything.

### Show a text

Declare a font (`type = font`) in the manifest, then give an entity a
`TextComponent` (font id, text, size, color, outline, origin) and a
`TransformComponent`. Add a `ScreenAnchorComponent` to pin it to a corner of the
window whatever the resolution, and a `LifetimeComponent` to remove it after a delay.
HUD wording lives in `[hud]` as templates with `{placeholders}`.

### Show a new networked entity (enemy, weapon...)

Give its server type id a profile, no code needed:

```ini
[entity_types]
4 = bydos_snake

[entity:bydos_snake]
asset = bydos_snake          # texture id from assets.cfg (can be an animated sheet)
z = 5
scale = 1.5
interpolation_speed = 15     # 0 = snap to the server position
spawn_sound =
hit_sound = sfx_hit          # played when the server lowers its health
destroy_sound = sfx_explosion
```

Every asset and sound referenced here is checked at startup. Test without the real
server with `python3 tools/mock_server.py`.

### Add or remap a control

In `[input]`, map an action name to one or more keys: `shoot = Space Enter`.
In the game: `_context.actions.isJustPressed("shoot")`.
Key names: `Up Down Left Right Space Escape Enter A..Z` (case-insensitive).

### Add a system

```cpp
class BlinkSystem : public engine::ISystem {
public:
  void update(engine::Registry& registry, float dt) override { /* ... */ }
};
```

Register it in the scene's `onInit()`: `addSystem<BlinkSystem>();`
Systems run every frame, in registration order.

### Add a scene

Derive from `engine::AScene`, implement `onInit()` (systems + entities) and optionally
`onUpdate(float)`. Register it in `root.cpp` with `addScene("name", ...)` and select the
first scene with `[engine] start_scene`. Switch scenes with
`_context.scenes.changeScene("name")` (the title screen does it on the `start` action).

Every `AScene` runs, after its own systems, the built-in `ScreenAnchorSystem`,
`BlinkSystem`, `FlashSystem`, `FollowSystem`, `LifetimeSystem` and the `DebugOverlay`.
`FollowComponent{target, offset}` keeps an entity glued to another one (the `P1` tag
above a ship) and destroys it when the target disappears.

Scene flow: `title` (difficulty) -> `skin` (ship) -> `lobby` (room) -> `game`. Leaving
a game, or pressing the menu key after a death, goes back to `lobby`. The order is only
in the config (`[title] next_scene`, `[skin_select] next_scene` / `previous_scene`,
`[lobby] next_scene` / `previous_scene`, `[game] menu_scene` / `lobby_scene`).

### Lobby and session states

`LobbyScene` opens its own socket with `ClientNetworkManager::openLobby()`, asks for the
room list every `[lobby] refresh_interval` seconds and creates rooms. It never joins a
room itself: it stores the chosen room id in the session (`room`), and `GameScene`
joins it with `joinRoom()`, because the server knows a player by the socket it plays
from. When the join fails, `GameScene` stores the reason in the session
(`room_error`, `room_error_id`) and goes back to the lobby, which shows it.

`ClientNetworkManager::getState()` tells where the client is:

| State | Meaning |
|-------|---------|
| `Disconnected` | No session. `getDisconnectReason()` says why the last one ended: `Left`, `Rejected` or `TimedOut` |
| `Lobby` | Socket open, room requests allowed (`requestRooms`, `createRoom`) |
| `Connecting` | `ConnectReq` or `RoomJoinReq` sent, waiting for `ConnectAck` |
| `InGame` | Inputs and snapshots flow |

`leaveRoom()` sends `RoomLeaveReq`, then calls `reset()`: every remote entity is
destroyed (without destroy sounds) and the session state is forgotten (id, token,
received sequence numbers, pending events, snapshots, snapshot clock). Gameplay packets
that arrive after that are ignored, so the client can join another room without
restarting. The socket is bound only once, so the server always sees the same port.

Room errors shown by the lobby: room full, room expired (it was in the list or was just
created, but the server no longer knows it), room not found, room limit reached, no
answer from the server. The server answers "not found" for both missing and expired
rooms; the lobby tells them apart with the `room_seen` session key.

### Add a ship skin

1. Declare the texture in `assets/config/assets.cfg` (`[ship_gold]` with `path`, `rect`).
2. Add `gold` at the end of `[skins] list` and a `[skin:gold]` section with `label`,
   `asset = ship_gold` and `color`.

Append new skins at the end of the list: the index is what is sent to the server, so
reordering the list changes the skin the other players see.

### Debug overlay

Press F3 (`[debug] toggle_action`) to show FPS, frame time and the number of sprites and
texts drawn. The visibility is engine-wide, so it stays on across scene changes.

A scene can add its own lines through `_context.debug.lines["name"]`. The game scene adds
a network line (`[network] debug_text`):

```
NET  tick 977  delay 100 ms  buffer 31/32  late 2  remote 27  pending 5  correction 0 px  snapshots 9.8 KiB/s (17 full, 883 delta, 0 req)
```

`tick` is the newest server tick received, `buffer` the average number of snapshots kept
per remote entity, `late` the number of snapshots that arrived after a newer one and were
still used, `remote` the number of interpolated entities, `pending` the local inputs not
confirmed by the server yet, `correction` the size of the last prediction correction,
`snapshots` the snapshot bandwidth received with the number of full states, deltas and
full state requests.

Three more lines (`LINK`, `TRAFFIC`, `ISSUES`) show the round-trip time, jitter, estimated
loss, throughput in bytes per second, packets received, sent and rejected, retransmissions
and prediction corrections. What each value means and how the loss is estimated is in
[network-metrics.md](network-metrics.md). Each line of the overlay is its own text, stacked
with `[debug] line_spacing`.

### Remote entity interpolation

Every snapshot (`0x31` / `0x32`) carries the server simulation tick (60 per second). The
client keeps, for each remote entity (enemies, missiles, bonuses, other players), a
bounded buffer of `{tick, position, velocity}` samples sorted by tick
(`SnapshotBufferComponent`, `engine/ecs/SnapshotBuffer.hpp`):

- a late snapshot is inserted at its place instead of being dropped;
- a duplicate tick is ignored, and a sample older than a full buffer is rejected;
- when the buffer is full the oldest samples are removed.

`SnapshotClock` estimates the current server tick from the arrival of the snapshots and
smooths it, so a late packet does not make the clock go backwards.
`SnapshotInterpolationSystem` draws the remote entities `interpolation_delay_ms` in the
past: it takes the two samples around that instant and interpolates between them. When no
newer sample exists yet, it continues along the last velocity for at most
`max_extrapolation_ms`, then holds the position.

The same timeline is used for the entity lifecycle:

- a new remote entity stays hidden until the render time reaches its first sample, then
  appears exactly there (its `FollowComponent` tag too);
- after `DestroyEntity`, the entity stops receiving updates but stays on screen until the
  render time reaches the destruction tick, so a missile visibly reaches its target before
  both disappear. After one second at most it is removed anyway.

The local ship has no buffer: it keeps the `InterpolationComponent` smoothing toward the
latest server position.

| Key (`[network]`) | Default | Why |
|---|---|---|
| `server_tick_rate` | `60` | The server simulates in fixed 1/60 s steps, so one tick is 16.7 ms. |
| `interpolation_delay_ms` | `100` | 6 server ticks. Two samples around the render time stay available with up to about 80 ms of jitter plus one lost snapshot. Lower values extrapolate more often on a bad connection, higher values make remote entities lag further behind. |
| `max_extrapolation_ms` | `100` | Bounds the error when data is missing: the fastest entity (player missile, 900 px/s) overshoots by 90 px at most. |
| `snapshot_buffer_size` | `32` | About 530 ms of history at 60 Hz, well above `delay + extrapolation` (12 ticks), so late snapshots still find their place. It costs 20 bytes per sample and per entity. |

Measured on a simulated enemy moving at constant speed (standard deviation of the
displacement per frame, lower is smoother):

| Network | Previous smoothing | Snapshot interpolation |
|---|---|---|
| Perfect | 0.00 px | 0.00 px |
| 30 ms jitter | 0.43 px | 0.07 px |
| 60 ms jitter, 10 % reordered | 0.65 px | 0.16 px |
| 90 ms jitter, 20 % reordered | 0.83 px | 0.23 px |

To see it in the game, put `scripts/lag_relay.py` between the clients and the server:

```sh
./build/r-type_server
python3 scripts/lag_relay.py --delay 30 --jitter 60 --reorder 0.1
./build/r-type_client -p 4243
```

`python3 scripts/lag_relay.py --help` lists the options (latency, jitter, reordering, loss,
impaired direction).

### Local prediction and server reconciliation

The local ship answers the keys immediately, while the server keeps the final word on
positions, collisions, damage and death.

1. The game scene sends one `PlayerInput` per fixed step of 1/60 s
   (`server_tick_rate`), whatever the frame rate. Its packet sequence is the input
   number, growing with every input.
2. `LocalPrediction` (`network/LocalPrediction.hpp`) applies each input at once with the
   shared `gameplay::applyMovement` (`include/common/gameplay/PlayerMovement.hpp`), the
   exact function the server uses, and keeps the inputs not confirmed yet.
3. The server applies the inputs one step each, in order, and answers with `InputAck`
   (`0x33`): last input applied and confirmed position.
4. On `InputAck`, the client drops the confirmed inputs, starts again from the confirmed
   position and replays the remaining ones. When the result differs from what was
   displayed (an input lost or rejected on the way), the difference is kept as a visual
   offset that fades out (`prediction_correction_speed`), so the ship never jumps. Above
   `prediction_snap_distance` the ship is moved at once (respawn, long freeze).
5. Only the movement of the local ship is predicted. Shots, hits, health and death come
   from the server. When the local player dies, the prediction stops.

| Key (`[network]`) | Default | Why |
|---|---|---|
| `prediction` | `true` | `false` brings back the server-driven ship, useful to compare. |
| `prediction_correction_speed` | `12` | The visual offset is divided by e (about 2.7) every 1/12 s, so a small correction is gone in about 0.25 s without a visible jump. |
| `prediction_snap_distance` | `120` | 0.4 s of movement at 300 px/s. A larger error is not a network hiccup but a teleport, so it is applied at once. |
| `prediction_max_pending_inputs` | `120` | 2 s of inputs at 60 Hz. Older unconfirmed inputs are dropped, which bounds the memory and the replay cost if the server stops answering. |

With `scripts/lag_relay.py --direction both` (30 ms + 0 to 60 ms of jitter each way and
10 % reordering, so a 60 to 180 ms round trip), the overlay shows 6 to 11 pending inputs
and corrections of 0 to 5 px: the ship moves on the first frame of a key press, and the
rare correction (an input the relay delivered too late for the server) fades out.

### Add a backend (SDL, Raylib...)

Implement `engine::IBackend` (window, input, textures, UDP socket) under
`backend/<name>/`, register it in `src/client/backend/Backends.cpp`, then set
`[engine] backend = <name>`. No engine or game file changes.

## Configuration reference

| Section       | Keys |
|---------------|------|
| `[engine]`    | `backend`, `asset_manifest`, `preload_assets`, `quit_action`, `mute_action`, `clear_color`, `max_frame_time`, `start_scene` |
| `[window]`    | `title`, `width`, `height`, `framerate` (0 = unlimited), `vsync` |
| `[input]`     | `action = key [key...]` |
| `[audio]`     | `enabled`, `muted`, `master_volume`, `music_volume`, `sfx_volume` |
| `[network]`   | `host`, `port`, `interpolation_speed` (local ship without prediction), `server_tick_rate`, `snapshot_buffer_size`, `interpolation_delay_ms`, `max_extrapolation_ms`, `prediction`, `prediction_correction_speed`, `prediction_snap_distance`, `prediction_correction_threshold`, `prediction_max_pending_inputs`, `debug_text`, `debug_link_text`, `debug_traffic_text`, `debug_issues_text`, `connect_timeout` |
| `[game]`      | `player_asset_prefix`, `player_variants`, `max_players`, `first_player_id`, `player_z`, `music`, `join_sound`, `leave_sound`, `player_hit_sound`, `player_death_sound`, `game_over_sound`, `hit_flash_color`, `hit_flash_duration`, `menu_action`, `menu_scene`, `lobby_scene`, `leave_action`, `leave_timeout`, `player_tag_text`, `player_tag_size`, `player_tag_offset`, `player_tag_outline` |
| `[title]`     | `font`, `title_*`, `subtitle_*`, `prompt_*`, `difficulty_*`, `start_action`, `next_scene`, `music`, `start_sound`, `server_text`, `controls`, `label_<action>` |
| `[skins]`     | `list`, `default`; one `[skin:<id>]` per ship: `label`, `asset`, `color`. The index in `list` is the value sent on the network |
| `[lobby]` | `font`, `title_*`, `status_*`, `error_*`, `list_y`, `row_size`, `row_spacing`, `max_rows`, `room_text`, `state_waiting`, `state_running`, `state_full`, `hint_*`, `create_hint_text`, `panel_*`, `max_capacity`, `refresh_interval`, `*_color`, `outline_thickness`, `up_action`, `down_action`, `prev_action`, `next_action`, `start_action`, `back_action`, `create_action`, `next_scene`, `previous_scene`, `*_text` messages, `move_sound`, `confirm_sound`, `back_sound`, `error_sound` |
| `[skin_select]` | `font`, `title_*`, `label_*`, `hint_*`, `prev_action`, `next_action`, `start_action`, `back_action`, `next_scene`, `previous_scene`, `carousel_y`, `spacing`, `selected_scale`, `other_scale`, `other_alpha`, `outline_thickness`, `dim_color`, `change_sound`, `confirm_sound`, `back_sound` |
| `[debug]`     | `font`, `text_size`, `line_spacing`, `margin`, `color`, `outline_thickness`, `z_index`, `toggle_action`, `visible`, `refresh`, `text` |
| `[difficulty]` | `levels`, `default`, `label_<level>`, `color_<level>` |
| `[augments]` | `<augment id> = <profile>`; profiles in `[augment:<profile>]`: `label`, `color`, `gain_sound`, `end_sound` |
| `[floating_text]` | `font`, `text_size`, `rise_speed`, `duration`, `fade_portion`, `offset_x`, `offset_y`, `outline_thickness`, `z`, `heal_color`, `heal_text`, `augment_text` |
| `[entity_types]` | `<type id> = <profile>`; profiles in `[entity:<profile>]`: `asset`, `z`, `scale`, `interpolation_speed`, `spawn_sound`, `hit_sound`, `destroy_sound` |
| `[hud]`       | `font`, `text_size`, `scale`, `color`, `outline_color`, `outline_thickness`, `margin`, `notification_duration`, `line_spacing`, `notification_top`, `health_bar_*`, `health_color_*`, `health_low_threshold`, `augments_top`, `big_text_size`, `destroyed_color`, `*_text` templates |
| `[starfield]` | `star_*`, `layer_speeds`, `layer_scales`, `layer_alphas`, `planet_*`, `wrap_margin`, `seed` |

Malformed values stop the client with the file, line, section and key in the error
message. Missing keys fall back to the defaults in the code.

Command line: `r-type_client [-c config.cfg] [-r width height]`. `-r` overrides `[window]`.

## Accessibility features

| Need | Feature |
|------|---------|
| Motor | Every action can be bound to several keys (`[input]`), arrows and ZQSD by default |
| Visual | Players differ by color AND number; health shown as a bar AND "HP x/y"; HUD `scale` enlarges texts; outlined texts |
| Audio | Separate music / effects volumes, global mute key; no information is sound-only (joins, leaves, hits and deaths are also shown visually) |
| Cognitive | Short HUD messages that disappear on their own; all wording editable in `[hud]` and `[title]`; the title screen lists the current controls; three difficulty levels chosen on the title screen (easy = more health, fewer and slower shots) |
| Photosensitivity | Blinking is slow (1.2 s cycle) and can be disabled (`prompt_blink_period = 0`) |

## Third-party assets

- Font: Press Start 2P, SIL Open Font License 1.1 (`assets/fonts/OFL.txt`).
- Sounds and music in `assets/sounds/` are generated procedurally for this project.
