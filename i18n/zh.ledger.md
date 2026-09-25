# zh-Hans concept ledger: SubZeroClaw landing page

Sources read: README.md (all), CONTRIBUTING.md, site/llms.txt, examples/decision-models/README.md (config + controller
sections), site/index.html (rendered en page). Consistency sources: ~/dev/genlayerlabscom-site/i18n/zh.json + zh.ledger.md
(GenLayer Labs homepage, same product), ~/dev/genrouter/unhardcoded-landing/localization/glossary.md (zh-CN column).

Register: mainland developer marketing; plain, confident, short sentences; 你 (not 您), as in the Labs homepage.
Conventions (same as the Labs homepage): space between CJK and Latin/digits ("C 语言", "90 KB", "shell 命令");
full-width punctuation in Chinese sentences, 、 between listed items (incl. between `<code>` items);
numbers "1,400"; "~" rendered as 约 in prose ("约 2 MB"). Stats line keeps the source's U+00A0 between number and unit.

## Objects

| Concept | Source term | Meaning (source) | zh | Avoid | Source |
|---|---|---|---|---|---|
| agent | agent | the thing SubZeroClaw runs: skill + LLM + shell + loop | 智能体 | 代理 (proxy) | Labs zh 智能体 |
| agent runtime | (minimal) agent / agentic runtime | the C program itself | 智能体运行时；"minimal" = 极简 | 运行环境, 框架 | Labs zh "我们的智能体运行时" |
| skill / skill file | skill, skill.md, "a markdown file" | plain markdown file describing the job, read into the system prompt | 技能 / 技能文件；"a markdown file" = 一个 Markdown 文件 | 插件, 能力 | README "You write a skill as a markdown file" |
| the loop | the loop / shell loop | call model → run shell → repeat until done | 循环；"just the loop" = 只有循环 | 回路, 环路 | llms.txt; Labs zh 循环 |
| shell / shell command | shell | the only tool | shell；shell 命令 | 外壳, 终端 (≠ terminal) | Labs zh keeps shell |
| tool | tool | what the model can call; only the shell | 工具 | — | CONTRIBUTING "The shell is the only tool" |
| integration | integration | an adapter you'd otherwise write for git/curl etc. | 集成（现成的集成） | 集成化, 插件 | README Philosophy |
| model / LLM | model, LLM | the language model | 模型；LLM kept | 大模型 (register shift) | glossary D: LLM keep EN |
| system prompt | system prompt | where the skill is placed | 系统提示词 | 系统提示符 (shell prompt) | README "reads the skill into its system prompt" |
| router | unhardcoded router | substrate doing model choice, cache affinity, compaction | unhardcoded 路由器 | 路由层 here (Labs used it for the product category, this page says "router") | Labs zh 路由器; glossary A 路由器 |
| model choice | model choice | router picks provider/model per call | 模型选择 | — | README "Routing & compaction" |
| prompt caching | prompt caching | cache affinity: pin to the peer holding the prompt-cache prefix | 提示词缓存 | 快取 (TW) | README |
| context compaction | context compaction | router-signalled async seal of old turns | 上下文压缩 | 上下文压缩包 | README "Compaction is asynchronous" |
| "summarize" | never pauses to summarize | compaction runs in background; loop never stops for it | 从不停下来总结上下文 | 不需要总结 (false: summaries still happen) | README "it never pauses to compact" |
| sandbox | sandbox | none | 沙箱 | 沙盒 (TW-leaning) | README WARNING |
| confirmation prompt | confirmation prompts | none | 确认提示 | 确认对话框 | README WARNING |
| allowlist | allowlist | none | 白名单 | 允许列表 (reads as translated in mainland dev copy) | — |
| decision controller | optional decision controller | opt-in `decision_extra` mode choosing prepared shell actions | 可选的决策控制器 | 决策模型 (it is the controller, not a model) | decision-models/README; glossary "decision path" 决策路径 |
| config reference | config reference | README table of keys | 配置参考 | 配置文档 | README |
| quickstart | Quickstart (aria-label) | the code block | 快速开始 | 快速入门指南 (long) | README |
| framework | framework | what it refuses to be | 框架 | — | README Philosophy |
| deps | deps | curl shell + unhardcoded | 依赖；"2 deps" = 2 个依赖 | 依存 | llms.txt Facts |
| swarms | large-scale swarms | many agents at once | 大规模智能体集群 | 蜂群 | Labs zh 集群 (GenSwarms) |
| Raspberry Pi | Raspberry Pi | device | 树莓派 | — | standard |
| Docs / Language / copy | UI | nav link, picker name, chip | 文档 / 语言 / 复制 · 已复制 ✓ | — | Labs zh 文档, 语言; glossary E 文档 |
| MIT License | footer | — | MIT 许可证 | MIT 授權 (TW) | glossary A "MIT 许可" |

