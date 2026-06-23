/* subzeroclaw.c — skill-driven agentic runtime */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <cjson/cJSON.h>

#define MAX_PATH   512
#define MAX_VALUE  1024
#define MAX_OUTPUT (128 * 1024)
#define MAX_EXTRA  8192   /* request_extra: an operator-supplied JSON body override */

typedef struct {
    char api_key[MAX_VALUE], endpoint[MAX_VALUE];
    char skills_dir[MAX_PATH], log_dir[MAX_PATH];
    char request_extra[MAX_EXTRA];   /* the loop JSON: model + routing policy_ir */
    char compact_extra[MAX_EXTRA];   /* the compaction JSON: keep_recent + seal policy_ir */
    char session[MAX_VALUE];   /* runtime per-run id (sid); sent so the router can
                                  keep this conversation pinned to its cache-hot peer */
    int  max_turns;
} Config;

static void config_parse_line(Config *cfg, const char *key, const char *val) {
    if      (!strcmp(key, "api_key"))      snprintf(cfg->api_key,    MAX_VALUE, "%s", val);
    else if (!strcmp(key, "endpoint"))     snprintf(cfg->endpoint,   MAX_VALUE, "%s", val);
    else if (!strcmp(key, "skills_dir"))   snprintf(cfg->skills_dir, MAX_PATH,  "%s", val);
    else if (!strcmp(key, "log_dir"))      snprintf(cfg->log_dir,    MAX_PATH,  "%s", val);
    else if (!strcmp(key, "request_extra")) snprintf(cfg->request_extra, MAX_EXTRA, "%s", val);
    else if (!strcmp(key, "compact_extra")) snprintf(cfg->compact_extra, MAX_EXTRA, "%s", val);
    else if (!strcmp(key, "max_turns"))    cfg->max_turns    = atoi(val);
}

int config_load(Config *cfg) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->endpoint,   MAX_VALUE, "https://openrouter.ai/api/v1/chat/completions");
    snprintf(cfg->skills_dir, MAX_PATH,  "%s/.subzeroclaw/skills", home);
    snprintf(cfg->log_dir,    MAX_PATH,  "%s/.subzeroclaw/logs", home);
    cfg->max_turns = 200;

    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "%s/.subzeroclaw/config", home);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[2048];
        while (fgets(line, sizeof(line), f)) {
            size_t len = strlen(line);
            while (len && strchr("\n\r ", line[len - 1])) line[--len] = '\0';
            char *s = line; while (*s == ' ') s++;
            if (*s == '#' || *s == '\0') continue;
            char *eq = strchr(s, '='); if (!eq) continue; *eq = '\0';
            char *key = s, *val = eq + 1;
            len = strlen(key); while (len && key[len-1] == ' ') key[--len] = '\0';
            while (*val == ' ') val++;
            len = strlen(val);
            if (len >= 2 && val[0] == '"' && val[len-1] == '"') { val++; val[len-2] = '\0'; }
            config_parse_line(cfg, key, val);
        }
        fclose(f);
    }
    char *v;
    if ((v = getenv("SUBZEROCLAW_API_KEY")))  snprintf(cfg->api_key,  MAX_VALUE, "%s", v);
    if ((v = getenv("SUBZEROCLAW_ENDPOINT"))) snprintf(cfg->endpoint, MAX_VALUE, "%s", v);
    if ((v = getenv("SUBZEROCLAW_REQUEST_EXTRA"))) snprintf(cfg->request_extra, MAX_EXTRA, "%s", v);
    if ((v = getenv("SUBZEROCLAW_COMPACT_EXTRA"))) snprintf(cfg->compact_extra, MAX_EXTRA, "%s", v);
    if (!cfg->api_key[0]) { fprintf(stderr, "error: no api_key\n"); return -1; }

    /* Scrub the provider config from our own environment now that it lives in cfg.
       The one tool we expose, "shell", runs commands via popen() (/bin/sh -c),
       which inherits this process's environment — so without this an LLM-driven
       command could read the key/endpoint with `echo $SUBZEROCLAW_API_KEY`, `env`,
       or by catting /proc/<pid>/environ. unsetenv() alone is NOT enough: getenv()
       returns a pointer into the original environment region that
       /proc/<pid>/environ exposes, and unsetenv() leaves those bytes intact — so
       wipe the value in place first, then remove the name. The values remain in
       cfg, so requests are unaffected. */
    {
        static const char *const secret_vars[] = {
            "SUBZEROCLAW_API_KEY", "SUBZEROCLAW_ENDPOINT",
            "SUBZEROCLAW_REQUEST_EXTRA", "SUBZEROCLAW_COMPACT_EXTRA"
        };
        for (size_t i = 0; i < sizeof(secret_vars) / sizeof(secret_vars[0]); i++) {
            char *p = getenv(secret_vars[i]);
            if (p) memset(p, 0, strlen(p));
            unsetenv(secret_vars[i]);
        }
    }
    return 0;
}

