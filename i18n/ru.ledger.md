# Concept ledger: SubZeroClaw landing page, ru

Register: **вы** (plural imperative: «Опишите задачу…», «Отдайте агенту…»), as on the GenLayer Labs homepage («Листайте вниз…»). Plain developer prose with no marketing inflation. Typography: « », em dash with spaces, U+00A0 between a number and its unit.

Authorities read: `README.md` (in full: warning, quickstart, "Routing & compaction via unhardcoded", config reference, tool, skills, philosophy), `CONTRIBUTING.md`, `site/llms.txt` (Facts section is the source for 1400 lines / 90 KB / ~2 MB / 2 deps), `examples/decision-models/README.md` (skimmed), the rendered `site/index.html`, and for shared vocabulary `~/dev/genlayerlabscom-site/i18n/ru.ledger.md` + `ru.json`.

## Objects

| Concept / object | Source term | Layer | Meaning in context | Must stay distinct from | RU | Short form | Avoid | Source |
|---|---|---|---|---|---|---|---|---|
| The product | agent runtime / agentic runtime | README intro, llms.txt | The C process that runs skill + LLM + shell in a loop | the agent it runs; the router | среда выполнения агентов | среда (fem.: «Рассчитана…») | «рантайм» (the Labs ledger rejected it), «движок» | README "A small C runtime for skill-driven agents"; Labs ledger row "Runtime" |
| Agent | agent | page, README | What you get from skill.md + LLM + shell + loop | the runtime | агент | агент | «бот» | README "= autonomous agent" |
| Skill | skill (the markdown file) | README "Skills" | A `.md` file that becomes part of the system prompt | tool | навык; «файл навыка» in the JSON-LD | навык | «скилл» (slang) | README "Drop a `.md` file… It becomes part of the system prompt" |
| skill.md | skill.md | h2 formula (translate="no") | File name | — | skill.md, unchanged | — | — | — |
| The job | job / task | page p1 | What the skill file describes and the loop runs until done | "brief" (Labs page «задание») | задача | задача | «работа» (vague), «задание» (Labs used it for the café's brief, a different object) | README "loops until done" |
| The loop | loop / agentic loop | whole corpus | call model → run command → repeat | — | цикл; «и так по кругу» in the how-it-works sentence | цикл | «петля», «луп» | README "call an LLM, execute tools, loop" |
| Shell | shell | README "Tool" | The one tool: `popen()` any command | "tool" in general | командная оболочка (first use); оболочка | оболочка | «шелл», «терминал» (the Labs page also says «командная оболочка») | README "One tool: shell" |
| Shell command | shell command | page p1 | A command the model asks the runtime to run | — | команда оболочки | команда | «shell-команда» | — |
| Tool | tool | README "Tool" | Something the model can call; here only the shell | integration | инструмент | инструмент | — | README "The shell is the only tool" |
| Integration | integration | page p2 | Code that connects an agent to an external program | — | интеграция («которую не нужно писать самому») | — | «адаптер» (the README's word for the thing that isn't needed, but the page says integration) | README "No adapters, no integrations. The adapter is the shell." |
| Model / LLM | model, LLM | page, meta | The LLM the runtime calls | router | модель; LLM in the formula and JSON-LD («LLM-роутер») | модель | «нейросеть», «ИИ» | — |
| System prompt | system prompt | page p1 | Where the skill file goes | — | системный промпт | — | «системная подсказка» (a calque nobody uses) | README "reads the skill into its system prompt" |
| Router | router | README "Routing & compaction" | unhardcoded: picks provider and model for each call, keeps cache affinity, signals compaction | the runtime | роутер unhardcoded / LLM-роутер unhardcoded | роутер | «маршрутизатор» (network hardware; the Labs ledger says the same) | README "The router picks the (provider, model) per call" |
| Model choice | model choice | page p3 | The router picks the model for each call | — | выбор модели | — | — | same |
| Prompt caching | prompt caching / prompt-cache affinity | README | Reusing the provider's cached prompt prefix | — | кэширование промптов | — | «кеширование подсказок» | README "keeps the conversation pinned to the peer that already holds its prompt-cache prefix" |
| Context compaction | context compaction | README | Old turns are sealed into a summary block asynchronously | "summarize" (the act the loop never pauses for) | сжатие контекста | сжатие | «компакция», «уплотнение» | README "Compaction is asynchronous" |
| "summarize" | never pauses to summarize | page p3, llms.txt | The runtime doesn't stop the loop to summarize history; compaction runs in the background | — | «никогда не прерывается на суммаризацию» | — | «пересказ», «подведение итогов» (don't mean LLM summarization of history) | llms.txt "the runtime does not summarize in-process… so the loop never pauses" |
| Sandbox | sandbox | warning | Isolation around executed commands (there is none) | — | песочница | — | — | README warning "no sandboxing" |
| Confirmation prompt | confirmation prompts | warning | Asking the user before running a command (there are none) | "prompt" as in system prompt | запрос на подтверждение | — | «промпт подтверждения» (collides with the LLM sense) | README warning "no confirmation prompts" |
| Allowlist | allowlist | warning | A list of permitted commands (there is none) | — | белый список | — | «список разрешений» (sounds like OS permissions) | — |
| Decision controller | optional decision controller | README "Decision-model control", decisions README | Opt-in mode (`decision_extra`): a controller picks prepared shell actions and their parameters via `/v1/decisions` and asks for generation only when needed | the router's model choice | необязательный контроллер решений | — | «модуль принятия решений» (sounds like a general AI claim), «опциональный» (anglicism) | decisions README "The runtime calls `/v1/decisions`… A chosen action executes through the existing shell." |
| Config reference | config reference | README table | The table of config keys | — | справочник по настройкам | — | «ссылка на конфиг» (false friend "reference") | README "## Config reference" |
| Quickstart | quickstart | aria-label of the code block | The shell snippet | — | Быстрый старт | — | — | — |
| Framework | framework | meta, philosophy | What SubZeroClaw deliberately isn't | — | фреймворк | — | «каркас» (rare outside academia) | README "no framework" |
| deps | deps | stats line | Dependencies (curl, unhardcoded) | — | зависимости | — | «деп.», «зав.» | llms.txt "Dependencies: a shell with curl, and unhardcoded" |
| Units | 90 KB, ~2 MB RAM | stats, meta | Binary size, memory at runtime | — | 90 КБ, ~2 МБ ОЗУ (U+00A0); 1400 with no space (Russian groups digits with a space only from five digits) | — | KB/MB/RAM in Latin, «1,400» | Labs ledger «2–4 МБ» |
| Edge devices | edge devices | meta, JSON-LD | Small hardware at the network edge (Pi etc.) | computer peripherals | edge-устройства | — | «периферийные устройства» (a Russian reader sees printers and keyboards) | llms.txt |
| Swarm | swarm | meta | Many agents running at once | — | рой агентов | рой | — | Labs ledger «рой» |

