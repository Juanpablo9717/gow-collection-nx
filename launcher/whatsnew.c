// Tela "Novidades": ver whatsnew.h.
#include "whatsnew.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

int wn_normalize(const char *in, char *out, size_t cap) {
    if (!cap) return 0;
    out[0] = 0;
    if (!in) return 0;
    while (*in == ' ' || *in == '\t') in++;
    if (*in == '[') in++;                      // "## [1.4] - data" (formato Keep a Changelog)
    if (*in == 'v' || *in == 'V') in++;
    size_t n = 0;
    for (; *in && *in != ' ' && *in != '\t' && *in != '\r' && *in != '\n' && *in != ']' && n + 1 < cap; in++)
        out[n++] = (char)tolower((unsigned char)*in);
    out[n] = 0;
    if (n >= 3 && !strcmp(out + n - 3, "-nx")) out[n -= 3] = 0;
    return n > 0;
}

// Saída com limite: nunca escreve além de cap-1 e sempre termina em '\0'. Quando enche, para de vez (full), para
// não abrir espaço depois e emendar um pedaço de outra linha.
typedef struct {
    char *p;
    size_t n, cap;
    int full;
} Out;

static void put(Out *o, char c) {
    if (o->full) return;
    if (o->n + 1 < o->cap) o->p[o->n++] = c;
    else o->full = 1;
}

static void trim_spaces(Out *o) {
    if (o->full) return;
    while (o->n && o->p[o->n - 1] == ' ') o->n--;
}

// Tira do fim uma sequência UTF-8 incompleta, deixada quando a saída enche no meio de um caractere.
static void trim_utf8(Out *o) {
    size_t i = o->n;
    while (i > 0 && ((unsigned char)o->p[i - 1] & 0xC0) == 0x80) i--;
    if (i == 0) return;
    unsigned char lead = (unsigned char)o->p[i - 1];
    size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    if (o->n - (i - 1) < need) o->n = i - 1;
}

// Último caractere escrito (0 se nenhum).
static char last(const Out *o) { return o->n ? o->p[o->n - 1] : 0; }

static void put_str(Out *o, const char *s) {
    while (*s) put(o, *s++);
}

// Escreve o texto de uma linha sem a marcação inline; junta espaços repetidos.
static void put_inline(Out *o, const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == '*' || c == '`' || c == '_') {
            // '_' só é marcação nas bordas de palavra (_x_); dentro de nomes como data_path fica
            if (c == '_' && i > 0 && i + 1 < n && isalnum((unsigned char)s[i - 1]) && isalnum((unsigned char)s[i + 1])) {
                put(o, c);
            }
            continue;
        }
        if (c == '[') {  // [texto](link) -> texto
            const char *close = memchr(s + i, ']', n - i);
            if (close && (size_t)(close - s) + 1 < n && close[1] == '(') {
                const char *end = memchr(close, ')', n - (size_t)(close - s));
                // link sem espaço entre ( e ): "[a](b [c](d)" não é link e fica como está
                if (end && (memchr(close, ' ', (size_t)(end - close)) || memchr(close, '\t', (size_t)(end - close))))
                    end = NULL;
                if (end) {
                    put_inline(o, s + i + 1, (size_t)(close - s) - i - 1);
                    i = (size_t)(end - s);
                    continue;
                }
            }
        }
        if (c == ' ' || c == '\t') {
            if (last(o) != ' ' && last(o) != '\n' && last(o) != 0) put(o, ' ');
            continue;
        }
        put(o, c);
    }
}

static void end_line(Out *o) {
    trim_spaces(o);
    if (o->n && last(o) != '\n') put(o, '\n');
}

