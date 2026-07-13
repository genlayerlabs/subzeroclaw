#define SZC_TEST
#include "subzeroclaw.c"
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  %-40s ", name);
#define PASS() do { printf("✓\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("✗ %s\n", msg); tests_failed++; } while(0)

/* ======== TOOL TESTS ======== */

static void test_shell_echo(void) {
    TEST("shell: echo");
    char *r = tool_execute("shell", "echo hello_subzeroclaw");
    assert(r);
    if (strstr(r, "hello_subzeroclaw")) PASS();
    else FAIL(r);
    free(r);
}

static void test_shell_pipe(void) {
    TEST("shell: pipe + grep");
    char *r = tool_execute("shell", "echo abc123def | grep -o '[0-9]\\+'");
    assert(r);
    if (strstr(r, "123")) PASS();
    else FAIL(r);
    free(r);
}

static void test_shell_stderr(void) {
    TEST("shell: captures stderr");
    char *r = tool_execute("shell", "LC_ALL=C ls /nonexistent_path_xyz");
    assert(r);
    if (strstr(r, "No such file") || strstr(r, "cannot access") || strstr(r, "error")) PASS();
    else FAIL(r);
    free(r);
}

static void test_unknown_tool(void) {
    TEST("unknown tool returns error");
    char *r = tool_execute("teleport", "mars");
    assert(r);
    if (strstr(r, "unknown tool")) PASS();
    else FAIL(r);
    free(r);
}

static void test_shell_bad_args(void) {
    TEST("shell: missing command");
    char *r = tool_execute("shell", NULL);
    assert(r);
    if (strstr(r, "error")) PASS();
    else FAIL(r);
    free(r);
}

static void test_shell_heredoc(void) {
    TEST("shell: heredoc not corrupted by wrap");
    char *r = tool_execute("shell", "cat <<'END'\nhello_heredoc\nEND");
    assert(r);
    if (strstr(r, "hello_heredoc")) PASS();
    else FAIL(r);
    free(r);
}

static void test_shell_exit_code_ok(void) {
    TEST("shell: exit code prefix on success");
    char *r = tool_execute("shell", "true");
    assert(r);
    if (strstr(r, "[exit:0]") == r) PASS();
    else FAIL(r);
    free(r);
}

static void test_shell_exit_code_fail(void) {
    TEST("shell: exit code prefix on failure");
    char *r = tool_execute("shell", "exit 42");
    assert(r);
    if (strstr(r, "[exit:42]") == r) PASS();
    else FAIL(r);
    free(r);
}

static void test_shell_large_output_is_drained(void) {
    TEST("shell: output beyond cap is drained without blocking");
    char *r = tool_execute("shell",
        "dd if=/dev/zero bs=1024 count=256 2>/dev/null | tr '\\000' x");
    int ok = r && !strncmp(r, "[exit:0] ", 9) && strlen(r) < MAX_OUTPUT + 16;
    if (ok) PASS(); else FAIL("large output did not finish within the bounded result");
    free(r);
}

static void test_http_post_uses_anonymous_pipes(void) {
    TEST("http_post: curl argv clean, config/body piped");
    char dir[] = "/tmp/subzeroclaw-curl-XXXXXX";
    char *made = mkdtemp(dir);
    char curl_path[1024];
    snprintf(curl_path, sizeof(curl_path), "%s/curl", made ? made : "");
    FILE *script = made ? fopen(curl_path, "w") : NULL;
    if (script) {
        fputs("#!/bin/sh\n"
              "case \"$*\" in *router.invalid/fail*) exit 22;; esac\n"
              "printf 'ARGV:'\n"
              "for arg do printf '[%s]' \"$arg\"; done\n"
              "printf '\\nCONFIG:\\n'\n"
              "prev=\nconfig=\n"
              "for arg do\n"
              "  if [ \"$prev\" = '--config' ]; then config=$arg; fi\n"
              "  prev=$arg\n"
              "done\n"
              "cat \"$config\"\n"
              "dd if=/dev/zero bs=1024 count=128 2>/dev/null | tr '\\000' x\n"
              "printf 'BODY:\\n'\n"
              "cat\n", script);
        fclose(script);
        chmod(curl_path, 0700);
    }

    const char *old_path_value = getenv("PATH");
    char *old_path = old_path_value ? strdup(old_path_value) : NULL;
    char path[4096];
    snprintf(path, sizeof(path), "%s:%s", dir, old_path ? old_path : "");
    if (script) setenv("PATH", path, 1);

    const char *key = "test-secret-key";
    const char *session = "test-session";
    char *body = malloc(256 * 1024 + 1);
    if (body) { memset(body, 'b', 256 * 1024); body[256 * 1024] = '\0'; }
    char *response = script && body ? http_post("https://router.invalid/v1/compact",
                                                key, session, body) : NULL;
    char *argv_end = response ? strchr(response, '\n') : NULL;
    size_t argv_len = argv_end ? (size_t)(argv_end - response) : 0;
    char *argv = argv_end ? strndup(response, argv_len) : NULL;
    const char *expected_config =
        "CONFIG:\nheader = \"Authorization: Bearer test-secret-key\"\n"
        "header = \"X-Unhardcoded-Session: test-session\"\n";
    char *body_capture = response ? strstr(response, "BODY:\n") : NULL;
    int captured = response && argv && argv_end &&
        !strncmp(argv_end + 1, expected_config, strlen(expected_config)) &&
        body_capture && !strcmp(body_capture + strlen("BODY:\n"), body);
    int clean_argv = argv && body && strstr(argv, "[--fail]") &&
        strstr(argv, "[-m][120]") && !strstr(argv, "--fail-with-body") &&
        strstr(argv, "[--config][/dev/fd/") &&
        strstr(argv, "[--data-binary][@-]") && !strstr(argv, key) &&
        !strstr(argv, session) && !strstr(argv, body) &&
        !strstr(argv, "Authorization") && !strstr(argv, "/tmp/");
    char *bad_newline = script ? http_post("https://router.invalid", "bad\nkey",
                                            session, body) : NULL;
    char *bad_quote = script ? http_post("https://router.invalid", key,
                                          "bad\"session", body) : NULL;
    char *curl_failed = script ? http_post("https://router.invalid/fail", key,
                                           session, body) : NULL;
    int rejected = !bad_newline && !bad_quote && !curl_failed;

    if (old_path) { setenv("PATH", old_path, 1); free(old_path); }
    else unsetenv("PATH");
    free(bad_newline); free(bad_quote); free(curl_failed);
    free(argv); free(response); free(body);
    if (script) unlink(curl_path);
    if (made) rmdir(dir);
    if (captured && clean_argv && rejected) PASS();
    else FAIL("curl transport exposed data, changed flags, or failed capture");
}

/* ======== STDIN TURN FRAMING TESTS ======== */

/* A turn is framed by a delimiter, not by escaping its content: '\0' for a
   driving program (pipe/FIFO), '\n' for a human at a tty. read_turn must hand
   back the bytes between delimiters completely verbatim. Regression guard for
   the restart-flood bug where a multi-line preamble fanned out into one LLM
   turn (and one reply) per line. */
static void test_turn_nul_keeps_newlines_one_turn(void) {
    TEST("read_turn: NUL-framed multi-line stays one turn");
    /* one turn carrying real newlines, ended by NUL, then a second turn */
    FILE *f = fmemopen("line1\nline2\nline3\0second", 24, "r");
    char *a = read_turn(f, '\0');
    char *b = read_turn(f, '\0');
    if (a && b && strcmp(a, "line1\nline2\nline3") == 0
              && strcmp(b, "second") == 0) PASS();
    else FAIL(a ? a : "(null)");
    free(a); free(b); if (f) fclose(f);
}

static void test_turn_content_verbatim(void) {
    TEST("read_turn: backslashes passed through verbatim");
    /* "a\nb" as typed (backslash, n) must NOT become a newline */
    FILE *f = fmemopen("a\\nb\0", 5, "r");
    char *r = read_turn(f, '\0');
    if (r && strcmp(r, "a\\nb") == 0) PASS();   /* a, backslash, n, b */
    else FAIL(r ? r : "(null)");
    free(r); if (f) fclose(f);
}