## Texture words (step 2b)

| Word | Decision | Rationale |
|---|---|---|
| small / run anywhere (thesis) | headline 一个智能体，小到<em>随处都能跑</em>; meta/descriptions 小到随处都能运行的智能体 | 小到… carries "small enough to". 跑 is the everyday dev verb (Labs native pass chose 一跑就是…) and makes the display line punchy; metadata keeps neutral 运行. `<em>` phrase sits at the end. |
| minimal | 极简 | "Minimal" as a design stance; 最小 would read as "smallest". |
| "No framework, just the loop." | 没有框架，只有循环。 | Two-beat parallel, same as source. |
| "stays small" (the loop) | 保持精简 | 精简 = lean code, fits "small" of code size; avoids repeating 小. |
| run (the 404 pun) | 这个路径跑到我们够不着的地方去了。 | Keeps the running pun (跑, echoing the headline) and the dry tone; 路径 = URL path. |
| warning | 模型与你的 shell 之间，没有任何阻隔 / 没有沙箱，没有确认提示，没有白名单。模型决定运行什么，就运行什么，rm -rf / 也不例外。给它一台毁掉也无妨的机器或容器。 | Unsoftened; imperative kept; "afford to lose" = 毁掉也无妨. |
| "an integration you don't have to write" | 都是现成的集成，无需自己编写 | 现成 carries "already there". |
| "lives in the router" | 都由 unhardcoded 路由器负责 | Natural ownership verb; no stronger claim. |

## Left in English

SubZeroClaw, GenLayer Labs, GitHub, README, MIT, unhardcoded (lowercase, router name), skill.md, C, LLM, shell,
Markdown (capitalised, as usual in zh), KB/MB units (RAM is rendered 内存 / 内存占用 everywhere),
`git clone`, all `<code>` content, file paths, "© 2026".

## For native review

- Headline register: 随处都能跑 (casual, punchy) vs 随处都能运行 (neutral, matches metadata).
- 白名单 vs 允许列表 for "allowlist".
- 毁掉也无妨 vs 丢了也不心疼 for "you can afford to lose".

## Audit

- [x] Accuracy: compaction stated as the router's job; "never pauses to summarize" kept as 从不停下来总结上下文 (not "no summarizing"); warning not softened; facts exact (~1,400 → 约 1,400; 1,400 kept without 约 where source has none; 90 KB; 约 2 MB; 2 个依赖).
- [x] Tags, attributes, hrefs and `<code>` content identical to source (checked by script).
- [x] Terms match the Labs homepage (智能体, 智能体运行时, 循环, shell, 路由器, 模型, 集群, 文档, 语言) and the router glossary zh-CN (路由, 模型, MIT 许可, 文档).
- [x] Cold read: no 的-chains, no 当…时 calques; full-width punctuation throughout.
- [ ] Rendered page (desktop/mobile): coordinator.

## Review

Independent reviewer pass (steps 6–7): cold read of all zh strings in page order, then accuracy against en.json, README
("WARNING", "Routing & compaction via unhardcoded") and llms.txt; tags/`<code>`/hrefs, CJK–Latin spacing and
full-width punctuation checked by script (all clean; U+00A0 in the stats line preserved; headline has one `<em>`).

Changes:

