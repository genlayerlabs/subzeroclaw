---
name: subzeroclaw-contribute
description: >-
  Develop SubZeroClaw — the single-file ~550-line C agentic runtime. Load this
  before changing src/subzeroclaw.c or src/test.c: it carries the anti-framework
  thesis (the goal is NOT to grow), the code map (the loop, config, the shell
  tool, async compaction), what will and won't be merged, and the build/test
  loop. To merely run or configure SubZeroClaw, use subzeroclaw-use.
---

# Contributing to SubZeroClaw

SubZeroClaw is an **anti-framework**: the goal is not to grow but to stay minimal,
readable and correct. The whole runtime is one file, `src/subzeroclaw.c`
(~550 lines). Before you add anything, read `CONTRIBUTING.md` — a PR that *adds*
surface has to clear a high bar, and several categories are refused outright.

## The two contributions that are welcome

1. **Reduce complexity** — same behaviour with less code, fewer moving parts, or
   clearer logic: remove a function that doesn't earn its place, flatten a control
   path, drop a dependency or an allocation, replace a hand-rolled pattern with
   libc. A PR that removes lines while keeping tests green is probably good.
2. **Prove a runtime limitation** — a task where "skill + shell + LLM loop" genuinely
   breaks down *in the runtime, not the skill*. The honest test: could a better
   `.md` skill fix it? If yes, it's a skill problem, not a SubZeroClaw one. If the
   limitation is structural (e.g. binary tool output can't round-trip through the
   JSON string protocol), that's worth discussing. Open an issue with the task, the
   skill you wrote, what happened, and why no skill can work around it.

**Won't be merged:** new tools (the shell is the only tool — the model runs `git`,
`tee`, `curl` itself); plugin systems, hooks, event buses, middleware (multi-user
platform problems SubZeroClaw doesn't have); backward-compatibility shims.

## Code map (`src/subzeroclaw.c`)

The loop, top to bottom:

| Function | Role |
|---|---|
| `config_parse_line` / `config_load` | parse the config keys; env overrides; **scrub the four provider secrets** (`SUBZEROCLAW_API_KEY`/`_ENDPOINT`/`_REQUEST_EXTRA`/`_COMPACT_EXTRA`) from the environment (wipe-in-place + `unsetenv`) so the model's shell never sees the key. `SUBZEROCLAW_SKILLS` is preserved on purpose. |
| `main` | read config + skills, build the system prompt, seed the message array, drive the loop. |
| `agent_run` | the turn loop: POST to `endpoint`, parse, dispatch tools, until a stop or `max_turns`. |
| `parse_response` | pull assistant text / tool calls / errors out of the completion JSON. |
| `process_tool_calls` | the **one tool**: run each shell command via `popen()` (stderr merged), append results as turns. |
| `round_has_command` | guard: did this round actually request a shell command? |
| `compact_fire` / `compact_splice` / `compact_url` | async context sealing: on the router's `x_router.compact` signal, fire `/v1/compact` in the background and splice the sealed block in ahead of intervening turns — no turn is ever blocked. |
| `log_write`, `write_temp`, `mkdirp` | session logging + scratch files. |

`cJSON` is vendored (`src/cJSON.c/.h`) and used automatically when system
`libcjson` is absent (the Makefile detects it via `pkg-config`).

## Build & test

```bash
make            # single-file build → subzeroclaw (needs gcc; -lm or -lcjson)
make test       # builds test_subzeroclaw and runs the suite → "31 passed, 0 failed"
make install    # → ~/.local/bin/
make clean
```

`src/test.c` `#include`s `src/subzeroclaw.c` directly and defines `SZC_TEST` to
exclude `main`, so tests link against the real functions. **All 31 tests must
pass**, and if you change `subzeroclaw.c` you must update `test.c` to match — the
suite covers the shell tool, turn framing, request building + `request_extra`
merge, response/error parsing, the compact flag + splice, tool dispatch, the
system prompt, skill loading, and config (including the env scrub).

## The bar, restated

Every line justifies its existence. The code that remains is the code that can't
be removed. If your change grows the file, the burden is on you to show the
runtime — not a skill — needed it. See `CONTRIBUTING.md` for the full policy.