static void test_turn_newline_framed_tty(void) {
    TEST("read_turn: newline-framed turns (tty path)");
    FILE *f = fmemopen("first\nsecond\n", 13, "r");
    char *a = read_turn(f, '\n');
    char *b = read_turn(f, '\n');
    if (a && b && strcmp(a, "first") == 0 && strcmp(b, "second") == 0) PASS();
    else FAIL(a ? a : "(null)");
    free(a); free(b); if (f) fclose(f);
}

static void test_turn_eof_empty_is_null(void) {
    TEST("read_turn: EOF with no bytes returns NULL");
    /* fmemopen(..., 0, ...) may legally return NULL (notably on macOS), while
       an empty tmpfile has the exact EOF behavior this test needs. */
    FILE *f = tmpfile();
    if (!f) { FAIL("tmpfile failed"); return; }
    char *r = read_turn(f, '\0');
    if (r == NULL) PASS();
    else FAIL(r);
    free(r); if (f) fclose(f);
}

/* ======== JSON / TOOLS DEFINITION TESTS ======== */

static void test_tools_definitions(void) {
    TEST("TOOLS_JSON structure");
    cJSON *tools = cJSON_Parse(TOOLS_JSON);
    assert(tools);
    int n = cJSON_GetArraySize(tools);
    if (n == 1) PASS();
    else { char m[64]; snprintf(m, 64, "expected 1 tool, got %d", n); FAIL(m); }

    /* verify shell tool structure */
    cJSON *t0 = cJSON_GetArrayItem(tools, 0);
    cJSON *fn = cJSON_GetObjectItem(t0, "function");
    cJSON *name = cJSON_GetObjectItem(fn, "name");
    assert(name && strcmp(name->valuestring, "shell") == 0);

    cJSON_Delete(tools);
}

/* ======== RESPONSE PARSING TESTS ======== */

static void test_parse_stop_response(void) {
    TEST("parse_response: stop");
    const char *mock = "{"
        "\"choices\": [{"
        "  \"finish_reason\": \"stop\","
        "  \"message\": {"
        "    \"role\": \"assistant\","
        "    \"content\": \"Hello from the forest!\","
        "    \"tool_calls\": null"
        "  }"
        "}]"
        "}";
    Response resp;
    int rc = parse_response(mock, &resp);
    if (rc == 0 &&
        !resp.tool_round && !resp.runnable &&
        resp.text && strstr(resp.text, "forest") &&
        resp.tool_calls == NULL)
        PASS();
    else
        FAIL("parse mismatch");
    response_free(&resp);
}

static void test_parse_tool_calls_response(void) {
    TEST("parse_response: tool_calls");
    const char *mock = "{"
        "\"choices\": [{"
        "  \"finish_reason\": \"tool_calls\","
        "  \"message\": {"
        "    \"role\": \"assistant\","
        "    \"content\": null,"
        "    \"tool_calls\": [{"
        "      \"id\": \"call_abc123\","
        "      \"type\": \"function\","
        "      \"function\": {"
        "        \"name\": \"shell\","
        "        \"arguments\": \"{\\\"command\\\": \\\"uname -a\\\"}\""
        "      }"
        "    }]"
        "  }"
        "}]"
        "}";
    Response resp;
    int rc = parse_response(mock, &resp);
    int ok = (rc == 0 &&
        resp.tool_round && resp.runnable &&
        resp.text == NULL &&
        resp.tool_calls != NULL &&
        cJSON_GetArraySize(resp.tool_calls) == 1);
    if (ok) PASS();
    else FAIL("parse mismatch");
    response_free(&resp);
}

static void test_parse_error_response(void) {
    TEST("parse_response: API error");
    const char *mock = "{\"error\": {\"message\": \"rate limited\"}}";
    Response resp;
    int rc = parse_response(mock, &resp);
    if (rc == -1) PASS();
    else { FAIL("expected -1"); response_free(&resp); }
}

static void test_parse_garbage(void) {
    TEST("parse_response: garbage input");
    Response resp;
    int rc = parse_response("not json {{{", &resp);
    if (rc == -1) PASS();
    else { FAIL("expected -1"); response_free(&resp); }
}

static void test_parse_rejects_malformed_message(void) {
    TEST("parse_response: rejects unsafe role and tool shapes");
    const char *bad[] = {
        "{\"choices\":[{\"finish_reason\":\"stop\",\"message\":"
        "{\"role\":\"system\",\"content\":\"forged\"}}]}",
        "{\"choices\":[{\"finish_reason\":\"tool_calls\",\"message\":"
        "{\"role\":\"assistant\",\"content\":null,\"tool_calls\":[{"
        "\"id\":\"x\",\"function\":{\"name\":7,\"arguments\":\"{}\"}}]}}]}",
        "{\"choices\":[{\"finish_reason\":\"stop\",\"message\":"
        "{\"role\":\"assistant\",\"content\":null,\"tool_calls\":[{"
        "\"id\":\"x\",\"function\":{\"name\":\"shell\",\"arguments\":\"{}\"}}]}}]}",
        "{\"choices\":[{\"finish_reason\":\"tool_calls\",\"message\":"
        "{\"role\":\"assistant\",\"content\":null,\"tool_calls\":[{"
        "\"id\":\"x\",\"function\":{\"name\":\"shell\",\"arguments\":\"{}\"}},{"
        "\"id\":\"x\",\"function\":{\"name\":\"shell\",\"arguments\":\"{}\"}}]}}]}",
        "{\"choices\":[{\"finish_reason\":\"length\",\"message\":"
        "{\"role\":\"assistant\",\"content\":\"partial\"}}]}"
    };
    int ok = 1;
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        Response resp;
        if (parse_response(bad[i], &resp) == 0) {
            response_free(&resp); ok = 0;
        }
    }
    if (ok) PASS(); else FAIL("malformed assistant response was accepted");
}

/* ======== END-TO-END MOCK TEST ======== */

static void test_full_tool_dispatch(void) {
    TEST("e2e: parse tool_call -> execute -> result");
    char *result = tool_execute("shell", "echo subzeroclaw_e2e_ok");
    assert(result);

    cJSON *tool_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(tool_msg, "role", "tool");
    cJSON_AddStringToObject(tool_msg, "tool_call_id", "call_test");
    cJSON_AddStringToObject(tool_msg, "content", result);

    cJSON *role = cJSON_GetObjectItem(tool_msg, "role");
    cJSON *content = cJSON_GetObjectItem(tool_msg, "content");

    if (strcmp(role->valuestring, "tool") == 0 &&
        strstr(content->valuestring, "subzeroclaw_e2e_ok"))
        PASS();
    else
        FAIL("e2e mismatch");

    cJSON_Delete(tool_msg);
    free(result);
}

/* ======== MALFORMED TOOL-CALL TESTS ======== */

/* Build one tool_call object: {"id":..,"function":{"name":"shell","arguments":<args_json>}}.
   `arguments` is the model-supplied JSON string (subzeroclaw cJSON_Parses it). */
static cJSON *mk_tool_call(const char *id, const char *args_json) {
    cJSON *tc = cJSON_CreateObject();
    cJSON_AddStringToObject(tc, "id", id);
    cJSON *fn = cJSON_CreateObject();
    cJSON_AddStringToObject(fn, "name", "shell");
    cJSON_AddStringToObject(fn, "arguments", args_json);
    cJSON_AddItemToObject(tc, "function", fn);
    return tc;
}