static void mkdirp(const char *path) {
    char tmp[MAX_PATH];
    snprintf(tmp, MAX_PATH, "%s", path);
    for (char *p = tmp + 1; *p; p++)
        if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
    mkdir(tmp, 0755);
}

static void log_write(FILE *log, const char *role, const char *content) {
    if (!log) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[32]; strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", t);
    fprintf(log, "[%s] %s: %s\n", ts, role, content); fflush(log);
}

static int write_temp(const char *prefix, const char *data, char *out, size_t out_size) {
    snprintf(out, out_size, "/tmp/.szc_%s_XXXXXX", prefix);
    int fd = mkstemp(out); if (fd < 0) return -1;
    FILE *f = fdopen(fd, "w");
    if (!f) { close(fd); unlink(out); return -1; }
    fputs(data, f); fclose(f);
    return 0;
}

static char *http_post(const char *url, const char *api_key, const char *body) {
    char body_path[64], hdr_path[64];
    if (write_temp("body", body, body_path, sizeof(body_path)) < 0) return NULL;
    char hdr[MAX_VALUE + 64];
    snprintf(hdr, sizeof(hdr), "-H \"Authorization: Bearer %s\"", api_key);
    if (write_temp("hdr", hdr, hdr_path, sizeof(hdr_path)) < 0) { unlink(body_path); return NULL; }

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "curl -s -m 120 -K '%s' -H 'Content-Type: application/json' -d @'%s' '%s' 2>&1",
        hdr_path, body_path, url);
    FILE *fp = popen(cmd, "r");
    if (!fp) { unlink(body_path); unlink(hdr_path); return NULL; }

    size_t cap = 65536, len = 0, n;
    char *buf = malloc(cap);
    while (buf && (n = fread(buf + len, 1, cap - len - 1, fp)) > 0) {
        len += n;
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); buf = NULL; break; }
            buf = nb;
        }
    }
    if (buf) buf[len] = '\0';
    pclose(fp); unlink(body_path); unlink(hdr_path);
    return buf;
}

static const char TOOLS_JSON[] =
    "[{\"type\":\"function\",\"function\":{\"name\":\"shell\","
    "\"description\":\"Run a shell command\","
    "\"parameters\":{\"type\":\"object\","
    "\"properties\":{\"command\":{\"type\":\"string\"}},\"required\":[\"command\"]}}}]";

