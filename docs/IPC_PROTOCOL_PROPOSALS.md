# IPC Protocol: Proposed Extensions

> Status: **Proposal** (nothing in this document is implemented).
> The current, implemented protocol is [IPC_PROTOCOL.md](./IPC_PROTOCOL.md).

This document lists the changes to `--ipc stdio` that would give the
[web control panel](./WEB_CONTROL_PANEL.md) (and any other client) more control over the
simulator, ordered by value. Each item states the limitation it removes, what it would look
like on the wire, and how it stays compatible with today's clients.

## 1. What limits the panel today

These are observed behaviours of the current implementation (`src/ipc/IpcSession.cpp`),
not opinions.

| # | Limitation | Consequence for the panel |
|---|---|---|
| L1 | The command loop is a single `std::getline(std::cin, ...)` and `run_full` runs **inside** that loop. | While a run is in progress no command is read: it cannot be paused, cancelled, or queried. The panel can only kill the process. `get_status.session_running` is never observable as `true` during a run. |
| L2 | When `run_full` / `run_ticks` fails while running, the simulator emits `error` and `simulation_finished` but **no ack/error for the request**. | A client that waits for the response of that request waits forever. The panel had to learn to treat `simulation_finished` with `success:false` as the failure, and to put a deadline on every wait. |
| L3 | `simulation_finished` for `run_ticks` has no `success` field (only `run_full` has it). | Clients cannot tell a finished chunk from a failed one without an extra `error` event. |
| L4 | `initialize` takes no payload: everything comes from `--config` at process start. | Changing plugins, plugin parameters or gravity means restarting the process (the panel does exactly that on every run). |
| L5 | No discovery: no way to ask which plugins are loaded, what parameters they accept, or what they provide. | The panel hard-codes plugin knowledge (for example which plugins apply gravity, in `plugin_registry.py`), which goes stale. |
| L6 | `state_sample` events exist but are not specified (fields, units, rate) and the panel does not use them. | No live trajectory; results are read from the CSV after the run. |
| L7 | Plugin configuration errors (`plugin_configure` return codes) only surface as log lines. | The panel cannot tell the user "plugin X rejected parameter Y". |
| L8 | No protocol version or capability handshake. | Clients cannot adapt to older/newer simulators, so new features would be guesses. |
| L9 | Events carry `event_seq` but not the `request_id` that caused them. | With more than one command in flight, events cannot be attributed. |

## 2. A first step that needs no protocol change

`run_ticks` already exists. A client can drive a run as a sequence of chunks
(`run_ticks {count: N}` repeated) and, **between** chunks, stop, cancel gracefully, or update
a progress bar from the ack. This would give cooperative cancellation (L1) and a natural pause,
at the cost of per-chunk overhead and with these points to verify before relying on it:

- that `run_ticks` honours the same stop conditions as `run_full`;
- that output files are finalized correctly when the client stops between chunks;
- the per-chunk overhead for small `count`.

It does not remove L2/L3 and does not allow changes mid-run, so it complements the proposals below.

## 3. Proposals

Priorities: **P1** removes a defect or unlocks the rest, **P2** adds user-visible control,
**P3** is convenience.

### P1-a. Always answer the request (fixes L2, L3)

Every command gets exactly one terminal response, success or failure.

```json
{"type":"error","ok":false,"request_id":"3","error":{"code":"engine_error","message":"Integration diverged at tick 7","details":{"tick":7,"sim_time":0.7}}}
```

- `simulation_finished` keeps being emitted, now with `success` for **all** modes.
- Compatible: clients that already treat `simulation_finished{success:false}` as failure keep
  working; they just also receive the response.

### P1-b. Handshake and capabilities (fixes L8, helps L5)

```json
{"type":"command","id":"0","name":"hello","payload":{"client":"molab-web","protocol":1}}
```
```json
{"type":"ack","ok":true,"request_id":"0","data":{
  "protocol":1,
  "simulator_version":"…",
  "features":["list_plugins","initialize_overrides","pause","get_state","state_sample"]
}}
```

Clients enable only the features listed. An old simulator answers `unsupported_command` and the
client falls back to today's behaviour.

### P1-c. Plugin discovery (fixes L5)

```json
{"type":"command","id":"1","name":"list_plugins"}
```
```json
{"type":"ack","ok":true,"request_id":"1","data":{"plugins":[{
  "name":"example_plugin",
  "type":"parallel",
  "loaded":true,
  "enabled":true,
  "provides":["force","gravity:state_vector"],
  "parameters":[
    {"key":"apply_gravity","type":"bool","default":false},
    {"key":"gravity_scale","type":"number","default":1.0,"min":0.0},
    {"key":"log_interval_ticks","type":"integer","default":100,"min":1}
  ]
}]}}
```

