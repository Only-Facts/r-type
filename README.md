# R-Type

A networked multiplayer remake of **R-Type**, written in **C++20** with **[SFML 3.1](https://www.sfml-dev.org/)**.

---

## 1. How to build

### Requirements

| Tool | Version | Link |
|---|---|---|
| CMake | 3.16 or later | [cmake.org](https://cmake.org/) |
| C++ compiler | C++20 (GCC 13+, Clang 16+ or MSVC 2022) | [gcc.gnu.org](https://gcc.gnu.org/) |
| Git | any | [git-scm.com](https://git-scm.com/) |
| SFML | 3.1 (downloaded automatically if not installed) | [sfml-dev.org](https://www.sfml-dev.org/) |

### Commands

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

## 2. How to run

Run the programs **from the root of the repository**, so the `assets/` folder is found.

### All at once

```bash
./run
```

The `run` script builds the project, then starts **1 server and 4 clients** on the same machine. Press **Ctrl+C** to stop everything. Client logs are written to `/dev/shm/r-type_client_<uid>_<n>.log`.

Options can be passed to the programs with environment variables:

```bash
SERVER_FLAGS="-p 4242" CLIENT_FLAGS="-r 1280 720" ./run
```

### One by one

```bash
./build/r-type_server
./build/r-type_client
```

The client connects to **`127.0.0.1:4242`**, i.e. a server running on the same machine.

### Game flags

#### Server: `./build/r-type_server [OPTIONS]`

| Flag | Argument | What it does | Default |
|---|---|---|---|
| `-p`, `--port` | `<n>`: a port number | UDP port the server listens on. The clients always connect to port `4242`, so keep the default to play. | `4242` |
| `-h`, `--help` | — | prints the help and exits | |

```bash
./build/r-type_server            # listens on port 4242
./build/r-type_server -p 5000    # listens on port 5000
./build/r-type_server --help
```

#### Client: `./build/r-type_client [OPTIONS]`

| Flag | Argument | What it does | Default |
|---|---|---|---|
| `-r`, `--resolution` | `<w> <h>`: width and height in pixels | size of the game window | `1920 1080` |
| `-h`, `--help` | — | prints the help and exits | |

```bash
./build/r-type_client                  # 1920 x 1080 window
./build/r-type_client -r 1280 720      # 1280 x 720 window
./build/r-type_client --help
```

#### Flags with the `run` script

The `run` script passes `SERVER_FLAGS` to the server and `CLIENT_FLAGS` to each of the 4 clients:

```bash
CLIENT_FLAGS="-r 1280 720" ./run       # 4 clients in 1280 x 720 windows
SERVER_FLAGS="-p 4242" ./run           # server on port 4242 (the default)
```

### Controls

| Where | Key | Action |
|---|---|---|
| Menu | ↑ / ↓, Enter | select and confirm **PLAY** or **QUIT** |
| Menu | Q / Escape | quit |
| Game | Arrow keys | move |
| Game | Space | shoot |
| Game | Escape | back to the menu |

---

## 3. How to add an enemy type

1. **Sprite**: put the enemy's sprite sheet in `assets/sprites/` (frames side by side, same size).

2. **Type**: in `include/common/GameConstants.hpp`, add a name before `Count`:
   ```cpp
   enum class EnemyType : std::uint8_t { Spinner = 0, Tank = 1, Count };
   ```

3. **Gameplay** (server): in `src/server/EnemyManager.cpp`, add a line to `ENEMY_STATS`:
   ```cpp
   {{6.0f, 6.0f, 84.0f, 84.0f}, 120.0f, 5, 500, 6.0f},   // hitbox, speed, health, points, explosion size
   ```

4. **Look** (client): in `src/client/game/SFMLEnemySpriteFactory.cpp`, add a line to `ENEMY_VISUALS`:
   ```cpp
   {"assets/sprites/enemy_tank.png", 32, 32, 4, 0.12f, true},   // image, frame size, frame count, frame duration, ping-pong
   ```

5. **Rebuild** the client and the server.

Lines must be in the **same order** as in the `enum` in steps 3 and 4.
