# Client engine architecture

The client is split in three layers. Each layer only depends on the one below it.

```
+-----------------------------------------------------------+
|  game/            R-Type: TitleScene, GameScene,          |
|                   StarfieldSpawner, Hud                   |
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
|                   Render (sprites + texts)                |
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
come from the server port are ignored. Typed entities and the shoot byte are described
in `docs/protocol-extension.md`.

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
`BlinkSystem`, `LifetimeSystem` and the `DebugOverlay`.

### Debug overlay

Press F3 (`[debug] toggle_action`) to show FPS, frame time and the number of sprites and
texts drawn. The visibility is engine-wide, so it stays on across scene changes.

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
| `[network]`   | `host`, `port`, `interpolation_speed`, `connect_timeout` |
| `[game]`      | `player_asset_prefix`, `player_variants`, `max_players`, `first_player_id`, `player_z`, `music`, `join_sound`, `leave_sound`, `player_hit_sound`, `player_death_sound`, `game_over_sound`, `hit_flash_color`, `hit_flash_duration` |
| `[title]`     | `font`, `title_*`, `subtitle_*`, `prompt_*`, `difficulty_*`, `start_action`, `next_scene`, `music`, `start_sound`, `server_text`, `controls`, `label_<action>` |
| `[debug]`     | `font`, `text_size`, `color`, `toggle_action`, `visible`, `refresh`, `text` |
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
