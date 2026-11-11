# Network metrics

The client measures its own network traffic, so that a change to the netcode can be
checked with numbers instead of impressions. Every metric is computed all the time by
`ClientNetworkManager` and `LocalPrediction`; the debug overlay only shows them.

Code: [NetStats.hpp](../include/client/network/NetStats.hpp),
[ClientNetworkManager.hpp](../include/client/network/ClientNetworkManager.hpp),
[LocalPrediction.hpp](../include/client/network/LocalPrediction.hpp).
Tests: `client_network_test` (each metric alone) and `net_metrics_test` (a real client
against the real server, on a perfect link and on a lossy link).

## Showing the metrics

Press **F3** in game. Under the frame line and the existing `NET` line, the overlay shows:

```text
LINK     rtt 16.8 ms (last 16.9)   jitter 0.1 ms   loss 0.0 % (0/12 pings)
TRAFFIC  in 5946 B/s   out 1609 B/s   packets 644 in / 374 out   resent 0 out / 0 in
ISSUES   rejected 9 (dup 1, stale 8, bad 0, sender 0, outside 0)   corrections 0 (snaps 0)
```

- F3 only decides whether the text is built and drawn. The metrics, the pings and the
  game do not depend on it: toggling the overlay never changes the gameplay.
- A value that does not exist yet is shown as `-` (no RTT before the first `Pong`, no
  jitter before the second one, no loss before 10 ping results).
- The three lines are templates in `client.cfg`: `[network] debug_link_text`,
  `debug_traffic_text` and `debug_issues_text`. An empty template hides its line. Line
  spacing is `[debug] line_spacing`.

## Metrics

| Overlay | Field | Meaning |
|---------|-------|---------|
| `rtt` | `rttMs` | Smoothed round-trip time, in milliseconds |
| `last` | `lastRttMs` | Last raw round-trip time |
| `jitter` | `jitterMs` | How much the round-trip time changes from one ping to the next |
| `loss` | `lossPercent` | Estimated round-trip loss, see below |
| `pings`, `lost` | `pingsSent`, `pingsLost` | Pings sent, pings declared lost |
| `in`, `out` | `bytesReceivedPerSecond`, `bytesSentPerSecond` | Throughput in **bytes** per second |
| `packets` | `packetsReceived`, `packetsSent` | Every datagram read from or written to the socket |
| `rejected` | `rejectedTotal()` | Received datagrams the client ignored, by reason |
| `resent out` | `retransmissionsSent` | Requests the client sent again because no answer came |
| `resent in` | `retransmissionsReceived` | Reliable server packets received a second time |
| `corrections` | `LocalPrediction::correctionCount()` | Server corrections of the predicted position |
| `snaps` | `LocalPrediction::snapCount()` | Corrections too large to be smoothed |

### Round-trip time and jitter

