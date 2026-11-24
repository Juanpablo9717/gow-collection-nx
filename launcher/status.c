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
    int first = 1;
    while (fgets(line, sizeof line, f)) {
        size_t len = strlen(line);
        int truncated = len == sizeof line - 1 && line[len - 1] != '\n';
        if (truncated) {  // linha maior que o buffer: conta como inválida e descarta o resto
            int c;
            while ((c = fgetc(f)) != EOF && c != '\n') {}
            out->bad++;
            first = 0;
            continue;
        }
        if (len && line[len - 1] == '\n') line[--len] = 0;
        if (len && line[len - 1] == '\r') line[--len] = 0;
        char *s = line;
        int line_bom = first && len >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB &&
                       (unsigned char)s[2] == 0xBF;
        if (line_bom) out->bom = 1;  // não pula: o jogo também não pula, então o nome da 1ª linha fica errado
        first = 0;
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
            if (!out->missing) snprintf(out->first_missing, sizeof out->first_missing, "%.*s", (int)file_len, file);
            out->missing++;
        }
    }
    fclose(f);
}

int st_pair(const char *a, const char *b) {
    int x = st_file_ok(a), y = st_file_ok(b);
    return x && y ? 1 : x || y ? -1 : 0;
}
