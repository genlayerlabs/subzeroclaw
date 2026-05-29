<p align="center">
  <img src="szc-logo.png" alt="SubZeroClaw" width="400">
</p>

# SubZeroClaw — `mock` branch

> **This is the `mock` branch.** It adds optional mock and record modes for offline development without touching the minimal core. The pristine ~450-line version lives on [`main`](../../tree/main). The runtime here is byte-for-byte identical when no mock/record env vars are set.

> **WARNING: This software executes arbitrary shell commands with no safety checks, no confirmation prompts, no sandboxing, and no guardrails. The LLM decides what to run and the runtime runs it — `rm -rf /` included. There is nothing between the model's output and your system. If you don't understand what that means, do not use this. This is a bare agentic loop: execute the task, whatever it takes, nothing more, nothing less.**

**~450 lines of C core + ~140 lines optional mock/record module. A skill-driven agentic daemon for edge hardware.**

```
skill.md + LLM + shell + loop = autonomous agent
```

Every agentic runtime does the same thing: read a skill, call an LLM, execute tools, loop. SubZeroClaw is that principle written directly in C — no framework, no abstractions, no architecture mimicking a problem that never existed. One file, one loop, one tool.

## What it does

You write a skill as a markdown file. You point SubZeroClaw at it. It calls an LLM, executes tools, loops until done. That's the entire runtime.

```
~/.subzeroclaw/skills/monitor.md    ← what the agent knows
~/.subzeroclaw/config               ← API key + model
~/.subzeroclaw/logs/<session>.txt   ← full I/O trace
```

The agent reads the skill into its system prompt, receives input, and autonomously calls tools until the task is complete. When context gets full, it compacts old messages into a summary and keeps going.

## Why not just use ZeroClaw / OpenClaw?

