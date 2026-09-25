# Concept ledger: SubZeroClaw landing page, Spanish (es)

Register: neutral international Spanish, tú for direct address, infinitive for buttons and links. This matches the GenLayer Labs homepage (`~/dev/genlayerlabscom-site/i18n/es.ledger.md`). Everyday developer vocabulary, with no calques and no nominalized officialese.
Authorities: `README.md` (read in full, and especially "What it does", "Decision-model control", "Routing & compaction via unhardcoded", "Config reference" and "Philosophy"), `CONTRIBUTING.md`, `site/llms.txt`, `examples/decision-models/README.md` and the English page `site/index.html`. The Spanish GenLayer Labs homepage is the reference for vocabulary shared with that site.

## Objects

| Concept | Source term(s) | Meaning (source) | Must stay distinct from | Spanish | Avoid | Source passage |
|---|---|---|---|---|---|---|
| Agent | agent | The skill-driven process: skill + LLM + shell + loop | model | agente | | README "skill.md + LLM + shell + loop = autonomous agent" |
| Agent runtime | agent runtime, agentic runtime | The program that runs the agent loop | the model; "at runtime" | entorno de ejecución para agentes | "runtime" used bare in prose; "tiempo de ejecución" (means "at runtime") | llms.txt "A minimal agentic runtime written in C". Same as the GenLayer Labs homepage. |
| Minimal | minimal | Deliberately small code base (anti-framework) | minimalist aesthetics | mínimo | "minimalista" (design connotation) | CONTRIBUTING "stay minimal" |
| Small | small (enough to) | Size in lines, KB and RAM, which is what lets it run anywhere | lightweight | pequeño | "ligero" (weight metaphor that the page doesn't use) | llms.txt "Small enough to run anywhere" |
| Run anywhere | run anywhere | It executes on edge devices, a Pi or swarms | "fits anywhere" | funciona donde sea (headline, titles); funciona en cualquier parte (descriptions followed by a list) | "corre" (Fundéu calls "correr un programa" a calque), "cabe" (changes the claim) | llms.txt |
| Skill | skill, skill file, skill.md | Markdown file that says what the agent knows. The runtime puts it in the system prompt | tool; "habilidad" (ability) | skill (la skill, las skills); archivo de skill | "habilidad" (reads as a human ability, and the file and folder are literally `skills/`) | README "You write a skill as a markdown file" |
| The loop | loop | Call model, run tool, repeat until done | | el bucle | "loop" | llms.txt. GenLayer Labs homepage uses "bucle" |
| Job / task | job (page), task (README) | What the skill describes and the loop finishes | | la tarea | "el trabajo" twice in one sentence | README "until the task is complete" |
| Shell / shell command | shell, shell command | The only tool, via `popen` | | la shell (fem.); comando de shell | "intérprete de comandos" (too formal), "consola", "terminal" | GenLayer Labs homepage "la shell" |
| Tool | tool | What the model calls. Here only the shell | integration | herramienta | | CONTRIBUTING "The shell is the only tool" |
| Integration | integration | Code that connects the agent to a service. With shell access there is none to write | | integración; "que no tienes que programar" | "escribir una integración" (calque) | README Philosophy |
| Model / LLM | model, LLM | The language model the router picks | agent, router | modelo; LLM | | |
| System prompt | system prompt | Where the skill is inserted | | prompt de sistema | "mensaje del sistema" (vendor-specific), "indicación de sistema" | README "reads the skill into its system prompt" |
| Router | unhardcoded router | External substrate: model choice, prompt-cache affinity, compaction | the runtime | router (el router unhardcoded; router de LLM unhardcoded) | "enrutador" | README "Routing & compaction". Same as GenLayer Labs homepage |
| Model choice | model choice | The router picks the provider and model per call | | la elección del modelo | | README "The router picks the (provider, model) per call" |
| Prompt caching | prompt caching | Keeps the conversation on the peer that already holds the cached prefix | | la caché de prompts | "almacenamiento en caché de prompts" (heavy) | README "prompt-cache affinity" |
| Context compaction | context compaction | Router-side sealing of old turns, run asynchronously in the background | summary as a blocking step | la compactación del contexto | "compresión" | README "seals the old turns asynchronously ... it never pauses to compact" |
| Summarize | never pauses to summarize | The loop doesn't stop to summarize, because compaction is async | | nunca se detiene a resumir | "hacer un resumen" (heavier), "se para" (Spain-only) | README, llms.txt "does not summarize in-process" |
| Sandbox | sandbox | Isolation layer that SubZeroClaw lacks | | sandbox | "entorno aislado" here: the warning needs the exact term devs search for | README warning |
| Confirmation prompt | confirmation prompts | Asking the user before running a command | system prompt | confirmaciones | "prompts de confirmación" (collides with the system prompt) | README "no confirmation prompts" |
| Allowlist | allowlist | List of permitted commands | | lista de comandos permitidos | "lista blanca" (dated), "allowlist" | |
| Decision controller | optional decision controller | Opt-in mode (`decision_extra`) that picks prepared shell actions and their parameters | the default loop; the router | el controlador de decisiones opcional | "controlador de decisión" | examples/decision-models/README.md |
| Config reference | config reference | README table of config keys | | la referencia de configuración | "referencia de config" | README "Config reference" |
| Quickstart | Quickstart (aria-label) | Code block with the setup commands | | Inicio rápido | "Guía rápida" (implies prose) | |
| Framework | framework | Plugin systems, middleware, abstractions | | framework | "marco de trabajo" (nobody says it) | README Philosophy |
| Deps | 2 deps | curl-capable shell + unhardcoded | | 2 dependencias | "deps", "dep." | llms.txt "Dependencies" |
| Edge devices | edge devices | Small hardware at the edge | | dispositivos edge | "dispositivos periféricos" (reads as peripherals) | llms.txt |
| Swarms | large-scale swarms | Many agents at once | | enjambres (de agentes) a gran escala | | GenLayer Labs homepage uses "enjambre" |
| Binary | a 90KB binary | Compiled executable | | un binario de 90 KB | | |

## Texture words (step 2b)

| Word | Occurrences | Spanish | Rationale |
|---|---|---|---|
| small enough to run anywhere | h1, title, og/twitter titles, image alts | tan pequeño que funciona donde sea | Keeps "small" (pequeño) as the cause and "run" as the claim. "funciona" is the neutral Spanish verb for software running on a platform. "donde sea" is short and dry, which the three-line h1 needs. In the h1 the `<em>` wraps "funciona donde sea" at the end. |
| run anywhere (descriptions) | meta description, JSON-LD | funciona en cualquier parte | Reads better before a colon and a list of places. |
| minimal | meta and og descriptions, JSON-LD | mínimo | The thesis is about less code, not about looks. |
| No framework, just the loop | meta, og, twitter | Sin framework, solo el bucle. | Same fragment rhythm. "solo" without an accent (RAE 2010). |
| never pauses to summarize | loop section | nunca se detiene a resumir | Neutral verb. The accuracy point (async compaction) survives. |
| live in the router | loop section | quedan en manos del router | "viven en" is a calque. "quedan en manos de" is idiomatic delegation. |
| an integration you don't have to write | loop section | una integración que no tienes que programar | "programar" is what a Spanish dev says for writing integration code. |
| Nothing stands between… | warning h3 | Nada se interpone entre el modelo y tu shell | Native idiom with the same force. |
| No sandbox, no confirmation prompts, no allowlist. | warning body | Ni sandbox, ni confirmaciones, ni lista de comandos permitidos. | Emphatic "ni… ni… ni" is stronger than a "sin" list. Nothing softened. "rm -rf / incluido" is kept, and there is no comma between the subject clause and the verb ("Lo que el modelo decida ejecutar se ejecuta"). |
| ran somewhere we can't reach (404) | 404 | esta ruta salió corriendo hacia algún lugar al que no llegamos. | "salir corriendo" (run off) keeps the pun on "run" in natural Spanish. Lowercase and dry. |

## Left in English (deliberately)

SubZeroClaw, GenLayer Labs, GitHub, README, MIT (as "Licencia MIT"), unhardcoded (lowercase router name), skill.md, skill/skills, C, LLM, RAM, KB/MB, markdown (lowercase as in the source), shell, sandbox, framework, prompt, router, edge, Raspberry Pi, git clone, code in `<code>`, file paths, and "Docs" (nav link).

## Conventions

- Numbers: **1400** with no separator. Four-digit numbers take no grouping in both the RAE rules (Ortografía 2010) and CLDR `es`. **This differs from the GenLayer research guide, which wrote "2.000 $"** (that page had to handle bigger amounts). The tilde (~) for "approximately" is kept as in the source.
- Units: a space between number and unit ("90 KB", "~2 MB"), which is the RAE and SI norm. The source's "90KB" is not kept.
- Stats line: NO-BREAK SPACE (U+00A0) inside each item, as the English markup uses `&nbsp;`, so no item breaks across lines.
- Titles: "SubZeroClaw: un agente…" uses a colon instead of the English em dash. A colon is the natural Spanish separator for a name plus tagline.
- "Docs" rather than the homepage's "Documentación". The nav slot is tight between 481 and 820 px (brand, Docs, GitHub ↗, language select), and "Docs" is common Spanish developer usage. Switch to "Documentación" if the layout allows.
- Buttons and links in the infinitive (Leer el README, Leer el código fuente, Copiar el comando git clone). The copy chip says "copiar" / "copiado ✓".

## Section semantic briefs

- **Hero.** A minimal agent runtime whose small size lets it run anywhere. Ten-second takeaway: "tiny agent, runs on anything".
- **Loop.** Skill (markdown) → system prompt → model → shell command → repeat. The shell as the only tool means every installed CLI is already an integration. The router handles model choice, caching and compaction, so the loop stays small and never stops to summarize (compaction is async on the router's signal, per README).
- **Warning.** No safety layer at all. The model's commands run as-is, `rm -rf /` included. Use a disposable machine or container.
- **Footer and links.** README, source, contributing guide.

## Audits

Accuracy (step 6): the facts are unchanged (~1400 lines, 90 KB, ~2 MB RAM, 2 dependencies, © 2026 GenLayer Labs · MIT). The router's three roles are listed as in the source, with no added claims. "Never pauses to summarize" is not strengthened into "never summarizes". The warning is not softened. The decision controller is still "optional". "Pensado para usarse con el router" keeps the design-intent wording of "Designed to run on" without claiming it is required. Tags, hrefs and `<code>` contents match the source exactly (checked by script).

Naturalness (step 7), with fixes made after a cold read:
- JSON-LD: "Diseñado para funcionar con" became "Pensado para usarse con", which avoids "funciona… funcionar" twice in one description.
- Orbit aria-label: "primero por detrás y de nuevo por delante" became "se oculta tras él y vuelve a salir por delante", a clear sequence that doesn't mix "primero" and "de nuevo".
- Loop copy: "lo pone en" became "lo incluye en" (more precise). "viven en el router" became "quedan en manos del router".

Open for a native reviewer:
1. Headline length. At 44 characters it is longer than the English 37 and may wrap to four lines at 80px. The shorter fallback is "un agente tan pequeño que <em>corre donde sea</em>" (41), which is colloquial and a mild calque.
2. "skill" left in English versus "habilidad".
3. "1400" (RAE/CLDR) versus "1.400" (the research guide's habit).
4. "Docs" versus "Documentación".

## Review

Independent review (steps 6 and 7). I read the Spanish cold first, then checked it against `en.json`, `README.md` and `site/llms.txt`, and ran a script over the tags. All 29 ids are present. Every tag, href and `<code>` content is byte-identical to the source, and the h1 has exactly one `<em>`.

Accuracy: no problems found. The figures (~1400 lines, 90 KB, ~2 MB RAM, 2 dependencies) match llms.txt. The warning keeps all three absences and the destructive `rm` example, with nothing softened. The router sentence has the same three roles as README "Routing & compaction". "nunca se detiene a resumir" matches the asynchronous compaction and is not strengthened. "opcional" is kept on the decision controller.

Naturalness: the cold read found nothing stiff or calqued. The register is tú and infinitive buttons throughout.

### Changes

| id | before → after | reason |
|---|---|---|
| 4eed1542 | `<em>funciona donde sea</em>` → `<em>funciona donde␣sea</em>` (␣ = raw U+00A0) | Layout. I measured Bricolage Grotesque 800 with the h1 CSS (clamp(2.4rem,6vw,5rem), −0.028em, 1.2fr column). At every desktop width from 821 to 1440 px and at 360 px, the h1 breaks as "un agente tan / pequeño que / funciona donde / sea", which leaves "sea" alone on a line and splits the emphasized phrase. The no-break space makes it "… / funciona / donde sea". Where the whole phrase fits (~390 to 800 px) nothing changes. The raw U+00A0 follows the convention of the stats line. |

### Verdicts on the translator's open questions

1. **Headline.** Keep "funciona donde sea", now with the no-break space. "corre donde sea" does not reliably fix the wrap. At the widest layout (column capped at ~535 px, 80 px type), "corre donde sea" measures about 102% of the column, so it also drops to four lines with "sea" orphaned, and it costs register. Other short options I rejected: "va donde sea" (three lines, but colloquial and closer to Spain usage), "cabe donde sea" (changes the claim to "fits"), and "funciona en todo" (vague, and still four lines at ≥1280 px). No compact phrasing that keeps "funciona" gets back to three lines, so the desktop h1 is four lines against the English three, which I accept. If three lines matter, the fix belongs in CSS (for example `text-wrap: balance` on h1), outside this file.
2. **"skill".** Keep it in English (la skill, las skills). The repo folder is literally `skills/` and the file is `skill.md`. Spanish developer usage says "skills", and "habilidad" reads as a human ability.
3. **1400 vs 1.400.** Keep **1400**. RAE (Ortografía 2010) and CLDR `es` (minimum grouping digits = 2) do not group four-digit numbers, and all five occurrences on this page are consistent. The "2.000 $" belongs to a different page, the research guide, and the GenLayer Labs homepage `es.json` has no four-digit number to clash with.
4. **Docs vs Documentación.** Keep **"Docs"**. This is a justified deviation from the homepage. At 481 px the nav content box is ~433 px. The brand (~195 px) plus "Documentación" (~95 px at 14.5 px), "GitHub ↗" and the ES select, with 16 px gaps, comes to ~440 px, so it would overflow or squeeze just above the 480 px breakpoint. "Docs" is ~32 px, it is common Spanish developer usage, and it matches the English nav.
5. **"lista de comandos permitidos" for allowlist.** Keep it. It is precise and immediately understood. "lista blanca" is dated and being phased out, and a bare "allowlist" is less transparent than "sandbox", which does stay in English.

### For a native human

- Look at the rendered h1 at about 1440 and 1024 px to confirm that the four-line break "funciona / donde sea" reads well. My widths come from font metrics (PIL), not from a browser.
- A matter of taste, left unchanged: "prompt de sistema" vs "prompt del sistema" (the latter is somewhat more frequent in Spanish AI docs). Both are correct.

## 2026-09-25: new catalogue ids

| id | Spanish | chars | decision |
|---|---|---|---|
| 5da16d26 (meta description, replaces 2989fe33, which is deleted) | SubZeroClaw: un entorno de ejecución mínimo para agentes en C. ~1400 líneas, 90 KB, ~2 MB de RAM. Sin framework, solo el bucle. Funciona donde sea. | 147 / 150 | Reuses the approved "entorno de ejecución mínimo para agentes", "Sin framework, solo el bucle" and "funciona donde sea". The brand is followed by a colon, as in the titles. "90 KB" appears without "binario", as in the approved stats line and og description (1f77c53c, 391ed594), because keeping "un binario de" and "escrito" would take it over 150. "Tan pequeño que" is dropped for the same reason. The figures just before it carry the size, so the claim is not weakened. The facts are unchanged: ~1400, 90 KB, ~2 MB. |
| 4bee1489 (language bar message) | También disponible en español. | 30 | Shown on other locales to Spanish-preferring browsers, so it names Spanish. "Esta página también está disponible en español." is the literal version, but it would not fit on one line next to the link at 390 px. Measured with SF 14 px, message plus link is ~279 px against ~298 px of room (390 − 16 − 4 padding − 44 button − 2×14 gap). "Esta página también está en español." measures ~312 px and would wrap. "También disponible en…" is a standard Spanish UI fragment. |
| 0991c336 (language bar link) | Leer en español | 15 | Infinitive for links, as in the ledger conventions. "la" is dropped ("Leerla") because the bare infinitive is the usual Spanish link style. |
| 70afe9ef (close button aria-label) | Cerrar | 6 | "Cerrar" is the standard accessible name for an × button. "Descartar" is a literal rendering of "Dismiss" and reads as discarding data. |
| 68a41942 (nav) | Docs | 4 | Confirmed per the review above: normal developer usage, and "Documentación" overflows the nav at 481 to 820 px. Listed in `_same_as_english`. |

### 2026-09-25: second pass (native editor)

Cold read first, then checked against `en.json` and the `.langbar` CSS in `site/index.html`. The bar was rendered in headless Chrome on macOS (system-ui resolves to SF, which iOS also uses) with the page's exact bar CSS, at 360, 375 and 390 px, `isMobile`.

| id | before → after | reason |
|---|---|---|
| 4bee1489 | `También disponible en español.` → `Esta página también está en español.` | Layout, and the result also reads better. In Chrome, "También disponible en español. Leer en español" measures 317 px against 312 px of room at 390 px (390 − 16 − 4 padding − 44 button − one 14 px gap). The earlier ~279 px figure came from font metrics and was too low. The bar then wraps and drops the × to a second row, making it 88 px tall, and the same happens at 375 and 360. The new message plus "Leer" measures 279 px. It stays on one line at 390, 375 and 360 px, and the bar keeps its 44 px height. It is also the natural full sentence, and it avoids saying "en español" twice in one line. |
| 0991c336 | `Leer en español` → `Leer` | Needed so the full sentence fits. The link sits inside the sentence that names the language, so its purpose is clear from context (WCAG 2.4.4). It also keeps `hreflang="es"`, and the infinitive matches the ledger's link style ("Leer el README"). Rejected: "Disponible en español. Leer en español" (259 px, but it repeats itself and reads machine-made), "También en español. Leer en español" (same problem), and "…Ver en español" (310 px, which fits by only 2 px at 390). |
| 5da16d26 | no change (147/150) | The cold read is fine. "para agentes en C" carries the same attachment as the English "agent runtime in C", and the comma list works as a spec fragment. Restoring ", escrito en C", "un binario de" or "Tan pequeño que" would each push it over 150. |
| 70afe9ef | no change | "Cerrar" is the standard accessible name for ×. |
| 68a41942 | no change | "Docs" is confirmed and stays listed in `_same_as_english`. |

Validation: `validate('es', …)` returns `[]`. 2989fe33 is absent, and the only non-catalogue key is `_same_as_english`. Build not run.