char *tool_execute(const char *name, const char *args_json) {
    if (strcmp(name, "shell")) return strdup("error: unknown tool");
    cJSON *args = cJSON_Parse(args_json);
    cJSON *cmd_item = args ? cJSON_GetObjectItem(args, "command") : NULL;
    const char *cmd = (cmd_item && cJSON_IsString(cmd_item)) ? cmd_item->valuestring : NULL;
    if (!cmd) { if (args) cJSON_Delete(args); return strdup("error: missing 'command'"); }
    /* Wrap as "{\n cmd \n} 2>&1": newline separators work for heredocs
       (terminator must be alone on its line) and for commands ending in ';'.
       The "; }" form breaks both. */
    size_t len = strlen(cmd);
    char *full = malloc(len + 16);
    memcpy(full, "{\n", 2); memcpy(full + 2, cmd, len);
    memcpy(full + 2 + len, "\n} 2>&1", 8);
    if (args) cJSON_Delete(args);
    FILE *fp = popen(full, "r"); free(full);
    if (!fp) return strdup("[exit:-1] error: popen failed");
    /* Reserve 16 bytes at the head for the "[exit:N] " prefix */
    char *out = malloc(MAX_OUTPUT + 16);
    size_t total = 16, n;
    /* Bound each read by the space left in `out` (cap = MAX_OUTPUT+16, with
       out[total] reserved for the trailing NUL). The previous loop kept the
       per-read count at MAX_OUTPUT-1 even as `total` grew, so the SECOND fread
       on any tool output under ~128KB ran past the buffer. On a fortified (-O2,
       Ubuntu-default _FORTIFY_SOURCE) glibc build that is a fatal __fread_chk
       abort (SIGABRT) — it silently killed the agent the moment it read back a
       small tool result (e.g. a `swarm-msg ask` envelope), wedging the turn. */
    while (total < MAX_OUTPUT + 15 &&
           (n = fread(out + total, 1, (MAX_OUTPUT + 15) - total, fp)) > 0) {
        total += n;
    }
    out[total] = '\0';
    int status = pclose(fp);
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    char prefix[16];
    int plen = snprintf(prefix, sizeof(prefix), "[exit:%d] ", code);
    memmove(out + plen, out + 16, total - 16 + 1);
    memcpy(out, prefix, plen);
    return out;
}

char *agent_build_system_prompt(const char *skills_dir) {
    size_t cap = 8192;
    char *prompt = malloc(cap);
    size_t len = snprintf(prompt, cap,
        "You are SubZeroClaw, a minimal agentic assistant.\n"
        "You have one tool: shell. Use it to run any command.\n"
        "For files, use cat, tee, sed, etc. Be concise. Just do it.\n\n");
    DIR *d = opendir(skills_dir); if (!d) return prompt;
    struct dirent *entry;
    while ((entry = readdir(d))) {
        size_t nlen = strlen(entry->d_name);
        if (nlen < 4 || strcmp(entry->d_name + nlen - 3, ".md") != 0) continue;
        char fp[MAX_PATH]; snprintf(fp, MAX_PATH, "%s/%s", skills_dir, entry->d_name);
        FILE *sf = fopen(fp, "r"); if (!sf) continue;
        fseek(sf, 0, SEEK_END); long sz = ftell(sf); fseek(sf, 0, SEEK_SET);
        char *content = malloc(sz + 1);
        content[fread(content, 1, sz, sf)] = '\0'; fclose(sf);
        size_t clen = strlen(content);
        while (len + clen + 128 >= cap) { cap *= 2; prompt = realloc(prompt, cap); }
        len += snprintf(prompt + len, cap - len, "\n--- SKILL: %s ---\n", entry->d_name);
        memcpy(prompt + len, content, clen); len += clen; prompt[len] = '\0';
        free(content);
    }
    closedir(d);
    return prompt;
}

typedef struct {
    char *finish_reason, *text;
    cJSON *tool_calls, *msg;
    int compact;   /* router asked us to seal context (x_router.compact) */
} Response;

static void response_free(Response *r) {
    if (r->finish_reason) free(r->finish_reason);
    if (r->msg) cJSON_Delete(r->msg);
}

