# Server Track 2 — completion report

Date: 2026-11-06

## Delivered

- Independent game rooms behind one UDP socket, with room listing, creation,
  joining, leaving, room capacity limits and empty-room cleanup.
- Server-authoritative simulation: movement, normalized diagonal speed, firing,
  projectiles, Bydos AI, collisions, player health/death, score, bonuses,
  temporary augments, boss encounters and stage progression.
- A dedicated UDP receive worker feeding a bounded queue; simulation and room
  state remain owned by the main server thread.
- Fixed-step simulation (60 Hz), bounded catch-up and accounting for discarded
  simulation time.
- Per-session tokens bound to the assigned player and UDP endpoint; stale input
  sequences, invalid tokens, malformed input bytes and unknown peers are rejected.
- Reliable delivery with ACKs, endpoint-scoped pending packets, bounded queues,
  exponential retransmission and timeout metrics. Continuous input and snapshots
  intentionally remain unreliable.
- Little-endian scalar serialization, compact snapshot records, delta updates and
  periodic full keyframes. Entity lifecycle events remain reliable.
- Server metrics for traffic, malformed/rejected/rate-limited packets, queue
  drops, reliable delivery and simulation catch-up.

## Bugs fixed during final integration

- Routed `PlayerInput` through the multi-room manager into the owning room. Before
  this fix, the room could connect and simulate clients but would not receive their
  movement/fire commands through the real manager.
- Added exact payload-size checks for authenticated input and disconnect packets.
- Updated the shared client network decoder to apply packed delta snapshots.
- Added per-entity sequence checks for health, score and augment state, plus
  separate pending augment entries per augment type. Older reliable packets can
  no longer roll state backwards when UDP delivers them out of order. This is
  protocol compatibility only; the lobby UI and client presentation remain open.
- Buffered player spawns that arrive before `ConnectAck`, which can happen because
  UDP does not guarantee packet order; the client now identifies its local player
  after the acknowledgement arrives.
- Corrected protocol documentation for the 17-byte `PlayerInput` payload and the
  13-byte `ConnectAck` payload, including the session token.
- Removed a departing player's already-fired missiles from the room and sent
  reliable destroy events to the remaining players. This prevents orphaned
  projectiles from continuing to deal damage or award score after an owner
  leaves or times out.
- Avoided binding references directly to fields of the packed 9-byte packet
  header. The stream reader/writer now copy header scalars through aligned local
  values, eliminating undefined unaligned accesses reported by UBSan.

## Automated validation

Five CTest suites pass in each configuration:

- GCC: server rooms, gameplay, protocol, client-network compatibility and stress.
- Clang 17: the same five suites.
- GCC with AddressSanitizer and UndefinedBehaviorSanitizer, including leak
  detection: the same five suites.
- GCC with ThreadSanitizer: the same five suites.

The stress test creates 16 rooms and simulates 60 players, checks room isolation
and bounded reliable queues, and acknowledges all pending reliable events. The
network tests also exercise retransmission, invalid ACK endpoints, input routing,
invalid input values, session validation, packet ordering and delta application.
Gameplay tests cover owned-projectile cleanup on disconnect, boss progression,
bonuses, augment expiration, score, fixed-step behavior and snapshot keyframes.

## Validation boundary / integration notes

The full SFML application target could not be configured in this environment:
CMake's CPM bootstrap attempted to download CPM.cmake from GitHub, but DNS resolution
was unavailable (`Could not resolve hostname`). The standalone C++ test targets do
not require SFML and all pass, but this archive therefore does not claim a successful
build or live UDP run of the SFML server executable here.

The lobby UI still needs to consume the server's `RoomListAck`, `RoomCreateAck`,
`RoomJoinAck` and `RoomLeaveAck` messages; this is client/UI integration rather than
server implementation. The archive also does not contain runtime art/configuration
for the boss profile (entity type `9`), so its visual presentation must be supplied
by the client/assets side.

Session tokens bind actions to a session and endpoint; they are not encryption.
The protocol does not provide confidentiality or cryptographic protection against
an attacker capable of observing or spoofing the client's UDP traffic. A public
Internet deployment would need an authenticated encrypted transport and operational
protections beyond this project implementation.

## Build the isolated test suite

```sh
cmake -S . -B build-tests \
  -DRTYPE_BUILD_APPLICATIONS=OFF \
  -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tests -j2
ctest --test-dir build-tests --output-on-failure
```
