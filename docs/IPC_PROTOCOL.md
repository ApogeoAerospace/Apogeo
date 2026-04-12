# IPC Protocol (`--ipc stdio`)

`simulator --ipc stdio` enables line-delimited JSON IPC over stdin/stdout.

## Framing

- Input: one JSON command object per line on stdin.
- Output: one JSON response/event object per line on stdout.

## Command envelope

```json
{"type":"command","id":"1","name":"get_status","payload":{}}
```

Compatibility aliases are accepted:

- `name` or `command`
- `id` or `request_id`

Notes:

- The parser currently routes commands by `name`/`command` fields.
- `type` is recommended to be `"command"` for forward compatibility, but is not strictly enforced by the current parser.

## Responses

### Ack

```json
{"type":"ack","ok":true,"request_id":"1","data":{}}
```

### Error

```json
{"type":"error","ok":false,"request_id":"1","error":{"code":"invalid_payload","message":"...","details":{}}}
```

Common error codes:

- `parse_error`
- `invalid_payload`
- `invalid_state`
- `unsupported_command`
- `engine_error`

## Supported commands

## `get_status`

Request:

```json
{"type":"command","id":"1","name":"get_status"}
```

Ack data:

- `initialized` (`bool`)
- `session_running` (`bool`) - current command-session execution state
- `engine_started` (`bool`) - internal engine start-state flag from `SimulationEngine::getStatus()` (may remain `true` after a run completes)
- `tick` (`integer`)
- `sim_time` (`number`)
- `last_tick_duration` (`number`, milliseconds)
- `compute_tick_duration_ms` (`number`, milliseconds)
- `io_tick_duration_ms` (`number`, milliseconds)

## `initialize`

Initializes engine using already loaded configuration (`--config` at startup).

Request:

```json
{"type":"command","id":"2","name":"initialize"}
```

## `run_ticks`

Request:

```json
{"type":"command","id":"3","name":"run_ticks","payload":{"count":10}}
```

Optional payload overrides:

- `payload.tick_event_interval` (`integer > 0`): overrides config default for `tick_completed` event rate.
- `payload.telemetry_interval_ticks` (`integer > 0`): overrides config default for `state_sample` event rate.

In IPC mode, `telemetry_interval_ticks` is also applied to runtime output sampling cadence.

Constraints:

- `payload.count` must be a positive integer.

Response behavior:

- On success: emits `simulation_started`/`tick_completed` events and returns an `ack`.
- On failure during execution: emits an `error` and `simulation_finished`; no success `ack` is emitted for that request.

## `run_full`

Runs until simulation stop conditions from configuration are met.

Request:

```json
{"type":"command","id":"4","name":"run_full"}
```

Optional payload overrides:

- `payload.tick_event_interval` (`integer > 0`)
- `payload.telemetry_interval_ticks` (`integer > 0`)

Response behavior:

- On success: emits `simulation_started`, throttled `tick_completed`, `simulation_finished`, then an `ack`.
- On failure during execution: emits an `error` and `simulation_finished`; no success `ack` is emitted for that request.

## `shutdown`

Stops and finalizes engine resources.

Request:

```json
{"type":"command","id":"5","name":"shutdown"}
```

Behavior notes:

- `shutdown` finalizes engine resources and returns an `ack` with `{ "shutdown": true }`.
- In `--ipc stdio` mode, this command does **not** terminate the simulator process by itself; the command loop remains active until stdin is closed.

## Events

IPC mode emits asynchronous events:

- `log`
- `error`
- `simulation_started`
- `simulation_finished`
- `tick_completed` (throttled)
- `state_sample` (from `OutputManager` realtime callback)

All event messages include `event_seq` (monotonic session-local sequence number).

### Startup log replay behavior

- On `--ipc stdio` session start, the simulator replays recent logger history as `log` events.
- Replay is capped to the latest `256` accepted log entries.
- Replayed `ERROR`/`CRITICAL` entries also emit `error` events.
- In IPC mode, plain console log lines are disabled to keep stdout JSON-only.

Example:

```json
{"type":"event","event":"tick_completed","event_seq":12,"payload":{"tick":50,"sim_time":0.5,"last_tick_duration":0.14,"compute_tick_duration_ms":0.11,"io_tick_duration_ms":0.03}}
```

## Notes

- Classic CLI mode (without `--ipc stdio`) remains the default and supported path.
- Configuration edits should be done through config files/CLI options, not through IPC commands.
