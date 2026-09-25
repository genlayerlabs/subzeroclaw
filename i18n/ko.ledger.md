# Korean (ko) concept ledger: SubZeroClaw landing page

**Register.** Korean developer-marketing prose in 합니다체, matching the GenLayer Labs homepage (`~/dev/genlayerlabscom-site/i18n/ko.*`). Instructions use the polite imperative (~하세요). Headline, links and chips are noun phrases or ~하기/~기 forms.

**Authorities.** `README.md` (read in full; "What it does", "Routing & compaction via unhardcoded", "Tool", "Skills", "Session logging", "Config reference", "Philosophy"), `CONTRIBUTING.md`, `site/llms.txt`, and `examples/decision-models/README.md` (the controller). For shared terms I followed the genlayerlabs.com ko ledger and json (에이전트 런타임, 셸 하나, 루프를 돌리다, 라우터, 모델, 맡기다) and the unhardcoded glossary ko column (라우팅, 정책, 모델, 문서, MIT 라이선스, LLM kept in English).

## Objects

| Concept | Source term(s) | Meaning here (source) | Keep distinct from | ko | Avoid | Source |
|---|---|---|---|---|---|---|
| Agent | agent | One loop running one skill (README "one agent, one skill, one device") | — | 에이전트 | 에이전트 봇 | genlayerlabs ko |
| Agent runtime | agent runtime / agentic runtime | The C program that runs the loop | framework | 에이전트 런타임 (both source variants) | 에이전틱 런타임, 실행 환경 | genlayerlabs d3bea27b "에이전트 런타임" |
| Skill | skill, skill file | A plain markdown file loaded into the system prompt | plugin, tool | 스킬, 스킬 파일 | 기술, 능력 (reads as "ability") | README "Skills" |
| skill.md | skill.md | The formula token in the h2 (not translated, `translate="no"`) | — | skill.md | — | index.html h2 |
| Markdown file | markdown file | Where you describe the job | — | 마크다운 파일 | — | README "What it does" |
| The loop | the loop | Read skill → call model → run shell → repeat | — | 루프 | 반복문 (sounds like a code construct) | README "Philosophy" |
| Job / task | the job | What the user writes in the skill | — | 할 일, 일 | 잡, 태스크 | — |
| Shell | shell | The only tool (`popen()`) | — | 셸 | 쉘 (non-standard spelling) | README "Tool"; genlayerlabs "셸 하나" |
| Shell command | shell command, command | A concrete CLI command the model asks for | model call (호출) | 셸 명령어, 명령어 | 명령 alone (reads as "order") | — |
| Tool | tool | "The shell is the only tool" | integration | 도구 | 툴 | genlayerlabs "도구는 셸 하나뿐" |
| Integration | integration | An installed CLI the agent can use without an adapter | 통합 (reads as "merge") | 연동 | 통합, 인티그레이션 | README "The adapter is the shell" |
| Model / LLM | model, LLM | The language model the router picks | — | 모델; LLM kept in English | 언어모델 in short copy | glossary B, D |
| Model call | call | One request to the model | command | 호출 | 콜 | — |
| System prompt | system prompt | Where the skill is placed | confirmation prompt | 시스템 프롬프트 | — | README "What it does" |
| Router | router | unhardcoded, the substrate that handles model choice, cache and compaction | runtime | 라우터 ("unhardcoded 라우터") | 라우팅 계층 (not needed here) | glossary A; genlayerlabs 77eb1db6 |
| Model choice | model choice | Router picks (provider, model) per call | — | 모델 선택 | — | README "Routing & compaction" |
| Prompt caching | prompt caching | Cache-affinity routing | — | 프롬프트 캐싱 | 프롬프트 캐시 저장 | llms.txt Facts |
| Context compaction | context compaction | Old turns sealed asynchronously in the background | "summarize" | 컨텍스트 압축 | 문맥 압축, 컨텍스트 요약 | README "Compaction" |
| Summarize | never pauses to summarize | The runtime never stops in-process to summarize | compaction | 요약하느라 멈추는 일도 없습니다 | 요약하지 않습니다 (false: the router's seal is a summary) | llms.txt para 2 |
| Sandbox | sandbox | Absent by design | — | 샌드박스 | 격리 환경 | README warning |
| Confirmation prompt | confirmation prompts | Asking the user before running a command | system prompt | 실행 전 확인 | 확인 프롬프트 (clashes with the LLM prompt) | README warning |
| Allowlist | allowlist | Absent by design | — | 허용 목록 | 화이트리스트 | — |
| Decision controller | optional decision controller | Opt-in mode (`decision_extra`) that picks prepared shell actions and their arguments | the default loop | 선택 기능인 결정 컨트롤러 | 의사결정 제어기 | examples/decision-models/README.md |
| Config reference | config reference | The README table of config keys | — | 설정 레퍼런스 | 구성 참조 | README "Config reference" |
| Quickstart | Quickstart (aria-label) | The code block | — | 빠른 시작 | 퀵스타트 | — |
| Framework | framework | What the project refuses to be | — | 프레임워크 | — | README "Philosophy" |
| Deps | deps | curl and unhardcoded (cJSON is vendored) | — | 의존성 2개 | 디펜던시 | llms.txt Facts |
| Swarm | agent swarms | Many agents run at scale | — | (에이전트) 스웜 | 군집 | llms.txt |
| Edge devices | edge devices | Small hardware (e.g. Raspberry Pi) | — | 엣지 디바이스; 라즈베리 파이 | 엣지 장치 | llms.txt |

## Texture words (step 2b)

| Concept | Occurrences | ko choice | Rationale |
|---|---|---|---|
| run anywhere | h1, title, og/twitter titles and alts, meta descriptions | 어디서든 돌아갈 (만큼 작은) | 돌아가다 is how Korean developers say software "runs". It is casual like "run anywhere" and matches genlayerlabs "어디서든 수천 개씩 돌릴 수 있을 만큼 작습니다". The h1 is `<em>어디서든 돌아갈</em> 만큼 작은 에이전트`. The em moves to the front because Korean puts the "~할 만큼" clause before the adjective. Every 어절 is ≤4 syllables. |
| small / minimal | h1 "small"; meta "minimal" | 작은 / 미니멀한 | 작다 for the plain claim. 미니멀한 for "minimal runtime", the dev loanword. I avoided 초경량 because it strengthens the claim. |
| just the loop | meta ×4 | 프레임워크는 없고 루프만 있습니다 | A contrastive 는 plus 만 carries "no X, just Y" without a fragment. |
| stays small / never pauses | 781a29e4 | 루프는 작게 유지되고, 요약하느라 멈추는 일도 없습니다 | ~느라 gives the "pause in order to" cause. It does not claim that nothing is ever summarized. |
| nothing stands between | h3 | 모델과 셸 사이를 가로막는 것은 없습니다 | 가로막다 = "stand in the way". Plain and unsoftened. |
| whatever… runs, rm -rf / included | warning p | 무엇이든 그대로 실행됩니다. `rm -rf /`도 예외가 아닙니다. | 예외가 아니다 keeps the blunt emphasis. No hedging. |
| afford to lose | warning p | 통째로 날아가도 괜찮은 머신이나 컨테이너를 내주세요 | 날아가다 is the Korean dev idiom for a machine or data getting wiped. It fits `rm -rf /`. 내주다 keeps "give it". |
| 404 "ran somewhere we can't reach" | 404 | 이 경로는 우리 손이 닿지 않는 어딘가에서 돌아갔나 봅니다. | Keeps the "run" pun (돌아가다, same verb as the headline). ~나 봅니다 gives a dry, shrugging guess. |
| copy / copied ✓ | chip | 복사 / 복사됨 ✓ | Short enough for the chip. |

## Left in English

SubZeroClaw, GenLayer Labs, GitHub, README, MIT, unhardcoded (lowercase, and always followed by 라우터 so no particle attaches to it), skill.md, C, LLM, RAM, KB/MB, the `git clone` command in the aria-label, and all `<code>` content and paths.

## Numbers

Numbers use the Korean format with a comma thousands separator. ~ becomes 약 (약 1,400줄, RAM 약 2 MB). The og:image:alt and the stats line have no ~ on 1,400 in the source, so none was added. "C 코드 1,400줄", "90 KB", "의존성 2개". The © line is unchanged except for MIT 라이선스.

## Section semantic briefs

- **Hero.** A tiny agent runtime that runs anywhere. Clone it now.
- **How it works.** You write a markdown skill. The runtime loops model → shell until done. The shell gives you every installed CLI for free. Routing, caching and compaction live in the router, so the runtime stays small and never blocks.
- **Warning.** There are no guardrails at all. Use a disposable machine.

## Review

- [x] Accuracy audit (step 6): the router *handles* model choice, caching and compaction ("맡습니다"). The decision controller is marked optional. The warning is not softened. No claim is strengthened (no 초경량, no "모든 곳에서").
- [x] Cold-read audit (step 7): I rewrote 1 line. The JSON-LD "설계했습니다" became "설계되었습니다" because the page has no first-person subject.
- [x] Every tag, href and `<code>` is identical to the source (checked by script).
- [ ] Rendered layout (step 8): the coordinator checks this after the build.

## Review (independent reviewer)

Method: I cold-read every ko string in page order first. Then I compared each one with `en.json`, README ("What it does", "Decision-model control", "Routing & compaction via unhardcoded") and `site/llms.txt`. I checked the mechanics by script: all ids are present, the tag multiset and `<code>` contents match the source byte for byte, and the h1 has exactly one `<em>`. Shared terms (에이전트 런타임, 라우터, 에이전트, 모델, 셸, 루프, 돌리다) match `genlayerlabscom-site/i18n/ko.*` and the unhardcoded glossary. Facts check out: 약 1,400줄, 90 KB, RAM 약 2 MB, 의존성 2개. The warning keeps no sandbox, no confirmation, no allowlist, and the root-delete example included. "라우터가 맡습니다" matches README's "belong to the substrate". "요약하느라 멈추는 일도 없습니다" matches the async seal.

### Changes

- **e764c232.** Before: `…ffmpeg</code>)은 모두 따로 만들 필요 없는 연동이 됩니다.` After: `…ffmpeg</code>)은 연동을 따로 만들 필요 없이 모두 바로 쓸 수 있습니다.` Reason: "X은 … 연동이 됩니다" is a calque of "is an integration". The new sentence keeps the proposition (no integration to build, usable immediately; llms.txt "immediately available… no integrations to build") in natural Korean.
- **06779798.** Before: `…머신이나 컨테이너를 내주세요.` After: `…머신이나 컨테이너에서 돌리세요.` Reason: 내주다 has no stated recipient, so the safety instruction reads vague and a little literary. "에서 돌리세요" is unambiguous. It matches llms.txt/README "run it on a machine or container you can afford to lose" and echoes the page's 돌리다/돌아가다 "run" motif. The warning is not softened.
- **f6ff13e3** (orbit aria-label). Before: `…빨라지고 커지며 태양 뒤로 넘어갔다가…` After: `…빨라지고 커지며, 태양 뒤로 돌아갔다가…` Reason: "뒤로 넘어가다" commonly means "fall over backwards". "뒤로 돌아갔다가 다시 앞으로 나옵니다" clearly says it passes behind the sun.

### Verdicts on the open questions

- **돌아갈 in the h1.** Keep it. "어디서든 돌아갈 만큼 작은" is how a Korean developer would say it, and it is as casual as "run anywhere". 실행될 만큼 is stiff. 돌릴 수 있을 만큼 is fine but longer, and it adds an implied "you". The genlayerlabs page uses 돌릴 수 있을 만큼 in a full sentence, so the verbs are the same family and consistent.
- **통째로 날아가도 / 내주세요.** 통째로 날아가도 괜찮은: keep. 날아가다 is the standard dev idiom for wiped data or machines, and it fits the root-delete example. 내주세요: changed (see above).
- **미니멀한 에이전트 런타임.** Keep. 미니멀한 is an established loanword and does not overclaim. 초경량/경량 would strengthen the claim. 최소한의 is heavier.
- **결정 컨트롤러.** Acceptable. The feature has no established Korean name. 의사결정 컨트롤러 is longer and suggests business decision-making. "선택 기능인" correctly keeps "optional" (README: the original loop stays the default).
- **설정 레퍼런스.** Keep. 레퍼런스 is standard in Korean dev docs (API 레퍼런스). 구성 참조 reads machine-translated.

### Still for a native speaker

- **29cf5d8c (404).** "이 경로는 우리 손이 닿지 않는 어딘가에서 돌아갔나 봅니다." The "run" pun doesn't carry over cleanly. 경로가 돌아가다 can read as "the route went back/around". An alternative that drops the headline echo but reads clearly is "…어딘가로 달아났나 봅니다". I left it unchanged because the choice is about taste and tone.