## Texture words (step 2b)

| Concept | Choice | Why |
|---|---|---|
| "small" (the thesis word) | headline **«так мал»**; loop **«небольшим»**; "minimal" = **«минималистичная»** | The short adjective «мал» is the shortest native form and carries the "small enough that…" construction («так мал, что…»). «Маленький» reads as childish about code, so the loop "stays small" becomes «остаётся небольшим». "Minimal runtime" matches CONTRIBUTING's "stay minimal". |
| "run anywhere" | **«запустится где угодно»** (headline, in `<em>`, at the end); **«Запускается где угодно»** (meta) | The perfective future is the natural "will run / can run" promise and matches the Labs page's «запускается где угодно». «Работать где угодно» is ambiguous with "work" (a job). |
| "No framework, just the loop." | **«Никакого фреймворка — только цикл.»** | A native emphatic negation, and the dash gives it the same two-beat rhythm. |
| "Nothing stands between the model and your shell" | **«Между моделью и вашей оболочкой ничего нет»** | Flat and literal on purpose. No softening, and nothing like «рекомендуем». |
| "Whatever the model decides to run, runs" | **«Всё, что модель решит запустить, запустится, и `rm -rf /` тоже.»** | Keeps the English echo (run → runs = запустить → запустится). «И … тоже» is the native "included". |
| "Give it a machine you can afford to lose" | **«Отдайте агенту машину или контейнер, которые не жалко потерять.»** | «Не жалко потерять» is the idiom. «Агенту» fixes the antecedent: in Russian a bare «ей/ему» would point to «модель». |
| "an integration you don't have to write" | **«становится интеграцией, которую не нужно писать самому»** | «Становится» is more natural than a copula with «это». |
| 404 "ran somewhere we can't reach" | **«этот путь убежал туда, где нам его не достать.»** | «Убежал» keeps the run pun. It's lowercase and dry. |
| "copy" / "copied ✓" | **«копировать» / «скопировано ✓»** | Standard RU UI verbs. They're longer than the English, but the chip is `flex:none` and the command next to it truncates with an ellipsis, so the layout holds. A shorter fallback is «готово ✓». |

## Left in English / unchanged

SubZeroClaw, GenLayer Labs, GitHub, README, MIT, unhardcoded (lowercase, never inflected: always after a noun, «роутер unhardcoded»), skill.md, C, LLM (in «LLM-роутер» and the JSON-LD), Raspberry Pi, markdown («markdown-файл»), git clone, all `<code>` content and paths, "edge" (as «edge-устройства»).

