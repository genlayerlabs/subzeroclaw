/* szc_mock.c — optional mock/record overrides for llm_chat.
 *
 * Activated via env vars (no CLI flags needed; subzeroclaw.c stays clean):
 *   SUBZEROCLAW_MOCK_SCRIPT=path     replay canned responses, no API calls
 *   SUBZEROCLAW_RECORD_SCRIPT=path   record real API responses to file
 *
 * Mock takes precedence if both are set. When this file is linked in,
 * its non-weak llm_chat overrides the weak default from subzeroclaw.c.
 * No #ifdef in the main file, no changes to its public surface.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

/* MUST stay in sync with the Config struct in subzeroclaw.c.
   Build-time check below guards against silent drift. */
#define MAX_PATH   512
#define MAX_VALUE  1024
typedef struct {
    char api_key[MAX_VALUE], model[MAX_VALUE], endpoint[MAX_VALUE];
    char skills_dir[MAX_PATH], log_dir[MAX_PATH];
    int  max_turns, max_messages;
} Config;
_Static_assert(sizeof(Config) == MAX_VALUE * 3 + MAX_PATH * 2 + sizeof(int) * 2,
    "Config struct layout drifted from subzeroclaw.c — update szc_mock.c");

extern char *build_request(const Config *cfg, cJSON *msgs, cJSON *tools);
extern char *http_post(const char *url, const char *api_key, const char *body);

static cJSON *g_mock = NULL;
static int    g_mock_idx = 0;
static FILE  *g_rec = NULL;
static int    g_rec_count = 0;
static int    g_init_done = 0;

static void mock_atexit(void) {
    if (g_rec) {
        fputs("\n]\n", g_rec); fclose(g_rec); g_rec = NULL;
        fprintf(stderr, "[record] saved %d responses\n", g_rec_count);
    }
    if (g_mock) { cJSON_Delete(g_mock); g_mock = NULL; }
}

static void mock_init(void) {
    if (g_init_done) return;
    g_init_done = 1;

    const char *mp = getenv("SUBZEROCLAW_MOCK_SCRIPT");
    if (mp && *mp) {
        FILE *f = fopen(mp, "r");
        if (!f) { fprintf(stderr, "[mock] cannot open %s\n", mp); }
        else {
            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
            char *data = malloc(sz + 1);
            if (data) {
                data[fread(data, 1, sz, f)] = '\0';
                g_mock = cJSON_Parse(data);
                free(data);
            }
            fclose(f);
            if (!g_mock || !cJSON_IsArray(g_mock)) {
                fprintf(stderr, "[mock] invalid script %s\n", mp);
                if (g_mock) cJSON_Delete(g_mock);
                g_mock = NULL;
            } else {
                fprintf(stderr, "[mock] loaded %d responses from %s\n",
                    cJSON_GetArraySize(g_mock), mp);
            }
        }
    }

    const char *rp = getenv("SUBZEROCLAW_RECORD_SCRIPT");
    if (rp && *rp) {
        g_rec = fopen(rp, "w");
        if (!g_rec) fprintf(stderr, "[record] cannot open %s\n", rp);
        else { fputs("[\n", g_rec); fprintf(stderr, "[record] writing to %s\n", rp); }
    }

    atexit(mock_atexit);
}

/* Pull next canned message and wrap it in an OpenAI-style envelope.
   Cycles the last entry once the script is exhausted. */
static char *mock_next(void) {
    int total = cJSON_GetArraySize(g_mock);
    if (g_mock_idx >= total) g_mock_idx = total - 1;
    cJSON *entry = cJSON_GetArrayItem(g_mock, g_mock_idx++);
    if (!entry) return NULL;

    cJSON *tcs = cJSON_GetObjectItem(entry, "tool_calls");
    const char *finish = (tcs && cJSON_GetArraySize(tcs) > 0) ? "tool_calls" : "stop";

    cJSON *resp = cJSON_CreateObject();
    cJSON *choices = cJSON_CreateArray();
    cJSON *choice = cJSON_CreateObject();
    cJSON_AddStringToObject(choice, "finish_reason", finish);
    cJSON_AddItemReferenceToObject(choice, "message", entry);
    cJSON_AddItemToArray(choices, choice);
    cJSON_AddItemToObject(resp, "choices", choices);

    char *json = cJSON_PrintUnformatted(resp);
    cJSON_DetachItemFromObject(choice, "message");
    cJSON_Delete(resp);

    fprintf(stderr, "[mock] response %d/%d (%s)\n", g_mock_idx, total, finish);
    return json;
}

/* Extract the message object from a real API response and append it to
   the record file. Output is a valid mock script that can be replayed. */
static void record_real(const char *body) {
    if (!g_rec || !body) return;
    cJSON *root = cJSON_Parse(body);
    if (!root) return;
    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    cJSON *choice  = choices ? cJSON_GetArrayItem(choices, 0) : NULL;
    cJSON *msg     = choice ? cJSON_GetObjectItem(choice, "message") : NULL;
    if (msg) {
        char *json = cJSON_PrintUnformatted(msg);
        if (json) {
            if (g_rec_count > 0) fputs(",\n", g_rec);
            fputs("  ", g_rec); fputs(json, g_rec); fflush(g_rec);
            free(json);
            g_rec_count++;
        }
    }
    cJSON_Delete(root);
}

/* Non-weak: overrides the weak default in subzeroclaw.c at link time. */
char *llm_chat(const Config *cfg, cJSON *msgs, cJSON *tools) {
    mock_init();
    if (g_mock) return mock_next();

    char *rj = build_request(cfg, msgs, tools);
    if (!rj) return NULL;
    char *rb = http_post(cfg->endpoint, cfg->api_key, rj);
    free(rj);
    if (rb && g_rec) record_real(rb);
    return rb;
}