/* references avoid copying the full message array */
static char *build_request(const Config *cfg, cJSON *msgs, cJSON *tools) {
    cJSON *req = cJSON_CreateObject();
    /* Session id (if any): sent so the router can keep this conversation pinned to
       the peer that already holds its prompt-cache prefix (cache_hot). */
    if (cfg->session[0]) cJSON_AddStringToObject(req, "session", cfg->session);

    /* The model is NOT hardcoded by the agent. It rides in the operator's
       SUBZEROCLAW_REQUEST_EXTRA — "model":"policy:auto" plus a "policy_ir" to let
       the unhardcoded router decide, or a concrete model for a direct provider.
       SUBZEROCLAW_REQUEST_EXTRA is a JSON object whose top-level keys are merged
       in: empty/unset → unchanged; malformed → ignored; on a key collision the
       override wins. Applied before messages/tools so it can add `model`,
       `policy_ir`, params (temperature, …) without colliding with the reference
       items below. */
    if (cfg->request_extra[0]) {
        cJSON *extra = cJSON_Parse(cfg->request_extra);
        if (extra && cJSON_IsObject(extra)) {
            cJSON *it = NULL;
            cJSON_ArrayForEach(it, extra) {
                cJSON *dup = cJSON_Duplicate(it, 1);
                if (!dup) continue;
                cJSON_DeleteItemFromObject(req, it->string);  /* override wins */
                cJSON_AddItemToObject(req, it->string, dup);
            }
        }
        if (extra) cJSON_Delete(extra);
    }

    /* messages/tools are owned by the runtime and added by reference (not copied);
       skip if the override already supplied them, so they are never duplicated. */
    int msgs_ref = 0, tools_ref = 0;
    if (!cJSON_GetObjectItem(req, "messages")) {
        cJSON_AddItemReferenceToObject(req, "messages", msgs); msgs_ref = 1;
    }
    if (tools && !cJSON_GetObjectItem(req, "tools")) {
        cJSON_AddItemReferenceToObject(req, "tools", tools); tools_ref = 1;
    }
    char *json = cJSON_PrintUnformatted(req);
    if (msgs_ref)  cJSON_DetachItemFromObject(req, "messages");
    if (tools_ref) cJSON_DetachItemFromObject(req, "tools");
    cJSON_Delete(req);
    return json;
}

#if defined(__GNUC__) || defined(__clang__)
#define SZC_WEAK __attribute__((weak))
#else
#define SZC_WEAK
#endif

/* Weak so an optional module (e.g. szc_mock.c) can override at link time. */
SZC_WEAK char *llm_chat(const Config *cfg, cJSON *msgs, cJSON *tools) {
    char *rj = build_request(cfg, msgs, tools);
    if (!rj) return NULL;
    char *rb = http_post(cfg->endpoint, cfg->api_key, rj);
    free(rj);
    return rb;
}

static int parse_response(const char *body, Response *out) {
    memset(out, 0, sizeof(*out));
    cJSON *root = cJSON_Parse(body); if (!root) return -1;
    cJSON *err = cJSON_GetObjectItem(root, "error");
    if (err) {
        cJSON *m = cJSON_GetObjectItem(err, "message");
        fprintf(stderr, "API error: %s\n", m ? m->valuestring : "unknown");
        cJSON_Delete(root); return -1;
    }
    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (!choices || !choices->child) { cJSON_Delete(root); return -1; }
    cJSON *choice  = choices->child;
    cJSON *message = cJSON_GetObjectItem(choice, "message");
    if (!message) { cJSON_Delete(root); return -1; }
    cJSON *fr = cJSON_GetObjectItem(choice, "finish_reason");
    out->finish_reason = strdup(fr && cJSON_IsString(fr) ? fr->valuestring : "stop");
    out->msg = cJSON_DetachItemFromObject(choice, "message");
    cJSON *ct = cJSON_GetObjectItem(out->msg, "content");
    out->text = (ct && cJSON_IsString(ct)) ? ct->valuestring : NULL;
    out->tool_calls = cJSON_GetObjectItem(out->msg, "tool_calls");
    cJSON *xr = cJSON_GetObjectItem(root, "x_router");
    out->compact = xr && cJSON_IsTrue(cJSON_GetObjectItem(xr, "compact"));
    cJSON_Delete(root);
    return 0;
}

static cJSON *make_msg(const char *role, const char *content) {
    cJSON *m = cJSON_CreateObject();
    cJSON_AddStringToObject(m, "role", role);
    cJSON_AddStringToObject(m, "content", content);
    return m;
}