static void test_valid_tool_calls_marks_runnable(void) {
    TEST("valid_tool_calls: validates first, marks runnable rounds");
    cJSON *valid = cJSON_CreateArray();
    cJSON_AddItemToArray(valid, mk_tool_call("c1", "{\"command\":\"echo hi\"}"));
    cJSON *empty = cJSON_CreateArray();
    cJSON_AddItemToArray(empty, mk_tool_call("c1", ""));                /* arguments "" — the real bug */
    cJSON_AddItemToArray(empty, mk_tool_call("c2", "{}"));             /* no command key */
    cJSON *mixed = cJSON_CreateArray();
    cJSON_AddItemToArray(mixed, mk_tool_call("c1", ""));               /* empty */
    cJSON_AddItemToArray(mixed, mk_tool_call("c2", "{\"command\":\"ls\"}")); /* valid */
    cJSON *blank = cJSON_CreateArray();
    cJSON_AddItemToArray(blank, mk_tool_call("c1", "{\"command\":\"\"}"));   /* blank command string */
    cJSON *none = cJSON_CreateArray();
    int v = 0, e = 0, m = 0, b = 0, n = 0;
    int ok =
        valid_tool_calls(valid, &v) && v &&
        valid_tool_calls(empty, &e) && !e &&
        valid_tool_calls(mixed, &m) && !m &&
        valid_tool_calls(blank, &b) && !b &&
        valid_tool_calls(none, &n) && !n;
    if (ok) PASS(); else FAIL("tool-call validation/runnable mismatch");
    cJSON_Delete(valid); cJSON_Delete(empty); cJSON_Delete(mixed);
    cJSON_Delete(blank); cJSON_Delete(none);
}

/* A runnable multi-call round must pair every tool_call_id before the next
   request. Semantically invalid mixed rounds are discarded by the preflight. */
static void test_recorded_round_pairs_every_call(void) {
    TEST("recorded multi-call round: every tool_call_id paired (no orphan)");
    cJSON *tool_calls = cJSON_CreateArray();
    cJSON_AddItemToArray(tool_calls, mk_tool_call("c1", "{\"command\":\"true\"}"));
    cJSON_AddItemToArray(tool_calls, mk_tool_call("c2", "{\"command\":\"echo ok\"}"));
    cJSON *msgs = cJSON_CreateArray();
    process_tool_calls(tool_calls, msgs, NULL);

    int n = cJSON_GetArraySize(msgs), seen_c1 = 0, seen_c2 = 0, all_tool = 1;
    for (int i = 0; i < n; i++) {
        cJSON *m = cJSON_GetArrayItem(msgs, i);
        cJSON *role = cJSON_GetObjectItem(m, "role");
        cJSON *tcid = cJSON_GetObjectItem(m, "tool_call_id");
        if (!role || !cJSON_IsString(role) || strcmp(role->valuestring, "tool")) all_tool = 0;
        if (tcid && cJSON_IsString(tcid) && !strcmp(tcid->valuestring, "c1")) seen_c1 = 1;
        if (tcid && cJSON_IsString(tcid) && !strcmp(tcid->valuestring, "c2")) seen_c2 = 1;
    }
    if (n == 2 && all_tool && seen_c1 && seen_c2) PASS();
    else FAIL("orphaned tool_call_id (multi-call round not fully paired)");

    cJSON_Delete(tool_calls);
    cJSON_Delete(msgs);
}

/* ======== SYSTEM PROMPT / SKILLS TEST ======== */

static void test_system_prompt(void) {
    TEST("system prompt builds without crash");
    char *p = agent_build_system_prompt("/nonexistent_dir");
    assert(p);
    if (strstr(p, "SubZeroClaw")) PASS();
    else FAIL("missing base prompt");
    free(p);
}

static void test_skills_loading(void) {
    TEST("skills: loads .md files into prompt");
    system("mkdir -p /tmp/szc_skills");
    FILE *f = fopen("/tmp/szc_skills/email.md", "w");
    fprintf(f, "You can use himalaya for email.\n");
    fclose(f);

    char *p = agent_build_system_prompt("/tmp/szc_skills");
    assert(p);
    if (strstr(p, "himalaya")) PASS();
    else FAIL("skill not loaded");
    free(p);
    system("rm -rf /tmp/szc_skills");
}

/* ======== REQUEST BUILDING TESTS ======== */

static void test_build_request(void) {
    TEST("build_request: JSON structure + session, no hardcoded model");
    Config cfg;
    memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.session, MAX_VALUE, "sid-123");
    cJSON *msgs = cJSON_CreateArray();
    cJSON_AddItemToArray(msgs, make_msg("system", "hello"));
    cJSON *tools = cJSON_Parse(TOOLS_JSON);
    char *json = build_request(&cfg, msgs, tools);
    assert(json);
    cJSON *req = cJSON_Parse(json);
    assert(req);
    cJSON *session = cJSON_GetObjectItem(req, "session");
    cJSON *model = cJSON_GetObjectItem(req, "model");
    cJSON *m = cJSON_GetObjectItem(req, "messages");
    cJSON *t = cJSON_GetObjectItem(req, "tools");
    if (session && strcmp(session->valuestring, "sid-123") == 0 &&
        !model &&                       /* the agent ships no model; it rides in REQUEST_EXTRA */
        m && cJSON_GetArraySize(m) == 1 &&
        t && cJSON_GetArraySize(t) == 1)
        PASS();
    else
        FAIL("request structure mismatch");
    cJSON_Delete(req); free(json);
    cJSON_Delete(msgs); cJSON_Delete(tools);
}

static void test_build_request_extra_merge(void) {
    TEST("build_request: REQUEST_EXTRA cannot replace runtime context");
    Config cfg; memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.session, MAX_VALUE, "runtime-session");
    snprintf(cfg.request_extra, MAX_EXTRA,
        "{\"temperature\":0.5,\"model\":\"override-model\","
        "\"session\":\"forged\",\"messages\":[],\"tools\":[],"
        "\"Session\":\"forged-case\",\"MESSAGES\":[],\"Tools\":[]}");
    cJSON *msgs = cJSON_CreateArray();
    cJSON_AddItemToArray(msgs, make_msg("user", "hi"));
    cJSON *tools = cJSON_Parse(TOOLS_JSON);
    char *json = build_request(&cfg, msgs, tools);
    assert(json);
    cJSON *req = cJSON_Parse(json); assert(req);
    cJSON *temp  = cJSON_GetObjectItem(req, "temperature");
    cJSON *model = cJSON_GetObjectItem(req, "model");
    cJSON *session = cJSON_GetObjectItem(req, "session");
    cJSON *m     = cJSON_GetObjectItem(req, "messages");
    cJSON *t     = cJSON_GetObjectItem(req, "tools");
    int model_keys = 0; cJSON *c = NULL;
    cJSON_ArrayForEach(c, req) if (c->string && !strcmp(c->string, "model")) model_keys++;
    if (temp && cJSON_IsNumber(temp) && temp->valuedouble == 0.5 &&
        model && !strcmp(model->valuestring, "override-model") && model_keys == 1 &&
        session && !strcmp(session->valuestring, "runtime-session") &&
        !cJSON_GetObjectItemCaseSensitive(req, "Session") &&
        !cJSON_GetObjectItemCaseSensitive(req, "MESSAGES") &&
        !cJSON_GetObjectItemCaseSensitive(req, "Tools") &&
        m && cJSON_GetArraySize(m) == 1 && t && cJSON_GetArraySize(t) == 1)
        PASS();
    else FAIL("request_extra replaced a runtime-owned field");
    cJSON_Delete(req); free(json); cJSON_Delete(msgs); cJSON_Delete(tools);
}

static void test_build_request_extra_empty(void) {
    TEST("build_request: empty REQUEST_EXTRA leaves body unchanged");
    Config cfg; memset(&cfg, 0, sizeof(cfg));
    cJSON *msgs = cJSON_CreateArray();
    char *json = build_request(&cfg, msgs, NULL);
    cJSON *req = cJSON_Parse(json); assert(req);
    if (cJSON_GetArraySize(req) == 1 &&        /* exactly messages (no session, no model) */
        cJSON_GetObjectItem(req, "messages"))
        PASS();
    else FAIL("body changed when unset");
    cJSON_Delete(req); free(json); cJSON_Delete(msgs);
}

