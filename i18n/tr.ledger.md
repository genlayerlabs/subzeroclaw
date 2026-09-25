# Turkish (tr) concept ledger: SubZeroClaw landing page

Sources: `README.md` (full; the opening warning, "What it does", "Routing & compaction via unhardcoded", "Tool", "Skills", "Philosophy" and "Config reference"), `CONTRIBUTING.md`, `site/llms.txt`, and `examples/decision-models/README.md` (for the decision controller). Shared terms follow the GenLayer Labs homepage (`~/dev/genlayerlabscom-site/i18n/tr.ledger.md`): ajan, ajan çalışma ortamı, yönlendirici, kabuk, döngü, "başa döner", sürü.

Register: formal "siz" imperatives (anlatın, okuyun, çalıştırın), as on the GenLayer Labs homepage. UI button labels (kopyala, "git clone komutunu kopyala") use the bare imperative, which is the Turkish UI convention. Numbers are written 1.400, units take a space (90 KB, ~2 MB), and suffixes on names and units go after an apostrophe (README'de, README'yi, 90 KB'lık, skill'ler).

## Objects

| Concept / object | Source term(s) | Meaning in context | Keep distinct from | TR | Avoid | Source passage |
|---|---|---|---|---|---|---|
| Agent | agent | The autonomous LLM process running one skill | – | ajan | temsilci, aracı | README "skill.md + LLM + shell + loop = autonomous agent"; homepage "ajan" |
| Agent runtime | (minimal) agent / agentic runtime | The C executable that runs the loop | framework | ajan çalışma ortamı | çalışma zamanı (reads as "run time") | README "A small C runtime for skill-driven agents"; homepage row "Agent runtime" |
| Skill / skill.md | skill, skill file, skills | A plain markdown file loaded into the system prompt | tool, plugin | skill dosyası; plural skill'ler; skill.md unchanged | beceri, yetenek (would break the link to skill.md and `skills/`) | README "Drop a `.md` file in `~/.subzeroclaw/skills/`. It becomes part of the system prompt." |
| Markdown file | markdown file | The job description the user writes | – | markdown dosyası | – | README "You write a skill as a markdown file." |
| The loop | loop | Call the model, run the command, repeat | – | döngü; "repeats until done" → "iş bitene kadar başa döner" | yineleme | README "It calls an LLM, executes tools, loops until done."; homepage "başa döner" |
| Shell / shell command | shell | The single tool (`popen`) | the user's machine | kabuk, kabuk komutu, kabuğunuz | shell (in running copy) | README "One tool: **shell**."; homepage "tek aracı kabuk" |
| Tool | tool | What the model can call; only the shell | integration | araç | alet | README "## Tool" |
| Integration | integration | Pre-existing CLI (git, curl…) the agent uses without adapters | tool | entegrasyon | bütünleştirme | README "No adapters, no integrations. The adapter is the shell." |
| Model / LLM | model, LLM | The language model the router picks | router | model; LLM kept in meta and JSON-LD | büyük dil modeli (too long for meta) | llms.txt |
| System prompt | system prompt | Where the skill is placed | confirmation prompt | sistem istemi | sistem prompt'u | README "reads the skill into its system prompt" |
| Router | router | unhardcoded: picks the model, keeps cache affinity, signals compaction | runtime | yönlendirici; "unhardcoded yönlendiricisi", "LLM yönlendiricisi unhardcoded" | router | homepage row "Router" |
| Model choice | model choice | Router picks (provider, model) per call | decision controller | model seçimi | – | README "The router picks the (provider, model) per call" |
| Prompt caching | prompt caching | Cache-affinity routing to the peer that holds the prefix | – | istem önbellekleme | prompt caching | README "routing with prompt-cache affinity" |
| Context compaction | context compaction | Old turns sealed asynchronously by the router | "summarize" | bağlam sıkıştırma | bağlam özetleme | README "Compaction is asynchronous" |
| "never pauses to summarize" | summarize | Compaction runs in the background, so the loop never stops for it | – | özetlemek için hiç durmaz | "özetlemez" (would claim no summarization at all) | llms.txt "the runtime does not summarize in-process … so the loop never pauses" |
| Sandbox | sandbox | Isolation layer; absent | – | sandbox | korumalı alan (Microsoft's term, less recognised by developers) | README "no sandboxing" |
| Confirmation prompt | confirmation prompts | Asking the user before running a command; absent | system prompt | onay istemi | onay sorusu | README "no confirmation prompts" |
| Allowlist | allowlist | List of permitted commands; absent | – | izin listesi | beyaz liste | – |
| Decision controller | decision controller | Opt-in controller that picks prepared shell actions and parameters via `/v1/decisions` | router, model | karar denetleyicisi (isteğe bağlı) | karar denetçisi (auditor), karar kontrolörü | decision-models README "The decision model selects procedures and their arguments together" |
| Config reference | config reference | README table of config keys | – | yapılandırma seçenekleri | yapılandırma başvurusu (calque) | README "## Config reference" |
| Quickstart | Quickstart (aria-label) | The code block | – | Hızlı başlangıç | – | – |
| Deps | deps | Dependencies (curl, unhardcoded) | – | bağımlılık ("2 bağımlılık", no plural after a numeral) | bağımlılıklar | llms.txt "Dependencies: a shell with curl, and unhardcoded" |
| Binary | binary | Compiled executable | – | ikili dosya (90 KB'lık bir ikili dosya) | binary | – |
| Swarm | swarms | Many agents at scale | – | ajan sürüleri | küme | homepage row "Swarm" |
| Edge devices | edge devices | – | – | uç cihazlar | kenar cihazlar | – |

## Texture words (step 2b)

| Concept | Decision | Why |
|---|---|---|
| "small" | **küçük** everywhere | The thesis word. "Minimal" is kept separately for "minimal runtime", where Turkish developers use the same word. |
| "run anywhere" | Headline: **o kadar küçük bir ajan ki <em>her yerde çalışır</em>**. Meta, title and alt text: **her yerde çalışabilecek kadar küçük bir ajan** | The "o kadar … ki" construction puts the verb last, so the accent `<em>` stays at the end as in the design. Noun-phrase titles need the "-ebilecek kadar" form, which the homepage also uses ("çalışabilecek kadar küçük"). |
| "framework" | **framework** ("Framework yok, yalnızca döngü.") | Turkish developers say "framework". "Çatı" is rare and would blunt the anti-framework line. |
| "Nothing stands between…" | **Modelle kabuğunuz arasında hiçbir engel yok** | "Engel" (barrier) makes the missing guardrails explicit. The warning is not softened: "Model neyi çalıştırmaya karar verirse o çalışır, rm -rf / dahil." |
| "you can afford to lose" | **kaybetmeyi göze alabileceğiniz** | The standard idiom for risk. "Give it a machine" becomes "…bir makinede ya da konteynerde çalıştırın" (run it on), which is the natural Turkish phrasing and means the same. |
| 404 "ran" | **bu yol, ulaşamadığımız bir yerde çalışıyor.** | "Çalışmak" = run/execute, so the joke about "run anywhere" survives. It is dry and lowercase. The tense moves to the present continuous, which reads better than "çalıştı". |
| copy chip | **kopyala / kopyalandı ✓** | Standard Turkish UI labels. CSS uppercases them with lang="tr" (KOPYALANDI). |

## Left in English

SubZeroClaw, GenLayer Labs, GitHub, README, MIT, unhardcoded (lowercase router name), skill.md, skill / skill'ler, markdown, LLM, framework, sandbox, C, KB/MB/RAM, the `git clone` command, all `<code>` content and paths.

## Audit notes

- Accuracy: the router "handles" model choice, caching and compaction → "yönlendiricisinin işi". The runtime "never pauses to summarize", not "never summarizes". Facts are unchanged: ~1.400 satır, 90 KB, ~2 MB RAM, 2 bağımlılık, © line.
- Every tag, attribute, href and `<code>` item is preserved (script-checked). The stats line uses `&nbsp;` inside each figure, as the English page does.
- Cold read: "tarif edin" → "anlatın" ("tarif" also means recipe), "bunu" → "bu dosyayı" (clearer referent), and the redundant "sizin" was removed.

## Review

Reviewer's pass: skill steps 6 (accuracy) and 7 (independent naturalness). Cold read first, then a comparison with en.json, README.md and site/llms.txt. Tags, hrefs and `<code>` contents were script-checked against the source and match; all 29 ids are present.

### Changes

| id | before → after | reason |
|---|---|---|
| 1f77c53c (stats) | `1.400&nbsp;satır&nbsp;C` → `1.400&nbsp;satır&nbsp;C&nbsp;kodu` | "satır C" is an English calque ("lines of C"). "C kodu" is what a Turkish developer writes. It is about the same length as the English, and the non-breaking spaces are kept. |
| 391ed594 (og:image:alt) | `1.400 satır C,` → `1.400 satır C kodu,` | Same fix, so the alt text matches the visible stats line. |
| 03765c8f (JSON-LD) | `LLM yönlendiricisi unhardcoded üzerinde çalışmak üzere tasarlandı.` → `Bir LLM yönlendiricisi olan unhardcoded üzerinde çalışmak üzere tasarlandı.` | The earlier version followed the English apposition order ("the LLM router unhardcoded"). The relative clause is the native form, keeps the lowercase name out of sentence-initial position, and puts no suffix on "unhardcoded". |
| 2989fe33, 03765c8f, 8f295ae5, 9f5acfe3 | ASCII `'` → `’` (KB’lık, skill’ler, README’de, README’yi) | The GenLayer Labs Turkish page uses the typographic apostrophe for every suffix (25 instances, no ASCII ones). This matches it. No tags or attributes are affected. |

### Accuracy findings (no change needed)

- The figures are unchanged. Meta and JSON-LD have ~1.400 satır, 90 KB, ~2 MB RAM, 2 bağımlılık. The stats line and og:image:alt drop the tilde, as the English does.
- The warning is not softened. It keeps no sandbox, no confirmation prompts, no allowlist, "Model neyi çalıştırmaya karar verirse o çalışır", and the destructive `<code>` example "dahil". "hiçbir engel yok" matches README "There is nothing between the model's output and your system."
- Router and compaction: "yönlendiricinin işi" means the same as "live in the router". "özetlemek için hiç durmaz" keeps the "never pauses" claim and does not say the runtime never summarises. This matches README ("it never pauses to compact") and llms.txt.
- The terms ajan, ajan çalışma ortamı, yönlendirici, kabuk, döngü/başa döner and Belgeler match the GenLayer Labs page.

### Verdicts on the open questions

- **istem vs prompt**: keep **istem**. It is the localized term in Microsoft and Google Turkish AI products, and it keeps "sistem istemi" and "onay istemi" consistent. Casual Turkish developer speech uses "prompt" more, so a native speaker should confirm.
- **sandbox vs korumalı alan**: keep **sandbox**. The warning has to be understood instantly, and developers say "sandbox". "Korumalı alan" is Microsoft product vocabulary. **framework vs çatı**: keep **framework**. "Çatı" is rare, and it would weaken the anti-framework line.
- **Headline "o kadar küçük bir ajan ki <em>her yerde çalışır</em>"**: not an overclaim. The "o kadar … ki" clause keeps the size → capability link of "small enough to". The Turkish aorist states a general capacity, much like "it runs anywhere". "çalışabilir" would be weaker and flatter than the English. It has exactly one `<em>`, placed at the end as in the design.
- **404 "bu yol, ulaşamadığımız bir yerde çalışıyor."**: acceptable. The run/çalışmak pun and the dry lowercase tone survive. One possible option, not applied because it is a matter of taste: the evidential past "çalışmış" ("apparently ran somewhere…") is closer to the English past tense and suits a 404.
- **Belgeler** (Docs): keep. It matches the GenLayer Labs nav. **Nasıl katkıda bulunulur** (link): keep. It is the standard rendering of "How to contribute" and describes the linked guide. The imperative "Katkıda bulunun" would turn it into a call to action.

### For a native speaker

- Confirm istem vs prompt for this audience.
- The page uses "ajan" without "yapay zekâ" on first use. The GenLayer Labs page adds it to rule out the spy sense. The code and terminal context make the meaning clear here, but a native speaker should check the title and SERP snippet.
- 404: "çalışıyor" or "çalışmış".
- Chip width: "KOPYALANDI ✓" is wider than "COPIED ✓". The command text truncates with an ellipsis, so the layout holds, but check it at mobile width in the build.

## 2026-09-25: new meta description and language suggestion bar

| id | TR | chars | decision |
|---|---|---|---|
| 5da16d26 (meta description, replaces 2989fe33) | SubZeroClaw, C ile yazılmış minimal bir ajan çalışma ortamı: ~1.400 satır, 90 KB ikili, ~2 MB RAM. Framework yok, yalnızca döngü. Her yerde çalışır. | 148 / 150 | The first two sentences reuse the approved old description word for word, except "90 KB’lık bir ikili dosya" becomes the stat-list shorthand "90 KB ikili". The literal "Her yerde çalışabilecek kadar küçük." pushed the text to 172+ characters. The close reuses the approved headline verb "her yerde çalışır" (the aorist of general capacity, which the review found is not an overclaim). "Small" is carried by "minimal" and the three figures. The figures are unchanged. 2989fe33 was deleted. |
| 4bee1489 (bar message) | Bu sayfa Türkçe olarak da mevcut. | 33 | Shown on other locales to visitors who prefer Turkish, so it names Turkish, not English. "… olarak da mevcut" is the standard Turkish UI phrasing for "also available in". |
| 0991c336 (bar link) | Türkçe okuyun | 13 | "siz" imperative, which is the page register. It is short enough to sit next to the message on a 390px phone. |
| 70afe9ef (close button aria-label) | Kapat | 5 | The standard Turkish UI label for a close (×) button. The bare imperative follows the UI-label convention (as "kopyala" does). |

For a native speaker: check whether "90 KB ikili" reads naturally in the SERP snippet. The alternative at the limit is "minimal ajan çalışma ortamı … 90 KB ikili dosya" (150 characters, dropping "bir").

### Editor's pass (2026-09-25)

Cold read of the four strings as a Turkish developer sees them (SERP snippet, suggestion bar), then against en.json.

| id | before → after | reason |
|---|---|---|
| 5da16d26 (meta description) | `minimal bir ajan çalışma ortamı: … 90 KB ikili, …` → `minimal ajan çalışma ortamı: … 90 KB ikili dosya, …` (148 → 150) | Bare "ikili" as a noun reads as "pair/duo" or a dangling adjective in a snippet; developers write "ikili dosya" (as the approved old description and JSON-LD do). Dropping the indefinite "bir" pays for it; the appositive "minimal ajan çalışma ortamı" is grammatical and natural. Exactly at the 150 limit. Figures unchanged. |

No change:

- 5da16d26 ending "Her yerde çalışır.": kept. It is the approved headline verb and makes no stronger claim than the English "small enough to run anywhere"; "minimal" plus the figures carry "small". "Her yerde çalışacak kadar küçük." would need 168 characters.
- 4bee1489 "Bu sayfa Türkçe olarak da mevcut.", 0991c336 "Türkçe okuyun", 70afe9ef "Kapat": natural, standard UI Turkish, register matches the page ("okuyun" as in "README’yi okuyun"; bare "Kapat" as in "kopyala"). Measured at 14px SF (500 message, 600 link): message + link ≈ 296px against ≈ 312px available on a 390px phone (16px left padding, 4px right, 44px button, 14px gap), so it fits on one line. The "Türkçe … Türkçe" repeat mirrors the English and each half must read alone (the link is also announced out of context).
- The earlier "for a native speaker" flag on "90 KB ikili" is resolved by this change.