/* Context compaction is no longer summarized in C. The router's /v1/compact seals
   the aged middle append-only (the frozen prefix + recent tail stay byte-identical,
   so the prompt-cache prefix is never invalidated). We run it ASYNCHRONOUSLY: fire
   the seal in the background, keep doing turns, and splice the result in when it
   lands — so a turn is never blocked and nobody perceives a compaction pause. */

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return NULL;
    size_t cap = 65536, len = 0, n;
    char *buf = malloc(cap);
    while (buf && (n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += n;
        if (len + 1 >= cap) { cap *= 2; char *nb = realloc(buf, cap);
            if (!nb) { free(buf); buf = NULL; break; } buf = nb; }
    }
    if (buf) buf[len] = '\0';
    fclose(f);
    return buf;
}

/* The /v1/compact URL, derived from the chat endpoint. */
static void compact_url(const Config *cfg, char *out, size_t sz) {
    const char *e = cfg->endpoint, *p = strstr(e, "/chat/completions");
    if (p) snprintf(out, sz, "%.*s/compact", (int)(p - e), e);
    else   snprintf(out, sz, "%s/compact", e);
}

/* Fire an append-only seal of the current `msgs` in the BACKGROUND: POST to the
   router's /v1/compact, atomically writing the spliced array to `res_path`. The
   seal routing + keep_recent ride in SUBZEROCLAW_COMPACT_EXTRA. Returns 0 if
   launched; the agent keeps looping and picks up the result on a later turn. */
static int compact_fire(const Config *cfg, cJSON *msgs, char *res_path, size_t res_sz) {
    cJSON *req = cJSON_CreateObject();
    if (cfg->compact_extra[0]) {            /* keep_recent / policy_ir / max_tokens */
        cJSON *extra = cJSON_Parse(cfg->compact_extra);
        if (extra && cJSON_IsObject(extra)) {
            cJSON *it = NULL;
            cJSON_ArrayForEach(it, extra) {
                cJSON *dup = cJSON_Duplicate(it, 1);
                if (dup) cJSON_AddItemToObject(req, it->string, dup);
            }
        }
        if (extra) cJSON_Delete(extra);
    }
    cJSON_AddItemReferenceToObject(req, "messages", msgs);
    char *body = cJSON_PrintUnformatted(req);
    cJSON_DetachItemFromObject(req, "messages");
    cJSON_Delete(req);
    if (!body) return -1;

    char body_path[64], hdr_path[64];
    if (write_temp("cbody", body, body_path, sizeof(body_path)) < 0) { free(body); return -1; }
    free(body);
    char hdr[MAX_VALUE + 64];
    snprintf(hdr, sizeof(hdr), "-H \"Authorization: Bearer %s\"", cfg->api_key);
    if (write_temp("chdr", hdr, hdr_path, sizeof(hdr_path)) < 0) { unlink(body_path); return -1; }

    char url[MAX_VALUE + 16]; compact_url(cfg, url, sizeof(url));
    snprintf(res_path, res_sz, "/tmp/.szc_cres_%d", (int)getpid());
    /* atomic: write .tmp then rename, so res_path appears only when complete; the
       subshell cleans its own temp files and detaches (&) so this returns at once. */
    char cmd[2048 + MAX_VALUE];
    snprintf(cmd, sizeof(cmd),
        "( curl -s -m 120 -K '%s' -H 'Content-Type: application/json' -d @'%s' '%s' "
        "-o '%s.tmp' && mv '%s.tmp' '%s' ; rm -f '%s' '%s' ) >/dev/null 2>&1 &",
        hdr_path, body_path, url, res_path, res_path, res_path, body_path, hdr_path);
    return system(cmd) == 0 ? 0 : -1;
}

/* A background seal finished: replace the `snapshot_len` compacted messages with
   the sealed view, keeping every turn that arrived while it ran (append-only ⇒
   conflict-free). */