static void test_build_request_extra_garbage(void) {
    TEST("build_request: malformed REQUEST_EXTRA ignored");
    Config cfg; memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.request_extra, MAX_EXTRA, "{not valid json");
    cJSON *msgs = cJSON_CreateArray();
    char *json = build_request(&cfg, msgs, NULL);
    cJSON *req = cJSON_Parse(json);            /* request must still be valid */
    if (req && cJSON_GetObjectItem(req, "messages"))
        PASS();
    else FAIL("garbage broke the request");
    if (req) cJSON_Delete(req); free(json); cJSON_Delete(msgs);
}


/* ======== CONFIG TESTS ======== */

static void test_config_no_key(void) {
    TEST("config: fails without API key");
    unsetenv("SUBZEROCLAW_API_KEY");
    unsetenv("SUBZEROCLAW_ENDPOINT");
    Config cfg;
    /* point config at nonexistent file so file parsing is skipped */
    char *old_home = getenv("HOME") ? strdup(getenv("HOME")) : NULL;
    setenv("HOME", "/tmp/szc_no_config", 1);
    int rc = config_load(&cfg);
    if (old_home) { setenv("HOME", old_home, 1); free(old_home); }
    else unsetenv("HOME");
    if (rc == -1) PASS();
    else FAIL("expected failure");
}

static void test_config_defaults(void) {
    TEST("config: loads with env var");
    setenv("SUBZEROCLAW_API_KEY", "sk-test-fake-key", 1);
    Config cfg;
    int rc = config_load(&cfg);
    if (rc == 0 &&
        strcmp(cfg.api_key, "sk-test-fake-key") == 0 &&
        strstr(cfg.endpoint, "openrouter.ai"))
        PASS();
    else
        FAIL("config mismatch");
    unsetenv("SUBZEROCLAW_API_KEY");
}

static void test_config_scrubs_env(void) {
    TEST("config: scrubs provider env after load");
    char *old_home = getenv("HOME") ? strdup(getenv("HOME")) : NULL;
    setenv("HOME", "/tmp/szc_no_config", 1); /* skip any real config file */
    setenv("SUBZEROCLAW_API_KEY", "sk-test-scrub", 1);
    setenv("SUBZEROCLAW_ENDPOINT", "https://test.example/v1/chat", 1);
    setenv("SUBZEROCLAW_REQUEST_EXTRA", "{\"temperature\":0}", 1);
    setenv("SUBZEROCLAW_COMPACT_EXTRA", "{\"legacy_secret\":true}", 1);
    Config cfg;
    int rc = config_load(&cfg);
    if (old_home) { setenv("HOME", old_home, 1); free(old_home); }
    else unsetenv("HOME");

    /* values survive in cfg, so requests are unaffected... */
    int cfg_ok = rc == 0 &&
        strcmp(cfg.api_key, "sk-test-scrub") == 0 &&
        strcmp(cfg.endpoint, "https://test.example/v1/chat") == 0 &&
        strcmp(cfg.request_extra, "{\"temperature\":0}") == 0;
    /* ...but are gone from the environment the shell tool inherits */
    int env_scrubbed =
        getenv("SUBZEROCLAW_API_KEY") == NULL &&
        getenv("SUBZEROCLAW_ENDPOINT") == NULL &&
        getenv("SUBZEROCLAW_REQUEST_EXTRA") == NULL &&
        getenv("SUBZEROCLAW_COMPACT_EXTRA") == NULL;

    if (cfg_ok && env_scrubbed) PASS();
    else if (!cfg_ok) FAIL("cfg lost a provider value");
    else FAIL("env not scrubbed after load");
}

/* ======== MAIN ======== */

static void test_parse_compact_flag(void) {
    TEST("parse_response: reads x_router.compact");
    Response r1, r2;
    int ok1 = parse_response(
        "{\"choices\":[{\"finish_reason\":\"stop\",\"message\":{\"role\":\"assistant\",\"content\":\"hi\"}}],"
        "\"x_router\":{\"compact\":true}}", &r1) == 0;
    int ok2 = parse_response(
        "{\"choices\":[{\"finish_reason\":\"stop\",\"message\":{\"role\":\"assistant\",\"content\":\"hi\"}}]}",
        &r2) == 0;
    if (ok1 && ok2 && r1.compact == 1 && r2.compact == 0) PASS();
    else FAIL("compact flag mismatch");
    if (ok1) response_free(&r1);
    if (ok2) response_free(&r2);
}

static cJSON *compaction_fixture(void) {
    char old[900];
    memset(old, 'x', sizeof(old) - 1); old[sizeof(old) - 1] = '\0';
    cJSON *msgs = cJSON_CreateArray();
    cJSON_AddItemToArray(msgs, make_msg("system", "system"));
    cJSON_AddItemToArray(msgs, make_msg("developer", "developer"));
    for (int i = 0; i < 8; i++) {
        cJSON_AddItemToArray(msgs, make_msg("user", old));
        cJSON_AddItemToArray(msgs, make_msg("assistant", old));
    }
    return msgs;
}

static cJSON *compaction_response(const char *summary, int compacted) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "contract_version", 3);
    cJSON_AddBoolToObject(root, "compacted", compacted);
    cJSON_AddStringToObject(root, "reason", compacted ? "compacted" : "declined");
    if (compacted && summary) cJSON_AddStringToObject(root, "summary", summary);
    return root;
}

static int apply_root(cJSON *msgs, int *sealed, cJSON *root, LogSink *log) {
    CompactLayout layout;
    if (compact_layout(msgs, *sealed, &layout) != 1) return -1;
    char *json = cJSON_PrintUnformatted(root);
    int rc = json ? compact_apply_response(msgs, &layout, json, sealed, log) : -1;
    free(json);
    return rc;
}

static void test_compact_applies_local_canonical_array_and_logs(void) {
    TEST("compact: local canonical splice + JSONL evidence");
    cJSON *msgs = compaction_fixture();
    cJSON *before = cJSON_Duplicate(msgs, 1);
    size_t expected_before_bytes = json_size(msgs);
    cJSON *root = compaction_response("SEALED", 1);
    LogSink log = {.jsonl = tmpfile(), .sequence = 0};
    int sealed = 0;
    int rc = apply_root(msgs, &sealed, root, &log);
    cJSON *boundary = cJSON_GetArrayItem(msgs, 2);
    cJSON *summary = cJSON_GetArrayItem(msgs, 3);
    int authority_ok =
        cJSON_Compare(cJSON_GetArrayItem(msgs, 0), cJSON_GetArrayItem(before, 0), 1) &&
        cJSON_Compare(cJSON_GetArrayItem(msgs, 1), cJSON_GetArrayItem(before, 1), 1);
    char line1[4096] = {0}, line2[4096] = {0};
    rewind(log.jsonl);
    fgets(line1, sizeof(line1), log.jsonl);
    fgets(line2, sizeof(line2), log.jsonl);
    cJSON *e1 = cJSON_Parse(line1), *e2 = cJSON_Parse(line2);
    cJSON *r1 = e1 ? cJSON_GetObjectItem(e1, "role") : NULL;
    cJSON *r2 = e2 ? cJSON_GetObjectItem(e2, "role") : NULL;
    cJSON *c1 = e1 ? cJSON_GetObjectItem(e1, "content") : NULL;
    cJSON *c2 = e2 ? cJSON_GetObjectItem(e2, "content") : NULL;
    cJSON *meta = c1 && cJSON_IsString(c1) ? cJSON_Parse(c1->valuestring) : NULL;
    cJSON *event = meta ? cJSON_GetObjectItem(meta, "event") : NULL;
    cJSON *before_bytes = meta ? cJSON_GetObjectItem(meta, "before_bytes") : NULL;
    cJSON *after_bytes = meta ? cJSON_GetObjectItem(meta, "after_bytes") : NULL;
    size_t expected_after_bytes = json_size(msgs);
    int ok = rc == 1 && sealed && cJSON_GetArraySize(msgs) == 16 && authority_ok &&
        !strcmp(message_content(boundary, "user"), SEAL_BOUNDARY) &&
        !strcmp(message_content(summary, "assistant"), SEAL_PREFIX "SEALED") &&
        r1 && !strcmp(r1->valuestring, "COMPACT") &&
        event && !strcmp(event->valuestring, "applied") &&
        cJSON_GetObjectItem(meta, "before_messages") &&
        cJSON_GetObjectItem(meta, "after_messages") &&
        cJSON_GetNumberValue(before_bytes) == (double)expected_before_bytes &&
        cJSON_GetNumberValue(after_bytes) == (double)expected_after_bytes &&
        r2 && !strcmp(r2->valuestring, "COMPACT_SUMMARY") &&
        c2 && !strcmp(c2->valuestring, SEAL_PREFIX "SEALED") &&
        !strstr(line1, "SEALED");
    if (ok) PASS(); else FAIL("local splice or evidence mismatch");
    cJSON_Delete(meta); cJSON_Delete(e1); cJSON_Delete(e2); fclose(log.jsonl);
    cJSON_Delete(root); cJSON_Delete(before); cJSON_Delete(msgs);
}