int wn_section(const char *md, size_t len, const char *version, char *out, size_t cap) {
    if (!cap) return -1;
    out[0] = 0;
    char want[64];
    if (!md || !wn_normalize(version, want, sizeof want)) return -1;

    Out o = { out, 0, cap, 0 };
    int inside = 0, in_item = 0;
    size_t i = 0;
    if (len >= 3 && (unsigned char)md[0] == 0xEF && (unsigned char)md[1] == 0xBB && (unsigned char)md[2] == 0xBF) i = 3;
    while (i < len) {
        const char *line = md + i;
        const char *nl = memchr(line, '\n', len - i);
        size_t ll = nl ? (size_t)(nl - line) : len - i;
        i += ll + (nl ? 1 : 0);
        if (ll && line[ll - 1] == '\r') ll--;
        if (memchr(line, '\0', ll)) ll = strlen(line) < ll ? strlen(line) : ll;  // texto binário: corta no NUL

        size_t ind = 0;
        while (ind < ll && (line[ind] == ' ' || line[ind] == '\t')) ind++;
        const char *t = line + ind;
        size_t tl = ll - ind;

        if (tl >= 3 && t[0] == '#' && t[1] == '#' && t[2] == ' ') {           // "## versão" começa/termina seção
            if (inside) break;
            char got[64];
            char head[128];
            size_t hl = tl - 3 < sizeof head - 1 ? tl - 3 : sizeof head - 1;
            memcpy(head, t + 3, hl);
            head[hl] = 0;
            if (wn_normalize(head, got, sizeof got) && !strcmp(got, want)) inside = 1;
            continue;
        }
        if (tl >= 2 && t[0] == '#' && t[1] == ' ' && inside) break;           // "# Título" de outro bloco
        if (!inside) continue;

        if (!tl) {                                                              // linha em branco fecha item/parágrafo
            end_line(&o);
            in_item = 0;
            continue;
        }
        if (tl >= 4 && t[0] == '#' && t[1] == '#' && t[2] == '#') {            // "### subtítulo"
            size_t h = 0;
            while (h < tl && t[h] == '#') h++;
            while (h < tl && (t[h] == ' ' || t[h] == '\t')) h++;
            if (h >= tl) continue;                                                // "### " sem texto
            end_line(&o);
            put(&o, '\x01');
            put_inline(&o, t + h, tl - h);
            end_line(&o);
            in_item = 0;
            continue;
        }
        size_t num = 0;                                                         // "1. item" (lista numerada)
        while (num < tl && num < 4 && t[num] >= '0' && t[num] <= '9') num++;
        if (num && num + 1 < tl && t[num] == '.' && (t[num + 1] == ' ' || t[num + 1] == '\t')) {
            end_line(&o);
            put_inline(&o, t, num + 1);
            put(&o, ' ');
            put_inline(&o, t + num + 2, tl - num - 2);
            in_item = 1;
            continue;
        }
        if (tl >= 2 && (t[0] == '-' || t[0] == '*' || t[0] == '+') && (t[1] == ' ' || t[1] == '\t')) {  // item de lista
            end_line(&o);
            put_str(&o, "\xE2\x80\xA2 ");                                       // "• "
            put_inline(&o, t + 2, tl - 2);
            in_item = 1;
            continue;
        }
        // continuação de item ou de parágrafo: junta com um espaço só
        trim_spaces(&o);
        if (last(&o) && last(&o) != '\n') put(&o, ' ');
        put_inline(&o, t, tl);
        (void)in_item;
    }
    end_line(&o);
    if (o.full) trim_utf8(&o);
    // tira linhas vazias do começo
    size_t s = 0;
    while (s < o.n && o.p[s] == '\n') s++;
    if (s) { memmove(o.p, o.p + s, o.n - s); o.n -= s; }
    o.p[o.n] = 0;
    return o.n ? (int)o.n : -1;
}

int wn_should_show(const char *seen, const char *version) {
    char v[64], s[64];
    if (!wn_normalize(version, v, sizeof v) || !strcmp(v, "dev")) return 0;
    if (!wn_normalize(seen, s, sizeof s)) return 1;
    return strcmp(v, s) != 0;
}

void wn_seen_read(const char *path, char *out, size_t cap) {
    if (!cap) return;
    out[0] = 0;
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (!fgets(out, (int)cap, f)) out[0] = 0;
    fclose(f);
    if (strlen(out) >= 3 && (unsigned char)out[0] == 0xEF && (unsigned char)out[1] == 0xBB && (unsigned char)out[2] == 0xBF)
        memmove(out, out + 3, strlen(out + 3) + 1);   // salvo por um editor com BOM
    size_t n = strlen(out);
    while (n && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ')) out[--n] = 0;
}

int wn_seen_write(const char *path, const char *version) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    int ok = fprintf(f, "%s\n", version) > 0;
    return fclose(f) == 0 && ok;
}
