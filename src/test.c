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
    char *r = tool_execute("shell", "{\"command\": \"echo hello_subzeroclaw\"}");
    assert(r);
    if (strstr(r, "hello_subzeroclaw")) PASS();
    else FAIL(r);
    free(r);
}

static void test_shell_pipe(void) {
    TEST("shell: pipe + grep");
    char *r = tool_execute("shell", "{\"command\": \"echo abc123def | grep -o '[0-9]\\\\+'\"}");
    assert(r);
    if (strstr(r, "123")) PASS();
    else FAIL(r);
    free(r);
}

static void test_shell_stderr(void) {
    TEST("shell: captures stderr");
    char *r = tool_execute("shell", "{\"command\": \"LC_ALL=C ls /nonexistent_path_xyz\"}");
    assert(r);
    if (strstr(r, "No such file") || strstr(r, "cannot access") || strstr(r, "error")) PASS();
    else FAIL(r);
    free(r);
}

static void test_unknown_tool(void) {
    TEST("unknown tool returns error");
    char *r = tool_execute("teleport", "{\"destination\": \"mars\"}");
    assert(r);
    if (strstr(r, "unknown tool")) PASS();
    else FAIL(r);
    free(r);
}

static void test_shell_bad_args(void) {
    TEST("shell: bad args JSON");
    char *r = tool_execute("shell", "not json at all");
    assert(r);
    if (strstr(r, "error")) PASS();
    else FAIL(r);
    free(r);
}

static void test_shell_heredoc(void) {
    TEST("shell: heredoc not corrupted by wrap");
    char *r = tool_execute("shell",
        "{\"command\": \"cat <<'END'\\nhello_heredoc\\nEND\"}");
    assert(r);
    if (strstr(r, "hello_heredoc")) PASS();
    else FAIL(r);
    free(r);
}

static void test_shell_exit_code_ok(void) {
    TEST("shell: exit code prefix on success");
    char *r = tool_execute("shell", "{\"command\": \"true\"}");
    assert(r);
    if (strstr(r, "[exit:0]") == r) PASS();
    else FAIL(r);
    free(r);
}

static void test_shell_exit_code_fail(void) {
    TEST("shell: exit code prefix on failure");
    char *r = tool_execute("shell", "{\"command\": \"exit 42\"}");
    assert(r);
    if (strstr(r, "[exit:42]") == r) PASS();
    else FAIL(r);
    free(r);
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
    FILE *f = fmemopen("", 0, "r");
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
        "    \"content\": \"Hello from the forest!\""
        "  }"
        "}]"
        "}";
    Response resp;
    int rc = parse_response(mock, &resp);
    if (rc == 0 &&
        strcmp(resp.finish_reason, "stop") == 0 &&
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
        strcmp(resp.finish_reason, "tool_calls") == 0 &&
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

/* ======== END-TO-END MOCK TEST ======== */

static void test_full_tool_dispatch(void) {
    TEST("e2e: parse tool_call -> execute -> result");
    const char *mock_args = "{\"command\": \"echo subzeroclaw_e2e_ok\"}";
    char *result = tool_execute("shell", mock_args);
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
    TEST("build_request: REQUEST_EXTRA merges, override wins");
    Config cfg; memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.request_extra, MAX_EXTRA,
        "{\"temperature\": 0.5, \"model\": \"override-model\"}");
    cJSON *msgs = cJSON_CreateArray();
    cJSON_AddItemToArray(msgs, make_msg("user", "hi"));
    char *json = build_request(&cfg, msgs, NULL);
    assert(json);
    cJSON *req = cJSON_Parse(json); assert(req);
    cJSON *temp  = cJSON_GetObjectItem(req, "temperature");
    cJSON *model = cJSON_GetObjectItem(req, "model");
    cJSON *m     = cJSON_GetObjectItem(req, "messages");
    int model_keys = 0; cJSON *c = NULL;
    cJSON_ArrayForEach(c, req) if (c->string && !strcmp(c->string, "model")) model_keys++;
    if (temp && cJSON_IsNumber(temp) && temp->valuedouble == 0.5 &&
        model && !strcmp(model->valuestring, "override-model") &&  /* override wins */
        model_keys == 1 &&                                          /* no duplicate */
        m && cJSON_GetArraySize(m) == 1)
        PASS();
    else FAIL("merge/override mismatch");
    cJSON_Delete(req); free(json); cJSON_Delete(msgs);
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
        getenv("SUBZEROCLAW_REQUEST_EXTRA") == NULL;

    if (cfg_ok && env_scrubbed) PASS();
    else if (!cfg_ok) FAIL("cfg lost a provider value");
    else FAIL("env not scrubbed after load");
}

/* ======== MAIN ======== */

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
    test_turn_nul_keeps_newlines_one_turn();
    test_turn_content_verbatim();
    test_turn_newline_framed_tty();
    test_turn_eof_empty_is_null();
    test_tools_definitions();
    test_parse_stop_response();
    test_parse_tool_calls_response();
    test_parse_error_response();
    test_parse_garbage();
    test_build_request();
    test_build_request_extra_merge();
    test_build_request_extra_empty();
    test_build_request_extra_garbage();
    test_full_tool_dispatch();
    test_system_prompt();
    test_skills_loading();
    test_config_no_key();
    test_config_defaults();
    test_config_scrubs_env();

    printf("\n  ═══════════════════════════════════════════\n");
    printf("  %d passed, %d failed\n\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