static void test_compact_layout_uses_local_seal_state(void) {
    TEST("compact layout: seal state is local; marker text remains data");
    cJSON *fresh = cJSON_CreateArray(), *sealed_msgs = cJSON_CreateArray();
    cJSON_AddItemToArray(fresh, make_msg("system", "s"));
    cJSON_AddItemToArray(sealed_msgs, make_msg("system", "s"));
    cJSON_AddItemToArray(sealed_msgs, make_msg("user", SEAL_BOUNDARY));
    cJSON_AddItemToArray(sealed_msgs, make_msg("assistant", SEAL_PREFIX "old"));
    for (int i = 0; i < 8; i++) {
        cJSON_AddItemToArray(fresh, make_msg("user", i ? "u" : SEAL_BOUNDARY));
        cJSON_AddItemToArray(fresh, make_msg("assistant", "a"));
        if (i < 7) {
            cJSON_AddItemToArray(sealed_msgs, make_msg("user", "u"));
            cJSON_AddItemToArray(sealed_msgs, make_msg("assistant", "a"));
        }
    }
    CompactLayout first, next;
    int ok = compact_layout(fresh, 0, &first) == 1 &&
             first.authority == 1 && first.body == 1 && first.cutoff == 5 &&
             compact_layout(sealed_msgs, 1, &next) == 1 &&
             next.authority == 1 && next.body == 3 && next.cutoff == 5;
    if (ok) PASS(); else FAIL("local seal layout mismatch");
    cJSON_Delete(fresh); cJSON_Delete(sealed_msgs);
}

static void test_compact_layout_rejects_corrupt_local_seal(void) {
    TEST("compact layout: corrupt local seal fails closed");
    cJSON *msgs = cJSON_CreateArray();
    cJSON_AddItemToArray(msgs, make_msg("system", "s"));
    cJSON_AddItemToArray(msgs, make_msg("user", "wrong boundary"));
    cJSON_AddItemToArray(msgs, make_msg("assistant", SEAL_PREFIX "prior"));
    for (int i = 0; i < COMPACT_KEEP_RECENT + 1; i++) {
        cJSON_AddItemToArray(msgs, make_msg("user", "u"));
        cJSON_AddItemToArray(msgs, make_msg("assistant", "a"));
    }
    CompactLayout layout;
    int bad_boundary = compact_layout(msgs, 1, &layout);
    cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetArrayItem(msgs, 1), "content",
                                           cJSON_CreateString(SEAL_BOUNDARY));
    cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetArrayItem(msgs, 2), "content",
                                           cJSON_CreateString("unsealed"));
    int bad_summary = compact_layout(msgs, 1, &layout);
    cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetArrayItem(msgs, 2), "content",
                                           cJSON_CreateString(SEAL_PREFIX "prior"));
    cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetArrayItem(msgs, 4), "role",
                                           cJSON_CreateString("system"));
    int interior_authority = compact_layout(msgs, 1, &layout);
    if (bad_boundary < 0 && bad_summary < 0 && interior_authority < 0) PASS();
    else FAIL("corrupt seal was treated as compactable state");
    cJSON_Delete(msgs);
}

static void test_compact_rejects_unsafe_summary(void) {
    TEST("compact: rejects unsafe summary and invalid success reason");
    cJSON *msgs = compaction_fixture(), *before = cJSON_Duplicate(msgs, 1);
    cJSON *missing = compaction_response(NULL, 1);
    cJSON *empty = compaction_response("", 1);
    cJSON *wrong_type = compaction_response(NULL, 1);
    cJSON *wrong_reason = compaction_response("SEALED", 1);
    cJSON *missing_reason = compaction_response("SEALED", 1);
    cJSON_AddNumberToObject(wrong_type, "summary", 7);
    cJSON_ReplaceItemInObject(wrong_reason, "reason", cJSON_CreateString("declined"));
    cJSON_DeleteItemFromObject(missing_reason, "reason");
    int sealed = 0;
    int ok = apply_root(msgs, &sealed, missing, NULL) < 0 &&
             apply_root(msgs, &sealed, empty, NULL) < 0 &&
             apply_root(msgs, &sealed, wrong_type, NULL) < 0 &&
             apply_root(msgs, &sealed, wrong_reason, NULL) < 0 &&
             apply_root(msgs, &sealed, missing_reason, NULL) < 0 && !sealed &&
             cJSON_Compare(msgs, before, 1);
    if (ok) PASS(); else FAIL("unsafe summary changed context");
    cJSON_Delete(missing); cJSON_Delete(empty); cJSON_Delete(wrong_type);
    cJSON_Delete(wrong_reason); cJSON_Delete(missing_reason);
    cJSON_Delete(before); cJSON_Delete(msgs);
}

static void test_compact_accepts_any_positive_reduction(void) {
    TEST("compact: any positive local reduction is accepted");
    cJSON *msgs = compaction_fixture();
    size_t before = json_size(msgs);
    char summary[3001];
    memset(summary, 's', sizeof(summary) - 1); summary[sizeof(summary) - 1] = '\0';
    cJSON *root = compaction_response(summary, 1);
    int sealed = 0;
    int rc = apply_root(msgs, &sealed, root, NULL);
    size_t after = json_size(msgs);
    int ok = rc == 1 && sealed && after < before &&
             (before - after) * 100 < before * 5;
    if (ok) PASS(); else FAIL("positive reduction was rejected or exceeded fixture target");
    cJSON_Delete(root); cJSON_Delete(msgs);
}

static void test_compact_false_and_insufficient_reduction_are_noops(void) {
    TEST("compact: declined/large summary are no-ops");
    cJSON *msgs = compaction_fixture();
    cJSON *before = cJSON_Duplicate(msgs, 1);
    cJSON *declined = compaction_response(NULL, 0);
    cJSON_ReplaceItemInObject(declined, "reason", cJSON_CreateString("REMOTE SECRET"));
    LogSink log = {.jsonl = tmpfile(), .sequence = 0};
    int sealed = 0;
    int declined_rc = apply_root(msgs, &sealed, declined, &log);
    char status[2048] = {0}, extra[8] = {0};
    rewind(log.jsonl);
    fgets(status, sizeof(status), log.jsonl);
    int one_status = fgets(extra, sizeof(extra), log.jsonl) == NULL &&
        strstr(status, "router_declined") && !strstr(status, "REMOTE SECRET");
    char huge[7000];
    memset(huge, 's', sizeof(huge) - 1);
    huge[sizeof(huge) - 1] = '\0';
    cJSON *large = compaction_response(huge, 1);
    int large_rc = apply_root(msgs, &sealed, large, NULL);
    if (declined_rc == 0 && large_rc == 0 && one_status &&
        cJSON_Compare(msgs, before, 1)) PASS();
    else FAIL("no-op response changed context");
    fclose(log.jsonl); cJSON_Delete(large); cJSON_Delete(declined);
    cJSON_Delete(before); cJSON_Delete(msgs);
}