This needs one new optional export in the plugin API (for example `plugin_describe`,
returning the JSON above). Plugins without it are listed with `parameters: null`. With it the
panel can build parameter forms and the "who applies gravity" logic from the plugins themselves
instead of from a hard-coded table.

### P1-d. Structured plugin errors (fixes L7)

When `plugin_configure` or `plugin_tick` returns non-zero, emit:

```json
{"type":"event","event":"plugin_error","event_seq":9,"payload":{"plugin":"example_plugin","phase":"configure","code":-3,"message":"drag_coefficient must be a finite number >= 0"}}
```

and use `invalid_parameter` / `plugin_error` as response error codes when it happens during a
command. The Example Plugin already produces such messages; today they are only log text.

### P2-a. Configuration at `initialize` (fixes L4)

```json
{"type":"command","id":"2","name":"initialize","payload":{
  "plugins":[{"name":"example_plugin","enabled":true,"parameters":{"apply_gravity":true}}],
  "environment":{"gravity":{"enabled":true,"magnitude":9.81,"direction":[0,0,-1]}}
}}
```

- Overrides are validated against `list_plugins` and rejected as a whole (`invalid_payload`
  with `details` naming the field), so a bad value never produces a half-configured engine.
- Gravity moves from "the web server rewrites the initial-state file" to a first-class,
  validated input of the environment domain. This is the natural home for it now that gravity
  is no longer a core concern.
- The panel could then keep a single simulator process alive between runs.

### P2-b. Concurrent command reading: `abort`, `pause`, `resume` (fixes L1)

Read stdin on its own thread and give the run loop a control flag.

```json
{"type":"command","id":"7","name":"pause"}
{"type":"command","id":"8","name":"resume"}
{"type":"command","id":"9","name":"abort"}
```

- `abort` ends the run cleanly (output finalized, `simulation_finished{success:false,reason:"aborted"}`).
- `get_status` becomes meaningful during a run (`session_running:true`, live `tick`).
- Commands that cannot be honoured in the current state return `invalid_state`.

### P2-c. State access (fixes L6)

```json
{"type":"command","id":"10","name":"get_state","payload":{"fields":["position","velocity","total_mass","gravity"]}}
```

- Specify the `state_sample` event (fields, units, ordering, what `telemetry_interval_ticks`
  means) and add a `subscribe`/`unsubscribe` pair for field selection and rate, so the panel
  can draw the trajectory while the run is in progress.
- `get_state` returns one snapshot, valid also while paused.

### P2-d. Request correlation (fixes L9)

Add `request_id` to events produced while serving a command (`log`, `error`,
`simulation_started`, `tick_completed`, `simulation_finished`). Events not tied to a command
keep omitting it.

### P3. Convenience

- `set_parameter` for a whitelist of plugin parameters that are safe to change while paused,
  validated with the schema from `list_plugins`.
- `checkpoint` / `restore` of the state buffer to re-run from a point.
- `list_outputs` returning the files the run produced (the panel currently searches the
  filesystem for them).
- Rate limits and a `dropped_events` counter in `simulation_finished` for slow readers.

## 4. What the web panel gains

| Proposal | Panel feature that becomes possible or simpler |
|---|---|
| P1-a | No special-casing of failed runs; every wait has a definite answer |
| P1-b, P1-c | Plugin list, parameter forms and gravity providers come from the simulator; `GRAVITY_PROVIDERS` and the per-plugin notes in `plugin_registry.py` can be deleted |
| P1-d | "Plugin X rejected parameter Y" shown in the panel instead of a log line |
| P2-a | One long-lived simulator process, validated gravity/plugin inputs, no temporary initial-state files |
| P2-b | Pause / resume / graceful cancel buttons |
| P2-c | Live trajectory and telemetry while the simulation runs |
| P2-d | Several clients or concurrent commands without mixing up events |

## 5. Compatibility and migration

1. Everything is additive. Existing commands, fields and event names keep their meaning.
2. New behaviour is gated by `hello.features`, so a client built for this document still works
   against an old simulator (it just sees fewer features).
3. New error codes proposed: `invalid_parameter`, `plugin_error`, `not_supported`, `aborted`.
4. Suggested order: P1-a (small, fixes a defect) → P1-b + P1-c + P1-d (one plugin-API change) →
   P2-a → P2-b → P2-c/P2-d.
5. Update `docs/IPC_PROTOCOL.md` in the same change as each item and add the matching
   command to `tests/fake_simulator.py` so the web tests keep covering the real wire format.

## 6. Open questions

- Should `initialize` be callable more than once per process (re-initialize with new overrides)?
  P2-a assumes yes.
- Is a second thread reading stdin acceptable for the engine's threading model (P2-b)?
- Which `GeneralState` fields are safe to expose through `get_state` / `state_sample` without
  copying the whole buffer?
- Should `plugin_describe` be mandatory for new plugins?

---

Back to: [`docs/README.md`](./README.md)