## Audit notes

- Accuracy (step 6): the facts match llms.txt (1400 lines, 90 KB, ~2 MB RAM, 2 deps; "~" is kept exactly where the source has it). The warning keeps all three absences plus `rm -rf /`. The router sentence says the router "takes on" model choice, caching and compaction («берёт на себя»), with no stronger claim. "Never pauses to summarize" is kept literally, which is right because compaction runs asynchronously (README "Routing & compaction"). In the JSON-LD, "One skill file" keeps «Один».
- Naturalness (step 7): read cold. The repeated «модель» in the how-it-works sentence is intentional: replacing the second one with «она» would be ambiguous with «оболочка».
- For native review: «контроллер решений» (the literal term; a descriptive option is «контроллер, выбирающий действия»); «edge-устройства» vs «периферийные устройства»; «суммаризация» (jargon, but it's what Russian LLM developers say).

## Review

Independent review against steps 6 (accuracy) and 7 (naturalness). I read the Russian cold first, then compared it with `en.json`, `README.md` ("Routing & compaction via unhardcoded", "Decision-model control", warning) and `site/llms.txt` Facts. The mechanics pass, checked by script: every tag, attribute and href is byte-identical, `<code>` contents are unchanged, the h1 has exactly one `<em>` around «запустится где угодно», the `# ` comment keeps its prefix and path, and every number–unit pair uses U+00A0. The facts hold (~1400 / 90 КБ / ~2 МБ / 2 зависимости, with the tilde exactly where the source has one). The warning isn't softened: all three absences are there, and so is `rm -rf /`. The router sentence matches the README: the router takes on model choice, cache affinity and compaction, and compaction is asynchronous, so the loop "never pauses". The shared terms match the Labs page (среда выполнения агентов, командная оболочка, роутер, цикл, рой).

### Changes

- **2989fe33**, **03765c8f**: «Запускается где угодно: …» → «Такая компактная среда запускается где угодно: …». The English says "*Small enough to* run anywhere". The draft dropped the causal link to size, and size is the page's thesis. The new wording restores it and reuses the Labs page phrasing («Такая компактная среда запускается где угодно»). It also gives the sentence an explicit subject; before, it hung off the previous sentence.

No other changes. Everything else reads as native developer prose.

### Verdicts on open questions

- **«необязательный контроллер решений»**: keep. It's the README's own name for the feature (`decision_extra`), and this line is a pointer to the README, where the reader will meet the term again. A descriptive gloss («контроллер, выбирающий заготовленные действия оболочки») would double a one-line footnote for no gain. «Необязательный» is correct and avoids the anglicism «опциональный».
- **«edge-устройства»**: keep. It's the standard word in Russian developer writing (Habr, vendor docs). «Периферийные устройства» reads as printers and keyboards. «Граничные устройства» exists (after «граничные вычисления») but is rarer and more academic.
- **«суммаризация»**: keep. It's the established Russian NLP term («задача суммаризации»), and here it names exactly the in-process LLM summarization the runtime doesn't do. «Пересказ» / «подведение итогов» would be wrong.
- **«так мал» vs «настолько мал»**: keep «так мал». «Так …, что …» is the idiomatic "small enough that…" and is 6 letters shorter. In a lowercased 2.4rem display headline on a 390px screen (342px column), those extra letters could push the headline onto a fifth line. The longest word, «запустится», fits either way.
- **Copy chip**: keep «копировать» / «скопировано ✓». «Копировать» is the shortest natural button verb. «Копия» is a noun ("a duplicate"), so as a button it misreads. Width at 390px: the label is uppercase JetBrains Mono at .7rem (≈5.9px per character with tracking) plus 24px of padding, so the chip takes ≈83px idle and ≈101px copied (EN: 48px / 71px). That leaves room for ≈24 and ≈22 characters of the command, so "git clone github.com/…" stays readable even at 360px (≈20 / ≈18 characters). The copied state lasts only 1.6s, and «скопировано ✓» tells both the eye and the aria-live announcement what happened, which «готово ✓» doesn't. «Готово ✓» remains the fallback if a real-device check shows the done state clipping badly.

### For a native speaker

- The alt text (f6ff13e3) «на эксцентрической орбите … уходит за него и снова выходит вперёд» is accurate but heavy. A plainer «на вытянутой орбите» would also be fine; this is screen-reader-only text, so I left it.
- The dashes use a plain space before «—», as the Labs ru page does. Strict Russian typesetting would use U+00A0 so that a line never starts with a dash. That's a sitewide choice, not something to fix per string.
- The rendered page hasn't been checked in a browser (the build was out of scope). Line breaks in the h1 and the chip width above are calculated, not observed.