static void test_compact_rejects_wrong_contract(void) {
    TEST("compact: requires integer contract v3");
    cJSON *msgs = compaction_fixture(), *before = cJSON_Duplicate(msgs, 1);
    cJSON *root = compaction_response("SEALED", 1);
    cJSON_ReplaceItemInObject(root, "contract_version", cJSON_CreateNumber(2));
    int sealed = 0;
    int wrong = apply_root(msgs, &sealed, root, NULL);
    cJSON_ReplaceItemInObject(root, "contract_version", cJSON_CreateNumber(2.5));
    int fractional = apply_root(msgs, &sealed, root, NULL);
    if (wrong < 0 && fractional < 0 && cJSON_Compare(msgs, before, 1)) PASS();
    else FAIL("non-v3 response changed context");
    cJSON_Delete(root); cJSON_Delete(before); cJSON_Delete(msgs);
}

static void test_session_id_shape(void) {
    TEST("session id: urandom/fallback shape");
    char sid[32] = {0};
    make_session_id(sid);
    if (sid[0] && strlen(sid) < sizeof(sid)) PASS(); else FAIL("empty session id");
}

static void test_agent_run_compacts_at_completed_seam(void) {
    TEST("agent_run: tool result, compact seam, empty terminal");
    char dir[] = "/tmp/subzeroclaw-seam-XXXXXX";
    char *made = mkdtemp(dir), curl_path[1024], marker_path[1024];
    snprintf(curl_path, sizeof(curl_path), "%s/curl", made ? made : "");
    snprintf(marker_path, sizeof(marker_path), "%s/chat-once", made ? made : "");
    FILE *script = made ? fopen(curl_path, "w") : NULL;
    if (script) {
        fputs("#!/bin/sh\nurl=\nprev=\n"
              "for arg do [ \"$prev\" = '--url' ] && url=$arg; prev=$arg; done\n"
              "cat >/dev/null\n"
              "case \"$url\" in */compact) printf '%s' \"$SZC_TEST_COMPACT_RESPONSE\";;"
              " *) if [ -e \"$SZC_TEST_CHAT_MARKER\" ]; then"
              " printf '%s' \"$SZC_TEST_CHAT_SECOND\"; else"
              " : >\"$SZC_TEST_CHAT_MARKER\"; printf '%s' \"$SZC_TEST_CHAT_FIRST\"; fi;; esac\n",
              script);
        fclose(script); chmod(curl_path, 0700);
    }

    cJSON *msgs = cJSON_CreateArray();
    char old[1024]; memset(old, 'x', sizeof(old) - 1); old[sizeof(old) - 1] = '\0';
    cJSON_AddItemToArray(msgs, make_msg("system", "s"));
    for (int i = 0; i < COMPACT_KEEP_RECENT; i++) {
        cJSON_AddItemToArray(msgs, make_msg("user", old));
        cJSON_AddItemToArray(msgs, make_msg("assistant", old));
    }
    cJSON *compact = cJSON_CreateObject();
    cJSON_AddNumberToObject(compact, "contract_version", 3);
    cJSON_AddBoolToObject(compact, "compacted", 1);
    cJSON_AddStringToObject(compact, "reason", "compacted");
    cJSON_AddStringToObject(compact, "summary", "SEALED");
    char *compact_json = cJSON_PrintUnformatted(compact);
    const char *chat_first =
        "{\"choices\":[{\"finish_reason\":\"tool_calls\",\"message\":"
        "{\"role\":\"assistant\",\"content\":null,\"tool_calls\":[{"
        "\"id\":\"call-1\",\"type\":\"function\",\"function\":{"
        "\"name\":\"shell\",\"arguments\":\"{\\\"command\\\":\\\"true\\\"}\"}}]}}],"
        "\"x_router\":{\"compact\":true}}";
    const char *chat_second =
        "{\"choices\":[{\"finish_reason\":\"stop\",\"message\":"
        "{\"role\":\"assistant\",\"content\":null}}]}";
    if (script && compact_json) {
        setenv("SZC_TEST_CHAT_FIRST", chat_first, 1);
        setenv("SZC_TEST_CHAT_SECOND", chat_second, 1);
        setenv("SZC_TEST_CHAT_MARKER", marker_path, 1);
        setenv("SZC_TEST_COMPACT_RESPONSE", compact_json, 1);
    }
    const char *old_path_value = getenv("PATH");
    char *old_path = old_path_value ? strdup(old_path_value) : NULL;
    char path[4096]; snprintf(path, sizeof(path), "%s:%s", dir, old_path ? old_path : "");
    if (script && compact_json) setenv("PATH", path, 1);

    Config cfg; memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.api_key, MAX_VALUE, "test-key");
    snprintf(cfg.endpoint, MAX_VALUE, "https://router.invalid/v1/chat/completions");
    snprintf(cfg.session, MAX_VALUE, "seam-session");
    cfg.max_turns = 2;
    cJSON *tools = cJSON_Parse(TOOLS_JSON);
    LogSink log = {.jsonl = tmpfile(), .sequence = 0};
    int sealed = 0;
    int rc = script && compact_json
        ? agent_run(&cfg, msgs, tools, "latest", &sealed, &log) : -1;
    char logged[8192] = {0};
    if (log.jsonl) { rewind(log.jsonl); fread(logged, 1, sizeof(logged) - 1, log.jsonl); }
    char *user = strstr(logged, "\"role\":\"USER\"");
    char *tool = strstr(logged, "\"role\":\"TOOL\"");
    char *result = strstr(logged, "\"role\":\"RES\"");
    char *asst = strstr(logged, "\"role\":\"ASST\"");
    char *event = strstr(logged, "\"role\":\"COMPACT\"");
    char *summary = strstr(logged, "\"role\":\"COMPACT_SUMMARY\"");
    int ok = rc == 0 && sealed && cJSON_GetArraySize(msgs) == 17 &&
        !strcmp(message_content(cJSON_GetArrayItem(msgs, 1), "user"), SEAL_BOUNDARY) &&
        !strcmp(message_content(cJSON_GetArrayItem(msgs, 2), "assistant"),
                SEAL_PREFIX "SEALED") &&
        !strcmp(message_role(cJSON_GetArrayItem(msgs, 3)), "user") &&
        user && tool && result && !asst && event && summary &&
        user < tool && tool < result && result < event && event < summary;

    if (old_path) { setenv("PATH", old_path, 1); free(old_path); } else unsetenv("PATH");
    unsetenv("SZC_TEST_CHAT_FIRST"); unsetenv("SZC_TEST_CHAT_SECOND");
    unsetenv("SZC_TEST_CHAT_MARKER"); unsetenv("SZC_TEST_COMPACT_RESPONSE");
    if (log.jsonl) fclose(log.jsonl);
    cJSON_Delete(tools); cJSON_Delete(msgs); cJSON_Delete(compact); free(compact_json);
    if (script) unlink(curl_path); if (made) { unlink(marker_path); rmdir(dir); }
    if (ok) PASS();
    else FAIL("complete tool round with empty terminal was not retained and compacted");
}

static void test_agent_run_rolls_back_unanswered_input(void) {
    TEST("agent_run: unanswered input does not poison history");
    cJSON *msgs = compaction_fixture();
    cJSON *before = cJSON_Duplicate(msgs, 1);
    cJSON *tools = cJSON_Parse(TOOLS_JSON);
    Config cfg; memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.api_key, MAX_VALUE, "test-key");
    snprintf(cfg.endpoint, MAX_VALUE, "https://router.invalid/v1/chat/completions");
    snprintf(cfg.session, MAX_VALUE, "invalid\"session");
    cfg.max_turns = 1;
    int sealed = 0;
    int rc = agent_run(&cfg, msgs, tools, "unanswered", &sealed, NULL);
    CompactLayout layout;
    int ok = rc < 0 && cJSON_Compare(msgs, before, 1) &&
             compact_layout(msgs, sealed, &layout) == 1;
    if (ok) PASS(); else FAIL("failed input remained in compactable history");
    cJSON_Delete(tools); cJSON_Delete(before); cJSON_Delete(msgs);
}

