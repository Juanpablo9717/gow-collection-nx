// Testes das partes puras do launcher (launcher/status.c e launcher/whatsnew.c), compilados no PC.
// Procuram quebrar o código: entradas vazias, truncadas, enormes, binárias, com BOM/CRLF, caminhos maliciosos e
// um fuzzer simples com milhares de entradas aleatórias (rodar com ASan/UBSan: tests/run_launcher_tests.sh).
#include "status.h"
#include "whatsnew.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

static int failures, checks;
#define CHECK(c) do { checks++; if (!(c)) { failures++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_STR(g, w) do { const char *g_ = (g), *w_ = (w); checks++; if (strcmp(g_, w_)) { failures++; \
    printf("FAIL %s:%d: %s\n  obtido:   \"%s\"\n  esperado: \"%s\"\n", __FILE__, __LINE__, #g, g_, w_); } } while (0)

static void write_bytes(const char *path, const void *data, size_t n) {
    FILE *f = fopen(path, "wb");
    if (!f) { printf("não consegui criar %s\n", path); exit(2); }
    fwrite(data, 1, n, f);
    fclose(f);
}
static void write_text(const char *path, const char *s) { write_bytes(path, s, strlen(s)); }

// ------------------------------------------------------------------ status: psarc e arquivos
static void test_psarc(void) {
    CHECK(st_psarc("nao_existe.psarc") == ST_PSARC_MISSING);
    write_bytes("t_ok.psarc", "PSAR\0\0\0\x01rest", 12);
    CHECK(st_psarc("t_ok.psarc") == ST_PSARC_OK);
    write_bytes("t_empty.psarc", "", 0);
    CHECK(st_psarc("t_empty.psarc") == ST_PSARC_BAD);          // vazio
    write_bytes("t_short.psarc", "PSA", 3);
    CHECK(st_psarc("t_short.psarc") == ST_PSARC_BAD);          // truncado antes do magic
    write_bytes("t_enc.psarc", "\x7F\x10\xA2\x55" "encrypted", 13);
    CHECK(st_psarc("t_enc.psarc") == ST_PSARC_BAD);            // cifrado
    write_bytes("t_lower.psarc", "psar....", 8);
    CHECK(st_psarc("t_lower.psarc") == ST_PSARC_BAD);          // magic sensível a maiúsculas
    CHECK(st_psarc(".") == ST_PSARC_BAD || st_psarc(".") == ST_PSARC_MISSING);  // diretório nunca é OK
    CHECK(!st_file_ok("t_empty.psarc"));
    CHECK(st_file_ok("t_ok.psarc"));
    CHECK(!st_file_ok("nao_existe"));
    CHECK(!st_file_ok("."));                                   // diretório não conta como arquivo
    CHECK(st_pair("t_ok.psarc", "t_enc.psarc") == 1);
    CHECK(st_pair("t_ok.psarc", "nao_existe") == -1);
    CHECK(st_pair("nao_existe", "t_ok.psarc") == -1);
    CHECK(st_pair("nao_existe", "t_empty.psarc") == 0);        // vazio conta como ausente
}

// ------------------------------------------------------------------ status: index.tsv
static void mkpack(const char *dir) { MKDIR(dir); }

static void test_index(void) {
    StIndex r;
    st_index("pasta_inexistente", 0, &r);
    CHECK(!r.folder && !r.index && !r.listed);

    mkpack("v_noindex");
    st_index("v_noindex", 0, &r);
    CHECK(r.folder && !r.index);                                // pasta sem index.tsv: o jogo ignora

    mkpack("v_ok");
    write_text("v_ok/a.mp4", "x");
    write_text("v_ok/b.mp4", "x");
    write_text("v_ok/index.tsv",
               "# comentário\n"
               "intro\ta.mp4\n"
               "boss\tb.mp4\t1.5\r\n"                           // CRLF e terceira coluna
               "\n"
               "falta\tc.mp4\n");
    st_index("v_ok", 0, &r);
    CHECK(r.index && r.listed == 3 && r.missing == 1 && r.bad == 0 && !r.bom);
    CHECK_STR(r.first_missing, "c.mp4");

    mkpack("v_bad");
    write_text("v_bad/x.mp4", "x");
    write_text("v_bad/index.tsv",
               "semtab x.mp4\n"                                 // espaço em vez de TAB
               "\tx.mp4\n"                                      // nome vazio
               "nome\t\n"                                       // arquivo vazio
               "nome\t\t2\n"                                    // arquivo vazio com atraso
               "fuga\t../../etc/passwd\n"                       // sai da pasta
               "abs\t/x.mp4\n"                                  // caminho absoluto
               "dev\tsdmc:/x.mp4\n"                             // dispositivo
               "ok\tx.mp4\n"
               "pontos\t..x.mp4\n");                            // "..x" não é "..": vale (e falta)
    st_index("v_bad", 0, &r);
    CHECK(r.bad == 7);
    CHECK(r.listed == 2 && r.missing == 1);
    CHECK_STR(r.first_missing, "..x.mp4");

    mkpack("v_space");
    write_text("v_space/a.mp4", "x");
    write_text("v_space/index.tsv", "intro\t a.mp4\n");        // o jogo não apara: " a.mp4" não existe
    st_index("v_space", 0, &r);
    CHECK(r.listed == 1 && r.missing == 1);

    mkpack("v_bom");
    write_text("v_bom/a.mp4", "x");
    write_text("v_bom/index.tsv", "\xEF\xBB\xBFintro\ta.mp4\nboss\ta.mp4\n");
    st_index("v_bom", 0, &r);
    CHECK(r.bom && r.listed == 2 && r.missing == 0);

    mkpack("v_empty");
    write_text("v_empty/index.tsv", "");
    st_index("v_empty", 0, &r);
    CHECK(r.index && r.listed == 0 && r.bad == 0);

    mkpack("v_long");                                          // linha gigante sem '\n' no meio
    {
        size_t n = 10000;
        char *big = malloc(n + 32);
        memset(big, 'a', n);
        memcpy(big + n, "\tz.mp4\nok\ta.mp4\n", 17);
        write_bytes("v_long/index.tsv", big, n + 16);
        free(big);
        write_text("v_long/a.mp4", "x");
        st_index("v_long", 0, &r);
        CHECK(r.bad == 1 && r.listed == 1 && r.missing == 0);  // a gigante é inválida, a seguinte ainda conta
    }

    mkpack("v_bin");
    write_bytes("v_bin/index.tsv", "a\0b\tc.mp4\n\xff\xfe\t\x01\n", 15);
    st_index("v_bin", 0, &r);
    CHECK(r.index);                                             // só não pode quebrar

    mkpack("v_longname");                                      // nome de arquivo maior que first_missing
    {
        char line[400];
        memset(line, 'n', sizeof line);
        line[0] = 'k'; line[1] = '\t';
        memcpy(line + sizeof line - 1, "\n", 1);
        write_bytes("v_longname/index.tsv", line, sizeof line);
        st_index("v_longname", 0, &r);
        CHECK(r.missing == 1 && strlen(r.first_missing) == sizeof r.first_missing - 1);
    }

    mkpack("t_pack");
    write_text("t_pack/a.png", "x");
    write_text("t_pack/index.tsv", "\xEF\xBB\xBF" "00FFAA\ta.png\nzzz\ta.png\n0x12\ta.png\n");
    st_index("t_pack", 1, &r);
    CHECK(r.listed == 3 && r.bad_key == 2 && r.bom);           // a 1ª chave é hex (depois do BOM); zzz e 0x12 não
}

// ------------------------------------------------------------------ whatsnew
static const char *MD =
    "# Changelog\n"
    "\n"
    "[Português](CHANGELOG.md) · **English**\n"
    "\n"
    "## v1.5-nx\n"
    "\n"
    "Intro line one\n"
    "continues here.\n"
    "\n"
    "### Performance\n"
    "- **deko3d** is *faster*, see [docs](https://x.y/z).\n"
    "  Continuation of item.\n"
    "- Second `item`.\n"
    "\n"
    "## v1.4-nx\n"
    "\n"
    "### Old\n"
    "- old stuff\n";

static void test_whatsnew(void) {
    char out[512], v[64];
    CHECK(wn_normalize("v1.4-nx", v, sizeof v) && !strcmp(v, "1.4"));
    CHECK(wn_normalize("  V1.5 (beta)", v, sizeof v) && !strcmp(v, "1.5"));
    CHECK(!wn_normalize("", v, sizeof v));
    CHECK(!wn_normalize(NULL, v, sizeof v));
    CHECK(!wn_normalize("v", v, sizeof v));
    CHECK(!wn_normalize("-nx", v, sizeof v) || strcmp(v, "") == 0);
    CHECK(wn_normalize("1.4", v, 2) && !strcmp(v, "1"));        // cap pequeno: corta sem estourar

    int n = wn_section(MD, strlen(MD), "1.5", out, sizeof out);
    CHECK(n > 0 && (size_t)n == strlen(out));
    CHECK_STR(out, "Intro line one continues here.\n"
                   "\x01Performance\n"
                   "\xE2\x80\xA2 deko3d is faster, see docs. Continuation of item.\n"
                   "\xE2\x80\xA2 Second item.\n");
    n = wn_section(MD, strlen(MD), "v1.4", out, sizeof out);
    CHECK_STR(out, "\x01Old\n\xE2\x80\xA2 old stuff\n");
    CHECK(wn_section(MD, strlen(MD), "1.3", out, sizeof out) == -1 && out[0] == 0);
    CHECK(wn_section(MD, strlen(MD), "1.", out, sizeof out) == -1);       // prefixo não basta
    CHECK(wn_section(MD, strlen(MD), "", out, sizeof out) == -1);
    CHECK(wn_section(NULL, 0, "1.5", out, sizeof out) == -1);
    CHECK(wn_section(MD, 0, "1.5", out, sizeof out) == -1);
    CHECK(wn_section(MD, strlen(MD), "1.5", out, 0) == -1);

    // saída pequena: nunca estoura, sempre termina em '\0' e o retorno bate com strlen
    for (size_t cap = 1; cap < 40; cap++) {
        char small[40];
        memset(small, 'Z', sizeof small);
        int r = wn_section(MD, strlen(MD), "1.5", small, cap);
        CHECK(small[cap - 1] == 0 || strlen(small) < cap);
        CHECK(r == -1 ? small[0] == 0 : (size_t)r == strlen(small));
        for (size_t k = cap; k < sizeof small; k++) CHECK(small[k] == 'Z');
    }

    // CRLF, BOM, seção vazia, seção no fim sem '\n', link quebrado, NUL no meio
    const char *crlf = "\xEF\xBB\xBF## v2.0\r\n- a\r\n- b\r\n";
    wn_section(crlf, strlen(crlf), "2.0", out, sizeof out);
    CHECK_STR(out, "\xE2\x80\xA2 a\n\xE2\x80\xA2 b\n");
    const char *empty = "## 3.0\n\n## 2.0\n- x\n";
    CHECK(wn_section(empty, strlen(empty), "3.0", out, sizeof out) == -1);
    const char *eof = "## 4.0\nlast";
    wn_section(eof, strlen(eof), "4.0", out, sizeof out);
    CHECK_STR(out, "last\n");
    const char *broken = "## 5.0\n- [unclosed link(and *stars and `ticks\n";
    CHECK(wn_section(broken, strlen(broken), "5.0", out, sizeof out) > 0);
    const char nul[] = "## 6.0\nbefore\0after\nnext\n";
    wn_section(nul, sizeof nul - 1, "6.0", out, sizeof out);
    CHECK_STR(out, "before next\n");
    const char *under = "## 7.0\n- keep data_path and _italic_\n";
    wn_section(under, strlen(under), "7.0", out, sizeof out);
    CHECK_STR(out, "\xE2\x80\xA2 keep data_path and italic\n");
    const char *nested = "## 8.0\n# Other doc\n- hidden\n";
    CHECK(wn_section(nested, strlen(nested), "8.0", out, sizeof out) == -1);

    CHECK(wn_should_show("", "1.5"));
    CHECK(wn_should_show(NULL, "1.5"));
    CHECK(!wn_should_show("1.5", "v1.5-nx"));
    CHECK(wn_should_show("1.4", "1.5"));
    CHECK(!wn_should_show("", "dev"));
    CHECK(!wn_should_show("", ""));
    CHECK(!wn_should_show("", NULL));

    char seen[32];
    wn_seen_read("nao_existe.txt", seen, sizeof seen);
    CHECK(seen[0] == 0);
    CHECK(wn_seen_write("t_seen.txt", "1.5"));
    wn_seen_read("t_seen.txt", seen, sizeof seen);
    CHECK_STR(seen, "1.5");
    write_text("t_seen.txt", "1.6\r\n  trailing junk line\n");
    wn_seen_read("t_seen.txt", seen, sizeof seen);
    CHECK_STR(seen, "1.6");
    write_text("t_seen.txt", "");
    wn_seen_read("t_seen.txt", seen, sizeof seen);
    CHECK(seen[0] == 0);
    char tiny[2];
    write_text("t_seen.txt", "123456");
    wn_seen_read("t_seen.txt", tiny, sizeof tiny);
    CHECK(strlen(tiny) <= 1);
    CHECK(!wn_seen_write("pasta_inexistente/x/y.txt", "1.5"));
}

// ------------------------------------------------------------------ regressões achadas pelo teste adversarial
static int utf8_complete(const char *s) {   // 1 se não termina no meio de um caractere
    size_t n = strlen(s), i = n;
    while (i > 0 && ((unsigned char)s[i - 1] & 0xC0) == 0x80) i--;
    if (i == 0) return n == 0;
    unsigned char lead = (unsigned char)s[i - 1];
    size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    return n - (i - 1) >= need;
}

static void test_regressions(void) {
    char out[64];
    StIndex r;
    // 1. corte no meio de um caractere UTF-8: nem a seção nem first_missing terminam pela metade
    const char *md = "## 1.4\n- \xC3\xA3\xC3\xA3\xC3\xA3\xC3\xA3\n";
    for (size_t cap = 1; cap < 16; cap++) {
        char o[16];
        wn_section(md, strlen(md), "1.4", o, cap);
        CHECK(utf8_complete(o));
    }
    mkpack("r_utf8");
    {
        char line[200] = "k\t";
        size_t n = 2;
        while (n < 190) { line[n++] = (char)0xC3; line[n++] = (char)0xA3; }   // "ããã..." maior que first_missing
        line[n++] = '\n';
        write_bytes("r_utf8/index.tsv", line, n);
        st_index("r_utf8", 0, &r);
        CHECK(r.missing == 1 && utf8_complete(r.first_missing));
    }
    // 2. ao encher, não emenda pedaço de outra linha
    const char *splice = "## 1.4\nabc  \nzz\n\n- x\n";
    wn_section(splice, strlen(splice), "1.4", out, 6);
    CHECK_STR(out, "abc z");
    // 3. sem espaço duplo ao juntar linhas
    wn_section(splice, strlen(splice), "1.4", out, sizeof out);
    CHECK_STR(out, "abc zz\n\xE2\x80\xA2 x\n");
    // 4. linha de exatamente 2047 bytes (o limite do buffer) no fim, sem '\n', e 2046 + "\r\n" são válidas
    mkpack("r_2047");
    {
        static char big[2100];
        memset(big, 'a', 2047);
        big[0] = 'k'; big[1] = '\t';
        write_bytes("r_2047/index.tsv", big, 2047);
        st_index("r_2047", 0, &r);
        CHECK(r.bad == 0 && r.listed == 1);
        big[2046] = '\r'; big[2047] = '\n';
        write_bytes("r_2047/index.tsv", big, 2048);
        st_index("r_2047", 0, &r);
        CHECK(r.bad == 0 && r.listed == 1);
        memset(big, 'a', 2100);
        big[0] = 'k'; big[1] = '\t';
        write_bytes("r_2047/index.tsv", big, 2100);          // passa do buffer: inválida
        st_index("r_2047", 0, &r);
        CHECK(r.bad == 1 && r.listed == 0);
    }
    // 5. '\0' no meio de uma linha longa não cria entrada fantasma
    mkpack("r_nul");
    {
        static char b[2200];
        size_t n = 0;
        b[n++] = 0;
        memset(b + n, 'q', 2046); n += 2046;
        memcpy(b + n, "k\tnope.bin\n", 11); n += 11;
        write_bytes("r_nul/index.tsv", b, n);
        st_index("r_nul", 0, &r);
        CHECK(r.listed == 0 && r.missing == 0 && r.bad == 1);
    }
    // 6. BOM detectado mesmo com a 1ª linha gigante
    mkpack("r_bomlong");
    {
        static char b[3000];
        memcpy(b, "\xEF\xBB\xBF", 3);
        memset(b + 3, 'z', 2990);
        b[2993] = '\n';
        write_bytes("r_bomlong/index.tsv", b, 2994);
        st_index("r_bomlong", 0, &r);
        CHECK(r.bom && r.bad == 1);
    }
    // 7. arquivo "visto" salvo com BOM não faz a tela voltar para sempre
    write_text("t_seen.txt", "\xEF\xBB\xBF" "1.4\n");
    char seen[16];
    wn_seen_read("t_seen.txt", seen, sizeof seen);
    CHECK_STR(seen, "1.4");
    CHECK(!wn_should_show(seen, "1.4"));
    // 8. Markdown: listas com TAB e numeradas, link quebrado, "### " vazio, "## [1.4] - data"
    const char *m8 = "## [1.4] - 2026-11-25\n###  \n-\ta\n1. b\n10.\tc\nsee [a](b [c](d)e\n";
    wn_section(m8, strlen(m8), "1.4", out, sizeof out);
    CHECK_STR(out, "\xE2\x80\xA2 a\n1. b\n10. c see [a](b ce\n");   // "[c](d)e" sí es link: queda "ce"
}

// ------------------------------------------------------------------ fuzz
static uint32_t rng = 0x12345678u;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static void fuzz(int iterations) {
    static const char *pieces[] = { "## ", "### ", "# ", "- ", "* ", "\n", "\r\n", "\t", "**", "*", "`", "_", "[", "](",
                                    ")", "v1.5-nx", "1.5", "\xEF\xBB\xBF", " ", "\xE2\x80\xA2", "..", "/", ":", "\0" };
    char md[600], out[256], small[7];
    int crashes_would_be_asan = 0;
    for (int it = 0; it < iterations; it++) {
        size_t n = 0;
        while (n < sizeof md - 16) {
            if (rnd() % 3 == 0) md[n++] = (char)(rnd() & 0xFF);
            else {
                const char *p = pieces[rnd() % (sizeof pieces / sizeof pieces[0])];
                size_t l = p[0] ? strlen(p) : 1;
                memcpy(md + n, p, l);
                n += l;
            }
            if (rnd() % 40 == 0) break;
        }
        int r = wn_section(md, n, "1.5", out, sizeof out);
        if (r != -1 && (size_t)r != strlen(out)) crashes_would_be_asan++;
        r = wn_section(md, n, "1.5", small, sizeof small);
        if (strlen(small) >= sizeof small) crashes_would_be_asan++;

        // o mesmo texto como index.tsv
        write_bytes("fz/index.tsv", md, n);
        StIndex s;
        st_index("fz", it & 1, &s);
        if (s.listed < s.missing || s.listed < 0 || s.bad < 0) crashes_would_be_asan++;
        if (strlen(s.first_missing) >= sizeof s.first_missing) crashes_would_be_asan++;
    }
    CHECK(crashes_would_be_asan == 0);
}

int main(int argc, char **argv) {
    int iters = argc > 1 ? atoi(argv[1]) : 3000;
    MKDIR("fz");
    test_psarc();
    test_index();
    test_whatsnew();
    test_regressions();
    fuzz(iters);
    printf("%d/%d verificações ok (fuzz: %d entradas)\n", checks - failures, checks, iters);
    return failures ? 1 : 0;
}
