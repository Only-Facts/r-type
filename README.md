# R-Type

A networked multiplayer shoot'em up inspired by the classic R-Type, written in C++20.

The project is split into an authoritative game server and a graphical client built on a small,
data-driven game engine based on an Entity Component System (ECS). Up to four players share the
same game over UDP.

## Features

- Authoritative UDP server: the server owns the game state, clients only send inputs and display what they receive
- Up to 4 players in the same game, with remote entities interpolated from a buffer of timed snapshots
- Local prediction with server reconciliation: your ship answers the keys immediately, the server keeps the final word
- Title screen with difficulty selection (Easy, Normal, Hard)
- Ship selection screen with 5 ships, the chosen ship is shown to every player
- Enemies (Bydos) that fly across the screen and shoot forward, aiming at players ahead of them in Hard mode
- Player shooting, collisions, health bar and death screen
- Bonuses dropped by destroyed enemies (double shot, triple shot, heal, damage up)
- Parallax starfield, sprite animations, sounds and music
- Debug overlay (F3) with frame rate, frame time, sprites drawn and network statistics (server tick, snapshot buffer, late snapshots)
- Fully configurable through text files: window, controls, assets, sounds, texts and colors

## Requirements

| Tool | Version |
|------|---------|
| CMake | 3.16 or newer |
| C++ compiler | with C++20 support (tested with GCC 13, GCC 14 and Clang 18) |
| Git | any recent version |
| Internet access | for the first configuration only |

SFML 3.1.0 does not need to be installed: CMake downloads and builds it automatically through
[CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) the first time the project is configured.

On Linux, SFML still needs a few system development packages:

```sh
# Debian / Ubuntu
sudo apt install libx11-dev libxrandr-dev libxcursor-dev libxi-dev libudev-dev \
                 libgl1-mesa-dev libegl1-mesa-dev libdrm-dev libgbm-dev

# Fedora
sudo dnf install libX11-devel libXrandr-devel libXcursor-devel libXi-devel systemd-devel \
                 mesa-libGL-devel mesa-libEGL-devel libdrm-devel mesa-libgbm-devel

# Arch Linux
sudo pacman -S libx11 libxrandr libxcursor libxi systemd-libs mesa libdrm
```

On macOS, the Xcode Command Line Tools are enough (`xcode-select --install`).

## Build

```sh
cmake -S . -B build
cmake --build build
```

The first configuration takes a few minutes because SFML is downloaded and compiled. The build
produces two executables in `build/`, and the `assets/` folder is copied next to them:

- `build/r-type_server`
- `build/r-type_client`

### Windows (cross-compilation from Linux)

With the MinGW-w64 toolchain installed:

```sh
cmake -S . -B build-win -DWIN=ON
cmake --build build-win
```

## Run

Start the server, then one client per player:

```sh
./build/r-type_server
./build/r-type_client
```

To play on several machines, start the server on one of them and give its address to every client:

```sh
./build/r-type_server -p 4242
./build/r-type_client -H 192.168.1.20 -p 4242
```

The `run` script builds the project, then starts one server and four clients at once, which is
handy to test multiplayer on a single machine:

```sh
./run
```

### Testing on a bad connection

`scripts/lag_relay.py` sits between the clients and the server and adds latency, jitter,
reordering or loss, to check that remote entities stay smooth:

```sh
./build/r-type_server
python3 scripts/lag_relay.py --delay 30 --jitter 60 --reorder 0.1
./build/r-type_client -p 4243
```

Use `--direction both` to also delay the inputs and see the local prediction at work.
Press F3 in game to see the network line of the debug overlay (server tick, buffered
snapshots, late snapshots, pending inputs, last correction).

### Server options

| Option | Description | Default |
|--------|-------------|---------|
| `-p, --port <n>` | Port to listen on | `4242` |
| `-h, --help` | Show the help message | |

### Client options

| Option | Description | Default |
|--------|-------------|---------|
| `-H, --host <addr>` | Server address | `127.0.0.1` |
| `-p, --port <n>` | Server port | `4242` |
| `-r, --resolution <w> <h>` | Window size | from `client.cfg` |
| `-c, --config <file>` | Client configuration file | `assets/config/client.cfg` |
| `-h, --help` | Show the help message | |

## How to play