static void test_agent_run_preflights_every_tool_before_side_effects(void) {
    TEST("agent_run: malformed later tool blocks every side effect");
    char dir[] = "/tmp/subzeroclaw-preflight-XXXXXX", curl_path[1024], marker[1024];
    char *made = mkdtemp(dir);
    snprintf(curl_path, sizeof(curl_path), "%s/curl", made ? made : "");
    snprintf(marker, sizeof(marker), "%s/should-not-exist", made ? made : "");
    FILE *script = made ? fopen(curl_path, "w") : NULL;
    if (script) {
        fputs("#!/bin/sh\ncat >/dev/null\nprintf '%s' \"$SZC_TEST_CHAT_RESPONSE\"\n",
              script);
        fclose(script); chmod(curl_path, 0700);
    }

    char response[4096];
    snprintf(response, sizeof(response),
        "{\"choices\":[{\"finish_reason\":\"tool_calls\",\"message\":{"
        "\"role\":\"assistant\",\"content\":null,\"tool_calls\":[{"
        "\"id\":\"valid-first\",\"function\":{\"name\":\"shell\","
        "\"arguments\":\"{\\\"command\\\":\\\"touch %s\\\"}\"}},{"
        "\"id\":\"invalid-second\",\"function\":{\"name\":\"shell\","
        "\"arguments\":\"{\\\"command\\\":\\\"true\\\"}TRAIL\"}}]}}]}", marker);
    setenv("SZC_TEST_CHAT_RESPONSE", response, 1);
    const char *old_path_value = getenv("PATH");
    char *old_path = old_path_value ? strdup(old_path_value) : NULL;
    char path[4096]; snprintf(path, sizeof(path), "%s:%s", dir, old_path ? old_path : "");
    if (script) setenv("PATH", path, 1);

    Config cfg; memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.api_key, MAX_VALUE, "test-key");
    snprintf(cfg.endpoint, MAX_VALUE, "https://router.invalid/v1/chat/completions");
    cfg.max_turns = 1;
    cJSON *msgs = cJSON_CreateArray(), *tools = cJSON_Parse(TOOLS_JSON);
    cJSON_AddItemToArray(msgs, make_msg("system", "s"));
    int sealed = 0;
    int rc = script ? agent_run(&cfg, msgs, tools, "request", &sealed, NULL) : 0;
    int ok = rc < 0 && access(marker, F_OK) != 0 && cJSON_GetArraySize(msgs) == 1;

    if (old_path) { setenv("PATH", old_path, 1); free(old_path); } else unsetenv("PATH");
    unsetenv("SZC_TEST_CHAT_RESPONSE");
    cJSON_Delete(tools); cJSON_Delete(msgs);
    if (script) unlink(curl_path); if (made) { unlink(marker); rmdir(dir); }
    if (ok) PASS(); else FAIL("a tool ran before the full round was validated");
}

static void test_agent_run_rolls_back_empty_terminal(void) {
    TEST("agent_run: empty terminal response does not answer input");
    char dir[] = "/tmp/subzeroclaw-empty-XXXXXX", curl_path[1024];
    char *made = mkdtemp(dir);
    snprintf(curl_path, sizeof(curl_path), "%s/curl", made ? made : "");
    FILE *script = made ? fopen(curl_path, "w") : NULL;
    if (script) {
        fputs("#!/bin/sh\ncat >/dev/null\nprintf '%s' \"$SZC_TEST_CHAT_RESPONSE\"\n",
              script);
        fclose(script); chmod(curl_path, 0700);
    }
    const char *old_path_value = getenv("PATH");
    char *old_path = old_path_value ? strdup(old_path_value) : NULL;
    char path[4096];
    snprintf(path, sizeof(path), "%s:%s", dir, old_path ? old_path : "");
    if (script) setenv("PATH", path, 1);

    const char *responses[] = {
        "{\"choices\":[{\"finish_reason\":\"stop\",\"message\":{\"role\":\"assistant\",\"content\":null}}]}",
        "{\"choices\":[{\"finish_reason\":\"stop\",\"message\":{\"role\":\"assistant\",\"content\":\"\"}}]}",
        "{\"choices\":[{\"finish_reason\":\"stop\",\"message\":{\"role\":\"assistant\",\"content\":\" \\n\\t\"}}]}"
    };
    int ok = script != NULL;
    for (size_t i = 0; ok && i < sizeof(responses) / sizeof(responses[0]); i++) {
        cJSON *msgs = compaction_fixture(), *before = cJSON_Duplicate(msgs, 1);
        cJSON *tools = cJSON_Parse(TOOLS_JSON);
        Config cfg; memset(&cfg, 0, sizeof(cfg));
        snprintf(cfg.api_key, MAX_VALUE, "test-key");
        snprintf(cfg.endpoint, MAX_VALUE, "https://router.invalid/v1/chat/completions");
        snprintf(cfg.session, MAX_VALUE, "empty-session");
        cfg.max_turns = 1;
        int sealed = 0;
        setenv("SZC_TEST_CHAT_RESPONSE", responses[i], 1);
        int rc = agent_run(&cfg, msgs, tools, "unanswered", &sealed, NULL);
        ok = rc < 0 && cJSON_Compare(msgs, before, 1);
        cJSON_Delete(tools); cJSON_Delete(before); cJSON_Delete(msgs);
    }

    if (old_path) { setenv("PATH", old_path, 1); free(old_path); } else unsetenv("PATH");
    unsetenv("SZC_TEST_CHAT_RESPONSE");
    if (script) unlink(curl_path); if (made) rmdir(dir);
    if (ok) PASS(); else FAIL("empty terminal response remained in history");
}

static void test_second_compaction_merges_previous_summary(void) {
    TEST("compact: second pass replaces prior semantic memory");
    cJSON *msgs = compaction_fixture();
    int sealed = 0;
    cJSON *first = compaction_response("FIRST", 1);
    int first_rc = apply_root(msgs, &sealed, first, NULL);
    for (int i = 0; i < 2; i++) {
        cJSON_AddItemToArray(msgs, make_msg("user", "new user"));
        cJSON_AddItemToArray(msgs, make_msg("assistant", "new answer"));
    }
    CompactLayout layout;
    char *body = compact_layout(msgs, sealed, &layout) == 1
        ? compact_request_body(msgs, sealed, &layout) : NULL;
    cJSON *request = body ? cJSON_Parse(body) : NULL;
    cJSON *previous = request ? cJSON_GetObjectItem(request, "previous_summary") : NULL;
    cJSON *second = compaction_response("MERGED", 1);
    int second_rc = apply_root(msgs, &sealed, second, NULL);
    const char *summary = message_content(cJSON_GetArrayItem(msgs, 3), "assistant");
    int ok = first_rc == 1 && second_rc == 1 && previous &&
             !strcmp(previous->valuestring, "FIRST") && summary &&
             !strcmp(summary, SEAL_PREFIX "MERGED");
    if (ok) PASS(); else FAIL("prior summary was not merged and replaced");
    cJSON_Delete(second); cJSON_Delete(request); free(body);
    cJSON_Delete(first); cJSON_Delete(msgs);
}

static void test_compact_request_sends_only_raw_prior_and_aged_history(void) {
    TEST("compact request: raw prior + aged only; authority/boundary/tail stay local");
    cJSON *msgs = cJSON_CreateArray();
    cJSON_AddItemToArray(msgs, make_msg("system", "authority"));
    cJSON_AddItemToArray(msgs, make_msg("user", SEAL_BOUNDARY));
    cJSON_AddItemToArray(msgs, make_msg("assistant", SEAL_PREFIX "prior"));
    for (int i = 0; i < COMPACT_KEEP_RECENT + 1; i++) {
        cJSON_AddItemToArray(msgs, make_msg("user", i ? "recent" : "aged"));
        cJSON_AddItemToArray(msgs, make_msg("assistant", i ? "new answer" : "old answer"));
    }
    CompactLayout layout;
    int ready = compact_layout(msgs, 1, &layout) == 1;
    char *body = ready ? compact_request_body(msgs, 1, &layout) : NULL;
    cJSON *req = body ? cJSON_Parse(body) : NULL;
    cJSON *aged = req ? cJSON_GetObjectItem(req, "messages") : NULL;
    cJSON *previous = req ? cJSON_GetObjectItem(req, "previous_summary") : NULL;
    int ok = cJSON_GetNumberValue(cJSON_GetObjectItem(req, "contract_version")) == 3 &&
             cJSON_IsString(previous) && !strcmp(previous->valuestring, "prior") &&
             cJSON_IsArray(aged) && cJSON_GetArraySize(aged) == 2 &&
             !strcmp(message_content(cJSON_GetArrayItem(aged, 0), "user"), "aged") &&
             !strcmp(message_content(cJSON_GetArrayItem(aged, 1), "assistant"), "old answer") &&
             !cJSON_GetObjectItem(req, "max_tokens") &&
             !cJSON_GetObjectItem(req, "keep_recent_interactions");
    if (ok) PASS(); else FAIL("non-aged context leaked into compact request");
    cJSON_Delete(req); free(body); cJSON_Delete(msgs);
}