- f6ff13e3 (orbit aria-label): 离得远时放慢、缩小，随后加速、变大 → 离得远时减速、变小，随后加速、变大.
  放慢 is transitive in normal use (放慢速度/脚步) and reads truncated on its own; 减速/加速 and 变小/变大 give two clean
  mirrored pairs, matching the source's slows/accelerates, shrinks/grows.

No other changes. Accuracy: facts exact (约 1,400 / 90 KB / 约 2 MB / 2 个依赖; 1,400 without 约 where the source has no ~);
warning unsoftened (no sandbox, no confirmation, no allowlist, `rm -rf /` included, imperative kept); compaction and
model choice attributed to the router, and 从不停下来总结上下文 says the loop never stops for it, not that no
summarizing happens, which matches README. Terms match the Labs zh page (智能体, 运行时, 路由器, 循环, 模型, shell).

"shell": keep. Mainland developer copy says shell / shell 命令 / shell 脚本 routinely; 终端 is the terminal (a different
object) and 命令行 is looser and would weaken "the shell is the only tool". Reads naturally in all four places
(a1bbbaff, e764c232, 172cf448, 03765c8f).

Verdicts on open questions:

- Headline: keep 随处都能跑 in the h1. 跑 is the normal dev verb (跑起来, 能跑) and the Labs zh page already uses 一跑就是;
  the metadata keeps neutral 运行 for search/snippets, which is the right split.
- Allowlist: keep 白名单. It is what mainland developers and docs say; 允许列表 reads as localized-from-English.
- "afford to lose": keep 毁掉也无妨. The risk is the machine being wiped/destroyed by the model; 丢了也不心疼 implies
  misplacing it and is more colloquial than the rest of the warning.

## 2026-09-25: meta description (80-char limit) and language suggestion bar

- 5da16d26 (meta description, replaces 2989fe33, now deleted): `SubZeroClaw 是 C 语言编写的极简智能体运行时：约 1,400 行代码，二进制 90 KB，内存约 2 MB。没有框架，只有循环。小到随处都能运行。` (80 chars).
  Reuses the approved old description and headline wording (极简智能体运行时, 没有框架，只有循环, 小到随处都能运行 with neutral 运行 for metadata).
  Trimmed to fit: 是用…编写 → 是…编写, 二进制文件 → 二进制, 内存占用约 → 内存约; the old list (边缘设备…集群) is gone because the new English drops it.
  Facts exact: 约 1,400 / 90 KB / 约 2 MB.
- Suggestion bar (shown on other locales to zh-Hans browsers, so it names 简体中文, not English):
  - 4bee1489: `本页也有简体中文版。` (10). 本页 is the short, standard web form; 也有…版 = "is also available in".
  - 0991c336: `阅读简体中文版` (7). Verb + object link label; 阅读 matches "Read it".
  - 70afe9ef: `关闭` (2). Standard label for a close (×) button; 忽略 would read as "ignore".

### 2026-09-25: editor second pass (meta description, suggestion bar)

Cold read, then against en.json. Changes:

- 5da16d26: `SubZeroClaw 是 C 语言编写的极简智能体运行时：约 1,400 行代码，…` → `SubZeroClaw：用 C 语言编写的极简智能体运行时。约 1,400 行代码，…` (rest unchanged; 80 → 80 chars).
  Reason: 是 C 语言编写的 (dropping 用 to save a character) reads clipped; the colon form restores 用 at the same length,
  reuses the approved hero wording 用 C 语言编写的极简智能体运行时 verbatim, follows the title's `SubZeroClaw：…` pattern,
  and gives the stats their own sentence. Meaning unchanged ("SubZeroClaw is a minimal agent runtime in C"); facts exact.
- 4bee1489 `本页也有简体中文版。`, 0991c336 `阅读简体中文版`, 70afe9ef `关闭`: no change. Natural and short; the link keeps the
  language name so it is meaningful out of context (accessibility). Width at 390px: bar is 14px, 10 + 7 CJK chars ≈ 238px
  of ~298px available beside the 44px close button, so one line.

Validation: `validate('zh', …)` returns []; 2989fe33 absent from zh.json.