The client sends a `Ping` every **0.5 s** while it is in the lobby or in a game. Its
payload is the client clock in microseconds; the server copies it into a `Pong` without
reading it ([protocol.md, section 9](protocol.md#9-ack-ping-and-pong)). When the `Pong`
arrives:

```text
sample = now - timestamp
rtt    = rtt + (sample - rtt) / 8                        first sample: rtt = sample
jitter = jitter + (|sample - previous sample| - jitter) / 16    from the second sample
```

The smoothing of `rtt` is the one TCP uses (RFC 6298), so one slow ping does not make the
value jump. The jitter formula is the one RTP uses (RFC 3550), applied to round-trip
samples.

Limits:

- The client reads the socket once per frame, so a sample includes up to one frame of
  waiting. On a local server at 60 FPS the round-trip time shows about **16.7 ms**: that
  is the frame time, not the network.
- The clock advances with the frame time given to `update()`. A frame that is longer than
  real time (a pause in a debugger) inflates the sample.
- Two pings per second are a small sample: a short spike between two pings is not seen.
- A `Pong` whose timestamp matches no waiting ping (a duplicate, or one that came after
  the ping was declared lost) is rejected as stale and gives no sample.

### Estimated loss

UDP gives no way to know how many packets the server sent to us and did not arrive. The
client therefore does **not** compute the loss from the packets it receives. In particular,
it does not use gaps in the `sequence` numbers: the server has one counter for every room
and every client, so a gap is normal and does not mean a packet was lost
([protocol.md, section 11](protocol.md#11-sequencing-reliability-timeouts-and-errors)).

The loss is **estimated from the pings**:

1. A ping that gets no `Pong` within **1 second** is counted as lost.
2. The client keeps the result (answered or lost) of the last **40** pings, about the last
   20 seconds.
3. `loss = lost pings / results kept × 100`, shown once at least **10** results are known.

What this number is, and is not:

- It is the loss of a **round trip**: a lost `Ping` and a lost `Pong` count the same. If
  each direction loses a fraction `p` of the packets, the estimate tends to
  `1 - (1 - p)²`, not `p`. With 20 % loss each way, expect about 36 %.
- It measures the path of small packets sent twice per second. Inputs, snapshots and
  events travel on the same path, but their own loss is not measured.
- With 40 results, the value moves in steps of 2.5 %. A short burst of loss can be missed
  or, on the contrary, weigh a lot for 20 seconds.
- A `Pong` that takes more than 1 second is counted as a loss: under very high latency,
  the loss is overestimated.
- Packets dropped by the server rate limit (90 packets per second per client) are counted
  as lost, because from the client they are.

In `net_metrics_test`, a link that drops one packet in five in each direction gives about
44 % estimated loss, and a perfect link gives 0 %.

### Throughput

`bytesSent` and `bytesReceived` add the size of every datagram written to or read from the
socket, the 9-byte header included and the IP and UDP headers (28 bytes per datagram) not
included. Every send goes through one function, so no packet escapes the count, and every
received datagram is counted, even when it is then rejected.

`bytesSentPerSecond` and `bytesReceivedPerSecond` are measured on windows of one second:
when one second has passed, the bytes of the window are divided by the time that really
passed, so a long frame still gives the right average. The value then stays the same until
the next window.

In `net_metrics_test`, the bytes the client counts are exactly the bytes the real server
counts in its own statistics, in both directions.

### Rejected packets

| Reason | Examples |
|--------|----------|
| `sender` | The datagram does not come from the server port |
| `bad` | Shorter than the header, wrong `magic`, wrong `payloadSize`, truncated payload, invalid snapshot or room list, `ConnectAck` with a zero token |
| `dup` | A reliable packet received again with the same `sequence`, or a spawn for an id the client already has |
| `stale` | An event, `InputAck` or snapshot older than the last one applied, an answer that came too late, an unknown `Pong` |
| `outside` | A gameplay packet received while the client is not in a room, such as a late packet from the room it just left |

A rejected packet is still counted in `packets in` and `in`. Reliable packets are still
acknowledged before being rejected. A few stale and duplicate packets are normal: the
server sends the joining player its own spawn, health and score twice
([protocol.md, section 5](protocol.md#5-connection-packets)).

### Retransmissions

- `resent out` counts every retry of `ConnectReq`, `RoomJoinReq` and the lobby requests.
  The first send is not counted. Inputs and snapshots are never resent.
- `resent in` counts the reliable packets received a second time with the same
  `sequence`: the server sent them again because our `Ack` was lost or late. The client
  cannot see retries of which no copy arrived; the exact number is
  `reliableRetransmitted` in the server statistics, printed in the server log every
  10 seconds.

### Prediction corrections

When an `InputAck` arrives, `LocalPrediction` compares the predicted position with the one
the server confirmed, after replaying the inputs not confirmed yet.

- A difference larger than `[network] prediction_correction_threshold` (**0.5 px**) counts
  as a correction. Smaller differences are rounding.
- A difference larger than `prediction_snap_distance` (120 px) is a **snap**: the ship is
  moved at once instead of sliding. A snap also counts as a correction.

With a working prediction both counters stay at 0 on a local server. They grow when the
link adds latency and jitter (see `scripts/lag_relay.py` in the README), or when the client
and the server disagree on the movement rules.

## Checking an improvement

1. Start the server, the lag relay and a client connected to the relay port:
   `./build/r-type_server`, then
   `python3 scripts/lag_relay.py --delay 30 --jitter 60 --loss 0.1 --direction both`, then
   `./build/r-type_client -p 4243`.
2. Play for at least 30 seconds, then note `rtt`, `jitter`, `loss`, `in`, `out`,
   `resent` and `corrections`.
3. Apply the change, run the same scenario, and compare. Throughput is the most stable
   number; loss needs at least 20 seconds to settle.
