// Tela "Novidades" do launcher: a seção do CHANGELOG da versão instalada, mostrada a cada abertura até o
// jogador marcar "Não mostrar de novo". Funções puras (sem libnx), testadas no PC.
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Versão comparável: sem 'v' inicial, sem sufixo "-nx" e sem o que vier depois de um espaço, em minúsculas.
// "v1.4-nx" e "1.4" viram "1.4". Devolve 0 se ficou vazia.
int wn_normalize(const char *in, char *out, size_t cap);

// Copia para out o texto da seção "## <versão>" do changelog em Markdown, já limpo para a tela:
// - títulos "### X" viram uma linha "\x01X" (o launcher desenha em dourado);
// - itens "- x" viram "• x", juntando as linhas de continuação;
// - parágrafos com quebra de linha viram uma linha só;
// - **negrito**, *itálico*, `código` e [texto](link) ficam só com o texto.
// Uma linha lógica por '\n'. Devolve o tamanho escrito, ou -1 se a seção não existe ou ficou vazia.
int wn_section(const char *md, size_t len, const char *version, char *out, size_t cap);

// Onde procurar as notas de um idioma, em ordem de preferência. Com code = "es-419":
//   <sd_dir>/CHANGELOG.es-419.md, <builtin_dir>/CHANGELOG.es-419.md, <sd_dir>/CHANGELOG.es.md,
//   <builtin_dir>/CHANGELOG.es.md, e por fim o inglês (<sd_dir>/CHANGELOG.en.md, <builtin_dir>/CHANGELOG.en.md).
// O português também procura <builtin_dir>/CHANGELOG.md (o changelog principal do projeto é em português).
// O código vira minúsculas com '-' no lugar de '_'; código vazio ou com caracteres fora de [A-Za-z0-9_-] (ou
// longo demais) vai direto para o inglês, então nunca vira caminho fora da pasta. Pasta NULL ou vazia é pulada, e um caminho
// que não cabe em WN_PATH_MAX também. english = 1 nos itens em inglês quando o idioma pedido não é inglês (para a
// tela avisar por que o texto está em inglês). Devolve quantos itens escreveu (no máximo max).
#define WN_PATH_MAX 768
#define WN_MAX_PATHS 8   // pior caso hoje: 7 ("pt-br": variante e base no SD e no romfs, CHANGELOG.md e o inglês x2)
typedef struct {
    char path[WN_PATH_MAX];
    int english;
} WnPath;
int wn_paths(const char *code, const char *sd_dir, const char *builtin_dir, WnPath *out, int max);

// 1 se a tela deve aparecer: versão conhecida (não "dev"/vazia) e diferente da última marcada como vista.
int wn_should_show(const char *seen, const char *version);

// Lê/grava a versão marcada como vista (arquivo de uma linha). wn_seen_read deixa out vazio se não houver.
void wn_seen_read(const char *path, char *out, size_t cap);
int wn_seen_write(const char *path, const char *version);

#ifdef __cplusplus
}
#endif
