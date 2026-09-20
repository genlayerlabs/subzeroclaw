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
inference can incur additional provider cost, so meter all HTTP attempts. After
exhausted transient retries or an invalid decision, the controller can request
up to two consecutive generative repairs. Permanent 4xx errors (except 408/429)
stop. A successful shell action resets this repair allowance; `max_turns` remains
the overall bound.

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
An invalid decision executes nothing and enters bounded repair. There is no second model
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
limit are rejected before decision inference. Admission includes JSON ASCII
escaping, pinned instructions, retained procedures and argument branches behind
dependencies. A rejected generation/discovery leaves the old catalog intact and
requests a smaller plan. Oversized initial instructions stop before any inference.

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
The controller cannot finish while any ordinary action is pending or any action
has failed. Further non-verification shell work invalidates previous successful
checks, so they must run again against the resulting state. It also evaluates whether the proposed answer is supported by the
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
An unavailable or malformed selected argument executes nothing, disables that
action and requests fresh candidates through bounded generative repair.

A successful repeatable procedure remains available with fresh arguments;
a failed one requires replanning. `repeat` cannot be combined with `verify`.
Execution history records the actual bound command, so the controller can avoid
repeating work. Three consecutive identical commands with identical captured
results disable the stalled action and request repair. Named procedures survive
agenda replacement as described below; unnamed procedures do not.

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

A single `generate` option runs directly without asking the decision model to
choose it. Choosing between generation policies, executing a prepared action and
finishing still requires a decision. Decision history contains execution results
and literal bound arguments; it does not repeat generated programs or plan
summaries that are already represented by the current agenda.

Reusable actions can set `procedure: "name"`, `repeat: true` and no `after`
dependencies to survive both replanning and discovery during the current run.
New actions with the same name replace the definition and candidate values;
top-level `forget: ["name"]` in a generated plan explicitly removes one. The
merged agenda must fit the existing 24-action/31-parameter limits. A failed
retained procedure stays disabled until replaced or forgotten. These are shell
procedures generated from the task and observations, not a built-in tool catalog.

Every conversation message is flushed to a private `<session>.events.jsonl`
archive before selection. A separate decision view can then discard complete older
assistant/tool-result units chosen by the decision model. System/user messages
and the recent tail remain pinned; invalid selections preserve all history.
The current agenda, dependency results and proposed answer live outside the
transcript, so pruning a proposal message cannot erase unfinished work.

Decision calls receive explicit excerpts of large observations. The generator
retains its exact prompts and text responses, appending execution observations with
the action description, bound arguments and at most about 6 KB of result (head
and tail, explicitly marked when shortened). The decision view uses about 1.5 KB
of each result, preserving structured arguments and evidence IDs. Commands occur once in
their generated plan; the archive records every full command actually executed.
The decision view omits duplicate plan summaries. Both receive the archive path
for recovering evidence. Selection starts when the **decision view** exceeds
`decision_context_bytes` (4,096–24,000; default 18,000) and is a synchronous,
bounded decision request. It is not the old asynchronous text-summary seal.
`compact_extra` also applies to the generator in decision mode: a router context
pressure signal can request a separate asynchronous seal of its transcript.

When `decision_extra` supplies `policy_ir`, the seal also sends that policy as
`decision_policy_ir` plus the indexes of genuine user inputs to pin. A router
supporting fragment compaction classifies complete aged units as keep, summarize
or archive, batches only the selected fragments into the summarizer policy, and
reassembles them in order. System/developer instructions, genuine user input and
the recent tail remain verbatim; tool-call/result groups stay together.
`compact_extra.target_ratio` defaults to 0.1 of serialized UTF-8 bytes, not model
tokens. Oversized or failed summaries retain the original unit. If protected or
kept evidence prevents 10%, the response reports `target_met:false` and the actual
sizes; those metrics and inference costs appear in the session log. Originals
remain in the local archive. An older router ignores these additive fields and
uses its existing seal; the fragment feature requires the router update.

Decision selection does not rewrite the generator's history or cache prefix.
Only a separate generative seal can replace that history. Prefix preservation
permits provider caching but does not guarantee a cache hit. If pinned instructions alone
exceed the decision API budget, the run stops with history intact. There is no
claim of unlimited context, guaranteed speedup, or measured model quality.

## Validation

`make test test-integration` covers the real executable and shell against local
HTTP fixtures: multiple actions between generations, discovery, dependencies,
failed-check recovery, invalid decisions/plans/arguments, archive retention,
parameterized discovery and reuse, literal shell binding, unified generation
policies, retained procedures across replanning/discovery, atomic replacement and
invalidation, rejected-response audit retention, policy/flow forwarding and
credential scrubbing. It does not spend provider credits.
Compare verified task completion, total inference cost, latency and generation
count on representative real tasks before replacing an existing deployment.