1. On the title screen, choose the difficulty with Left / Right and press Enter or Space.
2. Choose your ship with Left / Right and press Enter or Space to start. Backspace goes back to the title screen.
3. Destroy the Bydos, avoid their missiles and collect the bonuses they drop.

| Action | Keys |
|--------|------|
| Move | Arrow keys or Z Q S D |
| Shoot | Space |
| Mute / unmute | M |
| Debug overlay | F3 |
| Quit | Escape |

All keys can be changed in the `[input]` section of `assets/config/client.cfg`.

## Architecture

```
+-------------------------------------------------------------+
|  game/            R-Type itself: scenes, HUD, catalogs        |
|                   (knows nothing about SFML)                  |
+-------------------------------------------------------------+
|  engine/          Generic engine: ECS, systems, scenes,       |
|                   resources, input actions, audio, config     |
+-------------------------------------------------------------+
|  backend/sfml/    The only code that uses SFML, behind the    |
|                   engine interfaces (IWindow, ISprite, ...)   |
+-------------------------------------------------------------+
|  network/         Client side of the UDP protocol             |
+-------------------------------------------------------------+
```

- **ECS.** Every object in the game (ship, enemy, missile, star, text) is an entity. Components only hold
  data (position, velocity, sprite, health) and systems hold the logic (movement, animation,
  interpolation, rendering). The `RenderSystem` draws every entity that has a sprite or a text, sorted by depth.
- **Engine and game are separated.** The game asks the engine for resources by name (`ship_cyan`,
  `font_hud`) and for actions (`shoot`), never for files or keys. Replacing SFML only means
  writing a new backend.
- **Data-driven.** Assets are declared in `assets/config/assets.cfg`, and the game settings
  (window, controls, difficulty labels, ships, HUD texts and colors) are in `assets/config/client.cfg`.
- **Authoritative server.** The server runs the game logic at 60 Hz and broadcasts the state of
  every entity. Clients send their inputs and interpolate the positions they receive.

More details:

- [docs/client-engine.md](docs/client-engine.md): client engine, ECS, systems, configuration reference
- [docs/protocol-extension.md](docs/protocol-extension.md): network protocol, packet layouts and game rules

## Network protocol

Binary protocol over UDP. Every packet starts with a 9-byte header: magic `0x5254` (u16), type (u8),
sequence number (u32) and payload size (u16).

| Type | Packet | Direction | Payload |
|------|--------|-----------|---------|
| `0x01` | ConnectReq | client to server | difficulty, ship |
| `0x02` | ConnectAck | server to client | player id, difficulty |
| `0x03` | Disconnect | client to server | player id |
| `0x10` | SpawnPlayer | server to client | player id, position, ship |
| `0x11` | DestroyEntity | server to client | entity id |
| `0x12` | SpawnEntity | server to client | entity id, type, position |
| `0x13` | HealthUpdate | server to client | entity id, health, max health |
| `0x14` | AugmentUpdate | server to client | player id, bonus, level, remaining time |
| `0x20` | PlayerInput | client to server | player id, up, down, left, right, shoot |
| `0x30` | GameStateSync | server to client | position and velocity of every entity |

The exact byte layout of each packet is described in [docs/protocol-extension.md](docs/protocol-extension.md).

## Project structure

```
.
├── assets/
│   ├── config/         client.cfg (game settings) and assets.cfg (asset manifest)
│   ├── fonts/          fonts
│   ├── raw/            original R-Type sprite sheets
│   ├── sounds/         sound effects and music
│   └── ...             sprites (player, enemies, missiles, bonuses, UI)
├── cmake/toolchain/    MinGW-w64 toolchain for Windows cross-compilation
├── docs/               technical documentation
├── include/
│   ├── client/         client headers (backend, engine, game, network)
│   ├── common/         code shared by client and server (protocol, packets, sockets, CLI)
│   └── server/         server headers
├── src/
│   ├── client/
│   ├── common/
│   └── server/
├── CMakeLists.txt
└── run                 builds, then starts one server and four clients
```

## Current status

The bonuses are dropped by enemies and displayed by the client, but the server does not handle
picking them up yet, so the bonus effects (`AugmentUpdate`) are not sent.

## Authors

- Ahmed Al-Rawi
- Rosa Carvalho
- Raphael Farge
- Salah-Eddine Ghellab
