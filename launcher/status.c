// Verificações da tela "Estado": ver status.h.
#include "status.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

StPsarc st_psarc(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return ST_PSARC_MISSING;
    char m[4];
    size_t n = fread(m, 1, sizeof m, f);
    fclose(f);
    return n == 4 && !memcmp(m, "PSAR", 4) ? ST_PSARC_OK : ST_PSARC_BAD;
}

int st_file_ok(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

static int is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// O jogo junta "pasta/arquivo" sem conferir o nome; aqui um nome que sai da pasta conta como linha inválida.
static int name_escapes(const char *s, size_t n) {
    if (n && (s[0] == '/' || s[0] == '\\')) return 1;
    for (size_t i = 0; i < n; i++)
        if (s[i] == ':') return 1;  // "sdmc:/..." ou "C:\..."
    for (size_t i = 0; i + 1 < n; i++)
        if (s[i] == '.' && s[i + 1] == '.' && (i == 0 || s[i - 1] == '/' || s[i - 1] == '\\') &&
            (i + 2 == n || s[i + 2] == '/' || s[i + 2] == '\\'))
            return 1;
    return 0;
}

enum { LINE_EOF, LINE_OK, LINE_LONG, LINE_NUL };

// Lê uma linha inteira (até '\n' ou fim do arquivo), sem o '\n'. Uma linha maior que o buffer é descartada até o
// fim (LINE_LONG); uma com '\0' no meio volta como LINE_NUL, porque o texto depois do '\0' não é confiável.
static int read_line(FILE *f, char *buf, size_t cap, size_t *len) {
    size_t n = 0;
    int c, any = 0, over = 0, nul = 0;
    while ((c = fgetc(f)) != EOF) {
        any = 1;
        if (c == '\n') break;
        if (c == 0) nul = 1;
        if (n + 1 < cap) buf[n++] = (char)c;
        else over = 1;
    }
    buf[n] = 0;
    *len = n;
    return !any ? LINE_EOF : over ? LINE_LONG : nul ? LINE_NUL : LINE_OK;
}

// Tira do fim uma sequência UTF-8 incompleta (deixada por um corte), para a tela não ler caracteres pela metade.
static void utf8_trim(char *s) {
    size_t n = strlen(s), i = n;
    while (i > 0 && ((unsigned char)s[i - 1] & 0xC0) == 0x80) i--;   // bytes de continuação no fim
    if (i == 0) return;
    unsigned char lead = (unsigned char)s[i - 1];
    size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    if (n - (i - 1) < need) s[i - 1] = 0;
}

static int all_hex(const char *s, size_t n) {
    if (!n) return 0;
    for (size_t i = 0; i < n; i++)
        if (!isxdigit((unsigned char)s[i])) return 0;
    return 1;
}

void st_index(const char *folder, int is_texpack, StIndex *out) {
    memset(out, 0, sizeof *out);
    out->folder = is_dir(folder);
    if (!out->folder) return;
    char path[1024];
    if (snprintf(path, sizeof path, "%s/index.tsv", folder) >= (int)sizeof path) return;
    FILE *f = fopen(path, "rb");
    if (!f) return;
    out->index = 1;

    char line[2048];
    int first = 1, kind;
    size_t len;
    while ((kind = read_line(f, line, sizeof line, &len)) != LINE_EOF) {
        char *s = line;
        int line_bom = first && len >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB &&
                       (unsigned char)s[2] == 0xBF;
        if (line_bom) out->bom = 1;  // não pula: o jogo também não pula, então o nome da 1ª linha fica errado
        first = 0;
        if (kind == LINE_LONG || kind == LINE_NUL) { out->bad++; continue; }  // maior que o buffer ou com '\0'
        if (len && line[len - 1] == '\r') line[--len] = 0;
        if (!len || s[0] == '#') continue;

        char *tab = memchr(s, '\t', len);
        if (!tab) { out->bad++; continue; }
        size_t key_len = (size_t)(tab - s);
        char *file = tab + 1;
        char *tab2 = strchr(file, '\t');
        size_t file_len = tab2 ? (size_t)(tab2 - file) : strlen(file);
        if (!key_len || !file_len || name_escapes(file, file_len)) { out->bad++; continue; }

        out->listed++;
        if (is_texpack && !all_hex(line_bom ? s + 3 : s, line_bom ? key_len - 3 : key_len)) out->bad_key++;

        char fpath[1024];
        int w = snprintf(fpath, sizeof fpath, "%s/%.*s", folder, (int)file_len, file);
        if (w < 0 || w >= (int)sizeof fpath || !st_file_ok(fpath)) {
            if (!out->missing) {
                snprintf(out->first_missing, sizeof out->first_missing, "%.*s", (int)file_len, file);
                utf8_trim(out->first_missing);
            }
            out->missing++;
        }
    }
    fclose(f);
}

int st_pair(const char *a, const char *b) {
    int x = st_file_ok(a), y = st_file_ok(b);
    return x && y ? 1 : x || y ? -1 : 0;
}