static void compact_splice(cJSON *msgs, int snapshot_len, const char *res_path, FILE *log) {
    char *buf = read_file(res_path);
    unlink(res_path);
    if (!buf) return;
    cJSON *root = cJSON_Parse(buf); free(buf);
    cJSON *sp = root ? cJSON_GetObjectItem(root, "messages") : NULL;
    if (sp && cJSON_IsArray(sp) && cJSON_GetArraySize(msgs) >= snapshot_len) {
        for (int i = 0; i < snapshot_len; i++) cJSON_DeleteItemFromArray(msgs, 0);
        for (int i = cJSON_GetArraySize(sp) - 1; i >= 0; i--)
            cJSON_InsertItemInArray(msgs, 0, cJSON_Duplicate(cJSON_GetArrayItem(sp, i), 1));
        log_write(log, "SYS", "context compacted (append-only, async)");
    }
    if (root) cJSON_Delete(root);
}

static void process_tool_calls(cJSON *tool_calls, cJSON *msgs, FILE *log) {
    cJSON *tc = NULL;
    cJSON_ArrayForEach(tc, tool_calls) {
        cJSON *id = cJSON_GetObjectItem(tc, "id");
        cJSON *fn = cJSON_GetObjectItem(tc, "function");
        if (!id || !fn) continue;
        cJSON *name = cJSON_GetObjectItem(fn, "name");
        cJSON *args = cJSON_GetObjectItem(fn, "arguments");
        if (!name || !args) continue;
        // Log tool name with command argument
        cJSON *parsed_args = cJSON_Parse(args->valuestring);
        cJSON *cmd_arg = parsed_args ? cJSON_GetObjectItem(parsed_args, "command") : NULL;
        if (cmd_arg && cJSON_IsString(cmd_arg)) {
            char tool_log[4096];
            snprintf(tool_log, sizeof(tool_log), "%s: %s", name->valuestring, cmd_arg->valuestring);
            log_write(log, "TOOL", tool_log);
        } else {
            log_write(log, "TOOL", name->valuestring);
        }
        if (parsed_args) cJSON_Delete(parsed_args);
        char *result = tool_execute(name->valuestring, args->valuestring);
        log_write(log, "RES", result ? result : "null");
        cJSON *tm = cJSON_CreateObject();
        cJSON_AddStringToObject(tm, "role", "tool");
        cJSON_AddStringToObject(tm, "tool_call_id", id->valuestring);
        cJSON_AddStringToObject(tm, "content", result ? result : "error");
        cJSON_AddItemToArray(msgs, tm);
        free(result);
    }
}

static int agent_run(const Config *cfg, cJSON *msgs, cJSON *tools,
                     const char *input, FILE *log)
{
    cJSON_AddItemToArray(msgs, make_msg("user", input));
    log_write(log, "USER", input);

    int compacting = 0, snapshot_len = 0;
    char compact_res[80] = {0};
    for (int turn = 1; turn <= cfg->max_turns; turn++) {
        /* a background seal finished while we kept working? splice it in now —
           append-only, so it never collides with the turns added meanwhile. */
        if (compacting && access(compact_res, F_OK) == 0) {
            compact_splice(msgs, snapshot_len, compact_res, log);
            compacting = 0;
        }
        fprintf(stderr, "[%d] ...\n", turn);
        char *rb = llm_chat(cfg, msgs, tools);
        if (!rb) return -1;
        Response resp;
        if (parse_response(rb, &resp) != 0) { free(rb); return -1; }
        free(rb);
        cJSON_AddItemToArray(msgs, resp.msg); resp.msg = NULL;

        /* router signalled context pressure: seal in the background and keep going */
        if (resp.compact && !compacting && cfg->compact_extra[0]) {
            snapshot_len = cJSON_GetArraySize(msgs);
            if (compact_fire(cfg, msgs, compact_res, sizeof(compact_res)) == 0)
                compacting = 1;
        }

        if (!strcmp(resp.finish_reason, "stop")) {
            if (resp.text) { printf("%s\n", resp.text); log_write(log, "ASST", resp.text); }
            response_free(&resp); return 0;   /* msg already transferred; frees finish_reason */
        }
        if (!strcmp(resp.finish_reason, "tool_calls") && resp.tool_calls) {
            process_tool_calls(resp.tool_calls, msgs, log);
            response_free(&resp); continue;
        }
        if (resp.text) { printf("%s\n", resp.text); log_write(log, "ASST", resp.text); }
        response_free(&resp); return 0;
    }
    fprintf(stderr, "error: max turns (%d) reached\n", cfg->max_turns);
    return -1;
}