static void test_compact_request_keeps_aged_tool_group_intact(void) {
    TEST("compact request: aged tool group stays intact");
    cJSON *msgs = cJSON_CreateArray();
    cJSON_AddItemToArray(msgs, make_msg("system", "authority"));
    cJSON_AddItemToArray(msgs, make_msg("user", "aged question"));
    cJSON_AddItemToArray(msgs, cJSON_Parse(
        "{\"role\":\"assistant\",\"content\":null,\"tool_calls\":[{"
        "\"id\":\"aged-call\",\"type\":\"function\",\"function\":{"
        "\"name\":\"shell\",\"arguments\":\"{\\\"command\\\":\\\"true\\\"}\"}}]}"));
    cJSON_AddItemToArray(msgs, cJSON_Parse(
        "{\"role\":\"tool\",\"tool_call_id\":\"aged-call\",\"content\":\"[exit:0]\"}"));
    cJSON_AddItemToArray(msgs, make_msg("assistant", "aged answer"));
    for (int i = 0; i < COMPACT_KEEP_RECENT; i++) {
        cJSON_AddItemToArray(msgs, make_msg("user", "recent question"));
        cJSON_AddItemToArray(msgs, make_msg("assistant", "recent answer"));
    }

    CompactLayout layout;
    int ready = compact_layout(msgs, 0, &layout) == 1;
    char *body = ready ? compact_request_body(msgs, 0, &layout) : NULL;
    cJSON *req = body ? cJSON_Parse(body) : NULL;
    cJSON *aged = req ? cJSON_GetObjectItem(req, "messages") : NULL;
    cJSON *call_msg = aged ? cJSON_GetArrayItem(aged, 1) : NULL;
    cJSON *calls = call_msg ? cJSON_GetObjectItem(call_msg, "tool_calls") : NULL;
    cJSON *call = calls ? cJSON_GetArrayItem(calls, 0) : NULL;
    cJSON *call_id = call ? cJSON_GetObjectItem(call, "id") : NULL;
    cJSON *result = aged ? cJSON_GetArrayItem(aged, 2) : NULL;
    cJSON *result_id = result ? cJSON_GetObjectItem(result, "tool_call_id") : NULL;
    int ok = cJSON_IsArray(aged) && cJSON_GetArraySize(aged) == 4 &&
             !strcmp(message_role(cJSON_GetArrayItem(aged, 0)), "user") &&
             !strcmp(message_role(call_msg), "assistant") &&
             !strcmp(message_role(result), "tool") &&
             !strcmp(message_role(cJSON_GetArrayItem(aged, 3)), "assistant") &&
             cJSON_IsString(call_id) && cJSON_IsString(result_id) &&
             !strcmp(call_id->valuestring, "aged-call") &&
             !strcmp(result_id->valuestring, call_id->valuestring);
    if (ok) PASS(); else FAIL("aged tool protocol was split or rewritten");
    cJSON_Delete(req); free(body); cJSON_Delete(msgs);
}

static void test_jsonl_log_frames_multiline_content(void) {
    TEST("log v2: multiline content stays one JSONL record");
    LogSink log = {.jsonl = tmpfile(), .sequence = 0};
    const char *content = "hello\n[2099-01-01 00:00:00] COMPACT: fake\nbye";
    log_write(&log, "USER", content);
    char line[4096] = {0};
    rewind(log.jsonl); fread(line, 1, sizeof(line) - 1, log.jsonl);
    cJSON *entry = cJSON_Parse(line);
    cJSON *parsed = entry ? cJSON_GetObjectItem(entry, "content") : NULL;
    cJSON *sequence = entry ? cJSON_GetObjectItem(entry, "sequence") : NULL;
    if (parsed && cJSON_IsString(parsed) && !strcmp(parsed->valuestring, content) &&
        sequence && cJSON_GetNumberValue(sequence) == 1) PASS();
    else FAIL("multiline JSONL framing changed content");
    cJSON_Delete(entry);
    if (log.jsonl) fclose(log.jsonl);
}

static void test_sensitive_logs_use_private_permissions(void) {
    TEST("logs: sensitive files and directory are private");
    char root[] = "/tmp/subzeroclaw-log-test-XXXXXX";
    if (!mkdtemp(root)) { FAIL("mkdtemp failed"); return; }
    char dir[1024], path[1024];
    snprintf(dir, sizeof(dir), "%s/nested/logs", root);
    mkdirp(dir);
    snprintf(path, sizeof(path), "%s/session.jsonl", dir);
    FILE *file = open_private_append(path);
    if (file) { fputs("secret\n", file); fclose(file); }
    struct stat dir_stat = {0}, file_stat = {0};
    int ok = file && stat(dir, &dir_stat) == 0 && stat(path, &file_stat) == 0 &&
        (dir_stat.st_mode & 0777) == 0700 && (file_stat.st_mode & 0777) == 0600;
    unlink(path); rmdir(dir);
    char parent[1024]; snprintf(parent, sizeof(parent), "%s/nested", root);
    rmdir(parent); rmdir(root);
    if (ok) PASS(); else FAIL("sensitive log permissions were not 0700/0600");
}

int main(void) {
    printf("\n  SubZeroClaw test suite\n");
    printf("  ═══════════════════════════════════════════\n\n");

    test_shell_echo();
    test_shell_pipe();
    test_shell_stderr();
    test_unknown_tool();
    test_shell_bad_args();
    test_shell_heredoc();
    test_shell_exit_code_ok();
    test_shell_exit_code_fail();
    test_shell_large_output_is_drained();
    test_http_post_uses_anonymous_pipes();
    test_turn_nul_keeps_newlines_one_turn();
    test_turn_content_verbatim();
    test_turn_newline_framed_tty();
    test_turn_eof_empty_is_null();
    test_tools_definitions();
    test_parse_stop_response();
    test_parse_tool_calls_response();
    test_parse_error_response();
    test_parse_garbage();
    test_parse_rejects_malformed_message();
    test_build_request();
    test_build_request_extra_merge();
    test_build_request_extra_empty();
    test_build_request_extra_garbage();
    test_parse_compact_flag();
    test_compact_applies_local_canonical_array_and_logs();
    test_compact_layout_uses_local_seal_state();
    test_compact_layout_rejects_corrupt_local_seal();
    test_compact_rejects_unsafe_summary();
    test_compact_accepts_any_positive_reduction();
    test_compact_false_and_insufficient_reduction_are_noops();
    test_compact_rejects_wrong_contract();
    test_compact_request_sends_only_raw_prior_and_aged_history();
    test_compact_request_keeps_aged_tool_group_intact();
    test_jsonl_log_frames_multiline_content();
    test_sensitive_logs_use_private_permissions();
    test_session_id_shape();
    test_agent_run_compacts_at_completed_seam();
    test_agent_run_rolls_back_unanswered_input();
    test_agent_run_preflights_every_tool_before_side_effects();
    test_agent_run_rolls_back_empty_terminal();
    test_second_compaction_merges_previous_summary();
    test_full_tool_dispatch();
    test_valid_tool_calls_marks_runnable();
    test_recorded_round_pairs_every_call();
    test_system_prompt();
    test_skills_loading();
    test_config_no_key();
    test_config_defaults();
    test_config_scrubs_env();

    printf("\n  ═══════════════════════════════════════════\n");
    printf("  %d passed, %d failed\n\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
