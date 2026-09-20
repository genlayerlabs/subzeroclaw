# Decision-model controller

The runtime calls `/v1/decisions` with observed state and a finite action set.
A chosen action executes through the existing shell. Generation runs only when
the controller selects a generation option. The decision model selects procedures
and their arguments together; it never emits shell code.

This is an opt-in mode: set `decision_extra` to a JSON object. Without it the
existing generative loop and asynchronous `/v1/compact` behavior stay available.
No vendor or model name is compiled into the controller.

Transient HTTP failures use curl’s bounded retry policy (at most two retries).
The same inference request is retried; previously executed shell actions are
not repeated. Permanent errors such as HTTP 401 are not retried. Retried
inference can incur additional provider cost, so meter all HTTP attempts.

## Configure

Use a router consumer key and its `/v1/chat/completions` endpoint as usual.
`decision_extra` configures decision inference. `request_extra` supplies the
capable generation policy and optional `economy_extra` supplies an economical
one. The controller chooses an action, `generate_economy`, `generate_capable`,
or `finish` in **one decision**. Both generation configurations must contain
direct `policy_ir` (no nested `flow_ir`, messages or tools). The generation request defaults to `response_format: {"type":"json_object"}`
(unless supplied by the operator), so its policy must admit a compatible model.
Truncated output and multiple JSON objects are rejected without execution;
recovery receives a specific error. The router still
selects the actual model offer/provider and handles fallback within that policy.

Generate config using the **exact model families in your router catalog**:

```sh
python3 examples/decision-models/configure.py \
  --economy-family YOUR_LUNA_FAMILY \
  --capable-family YOUR_ASTRA_FAMILY
```

Copy its four lines into `~/.subzeroclaw/config`. Policies choose healthy
compatible offers, rank input price, and allow provider fallback. The decision
endpoint constrains the protocol, so accepting multiple decision-model families
cannot route a decision to a chat model. You can add `--decision-family` to pin
an evaluated family, or replace the generated policy with your own constraints.

The controller sees bounded history, commands, results and available arguments.
An invalid decision stops without executing a command. There is no second model
routing decision after the controller chooses generation. Choosing an economical
model is not evidence that it will solve a particular task. Provider restrictions
belong in every applicable policy.

Without `economy_extra`, the controller has a single `generate` option using
`request_extra` as supplied (including an operator-configured flow). Example minimum
configuration for decision inference (routing defaults are supplied by the router):

```ini
decision_extra = {}
decision_context_bytes = 18000
```

The equivalent environment variables are `SUBZEROCLAW_DECISION_EXTRA` and
`SUBZEROCLAW_ECONOMY_EXTRA`. They are scrubbed before shell execution, like the existing inference configuration.
`max_turns` bounds controller iterations, including repeated generation attempts.
There are at most 24 proposed actions; requests exceeding the router's 32,000-byte
limit fail before inference, rather than silently losing instructions.

## Action contract

The runtime tells the generative model to return an agenda:

```json
{
  "actions": [
    {"description": "Apply the prepared change", "command": "..."},
    {"description": "Check the change", "command": "...", "after": [0], "verify": true}
  ],
  "answer": null
}
```

`after` refers to earlier zero-based action indexes. Only successful predecessors
unlock an action. Ordinary actions execute at most once per agenda. A failed action
blocks its dependents and invalidates the proposed answer. Replanning replaces
the agenda; the generation prompt requires unresolved checks to be retained.
The controller cannot finish while any declared verification action is pending
or has failed. It also evaluates whether the proposed answer is supported by the
observations. These mechanisms do not prove that the proposed checks are complete.

A reusable procedure adds `repeat: true` and optional positional parameters:

```json
{
  "description": "Read a relevant file whose contents are still needed",
  "command": "cat -- \"$1\"",
  "repeat": true,
  "parameters": [{"description": "The file to read", "values": ["src/a.py", "src/b.py"]}]
}
```

The runtime asks branch-specific argument questions alongside the next-action
question in one request, then consumes only the selected action's answers. Each
parameter selects an existing string; the runtime binds it to `$1`, `$2`, etc.
with shell quoting, never text substitution into code. Procedures must quote
parameter expansions and must not `eval` them. Parameters are independent: use
a single compound candidate when values must remain coupled. There are at most
4 parameters per action, 31 total per agenda, and 31 values per parameter.
An unavailable or malformed selected argument executes nothing and stops.

A successful repeatable procedure remains available with fresh arguments;
a failed one requires replanning. `repeat` cannot be combined with `verify`.
Execution history records the actual bound command, so the controller can avoid
repeating work. A new generation or discovery replaces the agenda, including
procedures: retain any procedures that are still useful in the replacement.

No task-specific action catalog is required. Start with an empty agenda; a
generation can prepare a discovery command that enumerates the actual environment
and emits reusable procedures with observed candidate values. That permits many
observations between generations. New code, free-form arguments not present among
candidates, or an unsupported capability can still require generation.

A successful `discover: true` action must print a JSON actions array. It replaces
the agenda and clears any proposed answer. This lets scripts enumerate files,
links or tests into concrete actions without a generation per item. Ordinary
shell output is never interpreted as an action agenda. `discover` and `verify`
cannot be combined. The same schema may be provided in `skills/actions.json`
for initial actions before the first generation. The example here is optional
and repository-specific; it requires git and Python 3.

Skills remain Markdown instructions with optional shell scripts and this one
initial action file. There is no tool registry or new execution tool. Actions
are arbitrary shell commands with the existing process privileges.

## Context and compaction

Every conversation message is flushed to a private `<session>.events.jsonl`
archive before selection. The active transcript can then discard complete older
assistant/tool-result units chosen by the decision model. System/user messages
and the recent tail remain pinned; invalid selections preserve all history.
The current agenda, dependency results and proposed answer live outside the
transcript, so pruning a proposal message cannot erase unfinished work.

Decision calls receive explicit excerpts of large observations; generation
receives the active full messages. Both receive the archive path. Full generated plans are archived immediately;
the active transcript keeps a command-free plan summary, because executed commands
are already recorded in tool calls. The generation contract forms a stable prefix. A generated
shell read can recover omitted evidence. Selection starts at
`decision_context_bytes` (4,096–24,000; default 18,000) and is a synchronous,
bounded decision request. It is not the old asynchronous text-summary seal.
`compact_extra` applies to the original generative loop, not this selection mode.

This first implementation compacts by selection; it does not automatically
rewrite evidence into a generated summary. It does not preserve prompt-cache
prefix identity when selecting out older units. If pinned instructions alone
exceed the decision API budget, the run stops with history intact. There is no
claim of unlimited context, guaranteed speedup, or measured model quality.

## Validation

`make test test-integration` covers the real executable and shell against local
HTTP fixtures: multiple actions between generations, discovery, dependencies,
failed-check recovery, invalid decisions/plans/arguments, archive retention,
parameterized discovery and reuse, literal shell binding, unified generation
policies, policy/flow forwarding and credential scrubbing. It does not spend provider credits.
Compare verified task completion, total inference cost, latency and generation
count on representative real tasks before replacing an existing deployment.