/* Read one turn from f, terminated by `delim`. Grows as needed (replaces an
   fgets()+4KB buffer that silently truncated long pasted prompts). Returns NULL
   at EOF with no bytes read.

   The delimiter frames turns *without* touching their content: a human at a tty
   uses '\n' (one typed line = one turn); a driving program (non-tty stdin) uses
   '\0', which a turn — being text — can never contain, so a multi-line message
   survives verbatim as a single turn instead of fanning out into one turn (and
   one LLM call) per line. No escaping, no reinterpretation of backslashes. */
static char *read_turn(FILE *f, int delim) {
    size_t cap = 65536, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF && c != delim) {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

#ifndef SZC_TEST
int main(int argc, char **argv) {
    if (argc > 1 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        fprintf(stderr, "SubZeroClaw — skill-driven agentic runtime\n"
            "Usage: subzeroclaw [\"prompt\"]\nConfig: ~/.subzeroclaw/config\n");
        return 0;
    }
    Config cfg;
    if (config_load(&cfg)) return 1;
    char *sysprompt = agent_build_system_prompt(cfg.skills_dir);
    if (!sysprompt) return 1;

    char sid[32] = {0};
    { FILE *ur = fopen("/dev/urandom", "r");
      if (ur) { unsigned char b[8];
          if (fread(b, 1, 8, ur) == 8)
              snprintf(sid, sizeof(sid), "%02x%02x%02x%02x%02x%02x%02x%02x",
                  b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
          fclose(ur); }
      if (!sid[0]) snprintf(sid, sizeof(sid), "%lx%x", (long)time(NULL), getpid()); }
    snprintf(cfg.session, MAX_VALUE, "%s", sid);  /* sent on every request for cache affinity */
    mkdirp(cfg.log_dir);
    char lp[600]; snprintf(lp, sizeof(lp), "%s/%s.txt", cfg.log_dir, sid);
    FILE *log = fopen(lp, "a");
    if (log) { time_t now = time(NULL); fprintf(log, "=== %s %s", sid, ctime(&now)); fflush(log); }

    cJSON *msgs = cJSON_CreateArray();
    cJSON_AddItemToArray(msgs, make_msg("system", sysprompt));
    cJSON *tools = cJSON_Parse(TOOLS_JSON);
    int rc = 0;
    fprintf(stderr, "subzeroclaw · %s\n", sid);

    if (argc > 1) {
        char input[4096], *p = input, *end = input + sizeof(input) - 1;
        for (int i = 1; i < argc && p < end; i++) {
            if (i > 1) *p++ = ' ';
            size_t l = strlen(argv[i]), a = end - p; if (l > a) l = a;
            memcpy(p, argv[i], l); p += l;
        }
        *p = '\0';
        rc = agent_run(&cfg, msgs, tools, input, log);
    } else {
        /* A human at a tty ends a turn with Enter ('\n'); a driving program
           (pipe/FIFO) ends each turn with a NUL byte, letting one turn carry
           newlines verbatim instead of fanning out per line (see read_turn). */
        int delim = isatty(STDIN_FILENO) ? '\n' : '\0';
        for (;;) {
            printf("> "); fflush(stdout);
            char *input = read_turn(stdin, delim);
            if (!input) break;
            if (!input[0]) { free(input); continue; }
            if (!strcmp(input, "/quit") || !strcmp(input, "/exit")) {
                free(input); break;
            }
            agent_run(&cfg, msgs, tools, input, log);
            free(input);
            printf("\n<<TURN_COMPLETE>>\n");
            fflush(stdout);
        }
    }
    cJSON_Delete(msgs); cJSON_Delete(tools); free(sysprompt);
    if (log) fclose(log);
    return rc;
}
#endif
