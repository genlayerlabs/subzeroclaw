---
name: subzeroclaw-use
description: >-
  Run and operate SubZeroClaw — the ~550-line C agentic daemon (skill.md + LLM +
  shell + loop). Load this to build it, configure it (~/.subzeroclaw/config),
  write an agent skill, run a task, wire it to an unhardcoded router for
  routing/cache/compaction, or run it as a systemd service with credential
  isolation. WARNING: it executes arbitrary shell with no sandbox. To develop
  the runtime itself, use subzeroclaw-contribute.
---

# Using SubZeroClaw

**Read the warning first.** SubZeroClaw runs whatever the model emits through
`popen()` — no confirmation, no sandbox, `rm -rf /` included. There is nothing
between the model's output and your system. Run it only where you accept that,
ideally as an unprivileged service user (see *As a service*).

The whole runtime is: **read a skill (markdown) → call an LLM → run shell tools →
loop until done**. One tool: `shell`. The "adapter" for git/curl/email/ffmpeg is
that they're one `popen()` away — install the CLI, the model uses it.

> **Two senses of "skill" — don't conflate them.** A *SubZeroClaw skill* is a
> plain `.md` file in `~/.subzeroclaw/skills/` that becomes the agent's system
> prompt (what the agent knows). *This* file is a Claude Code skill (how to
> operate the tool). When the docs below say "write a skill", they mean the
> former.

## Build & run

```bash
git clone https://github.com/genlayerlabs/subzeroclaw && cd subzeroclaw
make                                   # ~0.5s → the 55KB binary (needs gcc; curl at runtime)
mkdir -p ~/.subzeroclaw/skills
# ...write ~/.subzeroclaw/config (below) and a skill .md...
./subzeroclaw "check disk usage and clean tmp if over 80%"   # one-shot
./subzeroclaw                                                 # interactive
make install                           # → ~/.local/bin/
```

An agent skill is just prose the LLM reads — no format, no registry, no trigger
matching. Drop a `.md` in `~/.subzeroclaw/skills/` and it joins the system
prompt. The repo's `skills/*.md` are format *examples* tied to one setup; write
your own.

## Configuration (`~/.subzeroclaw/config`)

These are the only keys the runtime parses (`config_parse_line`); `config.example`
is the canonical template.

| Key | Meaning |
|---|---|
| `api_key` | The unhardcoded router **consumer key** (`llmr_…`) — SubZeroClaw is designed to run on the router. A bare OpenRouter/provider key also works but is a degraded loop. **Required.** |
| `request_extra` | JSON merged into every request body. **Carries the model** (`{"model":"..."}`) — there is no dedicated `model` key. Against a router it also carries the routing `policy_ir`. Override wins on key collision. |
| `compact_extra` | JSON enabling async compaction: `keep_recent` + a cheap summariser `policy_ir`. Unset → no compaction. |
| `endpoint` | default OpenRouter; point at an unhardcoded router for routing/cache/compaction. |
| `skills_dir`, `log_dir` | defaults under `~/.subzeroclaw/`. |
| `max_turns` | tool-call loops per input (default 200). |

Env vars `SUBZEROCLAW_API_KEY`, `SUBZEROCLAW_ENDPOINT`, `SUBZEROCLAW_REQUEST_EXTRA`,
`SUBZEROCLAW_COMPACT_EXTRA` override the file.

## The unhardcoded substrate (routing + compaction)

Routing with prompt-cache affinity and context compaction are *not* part of
"skill + LLM + shell + loop" — they live in the substrate. Point `endpoint` at an
[unhardcoded](https://github.com/genlayerlabs/unhardcoded) router and express them
as JSON:
- **Routing:** `request_extra = {"model":"policy:auto","policy_ir":[ … ]}`. The
  runtime sends a per-run `session` id so the router keeps the conversation on its
  cache-hot peer. (Author the `policy_ir` with the **`unhardcoded-use`** skill.)
- **Compaction:** on an `x_router.compact` signal, the runtime fires an
  append-only seal at the router's `/v1/compact` **in the background** and keeps
  taking turns, splicing the sealed block in ahead of the turns that arrived
  meanwhile — no pause, the cache prefix is never rewritten. `compact_extra`
  carries `keep_recent` + the summariser policy.

Pointed at a bare provider the HTTP call still fires, but it's a **degraded** loop
(no routing, cache, or compaction) — not a supported standalone mode.

## As a service (and credential isolation)

SubZeroClaw doesn't supervise itself — bring your init system. A systemd unit with
`Restart=on-failure` and an `EnvironmentFile` (root-owned, `chmod 600`) holding
`SUBZEROCLAW_API_KEY` gets you restart + credential isolation. The runtime
**scrubs the four provider secrets** (`SUBZEROCLAW_API_KEY`, `_ENDPOINT`,
`_REQUEST_EXTRA`, `_COMPACT_EXTRA`) from its own environment right after
`config_load` — wiping the value in place, then `unsetenv` — so the shell it hands
the model never inherits the key (`cat /proc/self/environ` comes up empty).
Non-secret vars like `SUBZEROCLAW_SKILLS` are deliberately kept (the genswarms
wrapper sets it). The supervisor owns the secret; the shell stays unguarded by
design — the scrub only keeps the runtime's *own* secrets out of it.

## Gotchas

- **A top-level `model =` line in the config is ignored** — the runtime never
  parses it. The model rides in `request_extra`. (This is the most common
  misconfiguration.)
- **Use a tool-calling model.** If the model can't call tools it will loop without
  progress. Set it in `request_extra`.
- **`curl` is required at runtime** (API calls shell out to it); `gcc` only to build.
- **Compaction is opt-in:** unset `compact_extra` → context grows until the
  provider refuses it.

## Where to read more

- `README.md` — the full config reference, session logging, troubleshooting table.
- `config.example` — the canonical config template.
- The **`unhardcoded-use`** / `sigma-policy-author` skills — author the routing and
  summariser `policy_ir` this runtime sends.
