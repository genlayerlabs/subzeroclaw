# Decision-model controller

The runtime calls `/v1/decisions` with observed state and a finite action set.
A chosen action executes through the existing shell. Generation runs only when
the controller selects `generate`. The decision model never emits shell code.

This is an opt-in mode: set `decision_extra` to a JSON object. Without it the
existing generative loop and asynchronous `/v1/compact` behavior stay available.
No vendor or model name is compiled into the controller.

## Configure

Use a router consumer key and its `/v1/chat/completions` endpoint as usual.
`decision_extra` configures decision inference; `request_extra` configures
on-demand generation and accepts a `policy_ir` or `flow_ir`.

For router-owned economical/capable generation selection, first deploy a router
with decision routing in flow nodes. Generate config using the **exact model
families available in your router catalog**:

```sh
python3 examples/decision-models/configure.py \
  --economy-family YOUR_LUNA_FAMILY \
  --capable-family YOUR_ASTRA_FAMILY
```

Copy its three lines into `~/.subzeroclaw/config`. Policies choose healthy
compatible offers, rank input price, and allow provider fallback. The decision
endpoint constrains the protocol, so accepting multiple decision-model families
cannot route a decision to a chat model. You can add `--decision-family` to pin
an evaluated family, or replace the generated policy with your own constraints.

The flow's routing decision sees bounded history, commands, tool results and the
current generation request. It executes one declared generation policy; a
malformed/failed/uncertain decision uses the declared capable fallback. This is
an inference policy, not a claim that an economical model is sufficient for any
particular task. Provider restrictions belong in every applicable policy.

A plain generation policy works without the flow extension. Example minimum
configuration for decision inference (routing defaults are supplied by the router):

```ini
decision_extra = {}
decision_context_bytes = 18000
```

The equivalent environment variable is `SUBZEROCLAW_DECISION_EXTRA`. It is
scrubbed before shell execution, like the existing inference configuration.
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
unlock an action. Each action executes at most once per agenda. A failed action
blocks its dependents and invalidates the proposed answer. Replanning replaces
the agenda; the generation prompt requires unresolved checks to be retained.
The controller cannot finish while any declared verification action is pending
or has failed. It also evaluates whether the proposed answer is supported by the
observations. These mechanisms do not prove that the proposed checks are complete.

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
receives the active full messages. Both receive the archive path. A generated
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
failed-check recovery, invalid decisions/plans, archive retention, policy/flow
forwarding and credential scrubbing. It does not spend provider credits.
Compare verified task completion, total inference cost, latency and generation
count on representative real tasks before replacing an existing deployment.