[ZeroClaw](https://github.com/zeroclaw-labs/zeroclaw) rewrites [OpenClaw](https://github.com/openclaw) in Rust. It's good software — but it inherits the architecture of the thing it's replacing: trait systems, channel adapters, observer patterns, identity formats, security layers. All solutions to problems that exist when you're building a multi-user, multi-channel platform.

If your problem is "run one skill on one Pi", none of that applies. You don't need channel adapters because there's one channel. You don't need a security model because you wrote the skill. You don't need a trait system because there's one provider.

SubZeroClaw doesn't simplify their architecture. It ignores it and writes the loop directly.

|                   | SubZeroClaw  | ZeroClaw     | OpenClaw     |
|-------------------|--------------|--------------|--------------|
| Language          | C            | Rust         | TypeScript   |
| Source            | ~450 + 140   | ~15,000      | ~430,000     |
| Binary            | 54 KB        | 3.4 MB       | 80+ MB       |
| RAM (runtime)     | ~2 MB        | < 5 MB       | 80-120 MB    |
| Compiles on Pi    | 0.5s         | OOM          | slow         |
| Dependencies      | curl, cJSON  | ~100 crates  | ~800 npm     |

## Tool

One tool: **shell**. `popen()` any command, stderr merged into stdout.

Since the LLM has a shell, it has `git`, `curl`, `himalaya`, `signal-cli`, `ffmpeg`, `jq`, `khal`, `pass` — whatever you install. For file operations, the model uses `cat`, `tee`, `sed`, etc. No adapters, no integrations. The adapter is the shell.

## Skills

Drop a `.md` file in `~/.subzeroclaw/skills/`. It becomes part of the system prompt.

```bash
cat > ~/.subzeroclaw/skills/backup.md << 'EOF'
## Backup Agent
You monitor /home/pi/data every hour.
- Run `rsync -avz /home/pi/data pi@nas:/backup/`
- If rsync fails, retry 3 times with 30s delay
- Log results to /home/pi/backup.log
EOF
```

No format spec. No skill registry. No trigger matching. Just plain text the LLM reads.

The skills included in this repo (`skills/`) are just examples to show the format. They reference tools and paths specific to one setup. Don't use them as-is — write your own for your system, your tools, your workflow. The whole point is that a skill is just a markdown file you write in 30 seconds.

## Mock and record modes

Two opt-in modes for development without spending API calls. Both are triggered by env vars only — no CLI flags, no config changes, no runtime cost when unused.

```bash
# Replay canned LLM responses, no network, no API key needed
SUBZEROCLAW_MOCK_SCRIPT=script.json ./subzeroclaw "deploy the app"

# Record real API responses to a file (which is itself a replay-able mock script)
SUBZEROCLAW_RECORD_SCRIPT=session.json ./subzeroclaw "deploy the app"
```

Both can be set at the same time — `MOCK` wins if both are present.

### What gets mocked, what doesn't

Only the LLM response is canned. Everything else — skills loading, shell execution, context compaction, logging — runs for real. If the canned response says "run `echo hello`", the shell really runs `echo hello`. The mock script is a transcript of the model's brain, not of the world.

### Mock script format

A JSON array of message objects. Each entry is one LLM turn, consumed in order. When the script runs out, the last entry repeats.

```json
[
  {
    "content": null,
    "tool_calls": [
      {
        "id": "call_1",
        "type": "function",
        "function": {
          "name": "shell",
          "arguments": "{\"command\": \"echo hello\"}"
        }
      }
    ]
  },
  {
    "content": "Done. The command printed hello."
  }
]
```

First entry: a tool call (`finish_reason` is inferred as `tool_calls` because `tool_calls` is non-empty). Second entry: a plain message (`finish_reason` becomes `stop`). The runtime treats this exactly like an OpenAI chat completion.

### Recording

Record mode wraps a real session and writes each LLM message to a file in mock-script format. You can replay the session afterwards by pointing `SUBZEROCLAW_MOCK_SCRIPT` at the same file. Useful for capturing a known-good run as a regression fixture.

### How it works (no `#ifdef` in the core)

The core `subzeroclaw.c` exposes `llm_chat` as a weak symbol. `src/szc_mock.c` provides a non-weak version that overrides it at link time. When the optional module is linked in (this branch's default), all LLM calls flow through it; the module checks the env vars on first call and routes to mock, record, or the real path accordingly.

The core file gains nothing: no mock-specific fields in `Config`, no flag parsing in `main`, no `#ifdef` blocks. The diff between this branch and `main` is two `static` keywords removed (so the mock module can call `build_request` / `http_post`) plus the new `src/szc_mock.c` file and a Makefile line.

## Build

```bash
make            # builds subzeroclaw with the mock/record module linked in
make watchdog   # builds watchdog (17KB)
make test       # runs 19 tests
make install    # copies to ~/.local/bin/
```

Requires `libcjson-dev` or uses vendored cJSON automatically.

## Setup

```bash
mkdir -p ~/.subzeroclaw/skills

cat > ~/.subzeroclaw/config << EOF
api_key = "sk-or-your-openrouter-key"
model = "minimax/minimax-m2.5"
EOF
```

Or just use the `.env.example`:

```bash
cp .env.example .env
# Edit .env with your real API key
source .env
```

Environment variables override the config file:

```
SUBZEROCLAW_API_KEY
SUBZEROCLAW_MODEL
SUBZEROCLAW_ENDPOINT
```

## Usage

```bash
# One-shot task
./subzeroclaw "check disk usage and clean tmp if over 80%"

# Interactive
./subzeroclaw

# Daemon with watchdog (restarts on crash, exponential backoff)
./watchdog ./subzeroclaw "run the backup skill"
```

## Session logging

Every session gets a random hex ID. All input, output, tool calls, and results are logged to `~/.subzeroclaw/logs/<session>.txt` with timestamps.

```
=== f850c58ddd4ae72a Sun Feb 16 16:30:01 2026
[2026-02-16 16:30:01] USER: check disk usage
[2026-02-16 16:30:03] TOOL: shell
[2026-02-16 16:30:03] RES: /dev/sda1  72% /
[2026-02-16 16:30:04] ASST: Disk usage is at 72%, below threshold.
```

## Context compaction

When the message history exceeds `max_messages` (default 40), the agent:

1. Sends old messages to the LLM for summarization
2. Replaces them with the summary
3. Keeps the last N raw messages intact

No vector DB. No embeddings. One API call to compress context.

## Config reference

| Key | Default | Description |
|-----|---------|-------------|
| `api_key` | (required) | OpenRouter API key |
| `model` | `minimax/minimax-m2.5` | Any OpenAI-compatible model |
| `endpoint` | `https://openrouter.ai/api/v1/chat/completions` | API endpoint |
| `skills_dir` | `~/.subzeroclaw/skills` | Path to skill markdown files |
| `log_dir` | `~/.subzeroclaw/logs` | Session log directory |
| `max_turns` | 200 | Max tool-call loops per input |
| `max_messages` | 40 | Trigger context compaction |

## Source

```
src/
├── subzeroclaw.c   ~450 lines  The entire core runtime
├── szc_mock.c      ~140 lines  Optional mock/record (link-time override)
├── test.c                      19 tests
├── watchdog.c       50 lines   Crash recovery + backoff
├── cJSON.c                     Vendored JSON parser
└── cJSON.h
```

## Philosophy

SubZeroClaw is an anti-framework. It does one thing: connect an LLM to a shell and loop. That's it. No plugin system, no middleware, no lifecycle hooks, no dependency injection. Just the loop.

This follows the Unix philosophy — do one thing and do it well. `grep` searches text. `curl` fetches URLs. `subzeroclaw` runs an agentic loop. It doesn't need to know about git, email, HTTP, or filesystems because the tools that already do those things are one `popen()` away. The model calls `git` the same way you do. The entire system is the integration layer.

Every layer of "framework" between the model and the shell is complexity that adds nothing. If the model can run `git`, why build a git adapter? If it can run `curl`, why build an HTTP tool? If it can run `tee`, why build a file-writing abstraction? Frameworks grow because they solve problems that emerge from their own architecture — channel routing because they support multiple channels, plugin registries because they have plugins, security models because they run untrusted code. SubZeroClaw has none of these problems because it doesn't have any of these features. One agent, one skill, one device. The code that remains is the code that can't be removed.

OpenClaw solved the agentic loop with 430,000 lines of TypeScript. ZeroClaw re-solved it with 15,000 lines of Rust. Both are good — but both carry the weight of problems that only exist at platform scale: multi-tenancy, channel routing, identity portability, plugin registries.

SubZeroClaw asks: what if the problem is just "one agent, one skill, one device"? Then the answer is ~450 readable lines of C. The mock/record module on this branch follows the same rule: it's a separate file you can drop or keep, linked into the binary without a single conditional in the core.

## License

MIT
