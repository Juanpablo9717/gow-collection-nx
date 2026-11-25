// Verificações da tela "Estado" do launcher: arquivos do jogo, shaders e extras opcionais (texturas HD,
// vídeos HD, áudio do PS2). Só lê o SD; não depende do libnx, então roda nos testes do PC.
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Resultado de um arquivo PSARC do jogo.
typedef enum {
    ST_PSARC_MISSING = 0,   // não existe
    ST_PSARC_OK,            // começa com "PSAR" (arquivo já decifrado, como o jogo espera)
    ST_PSARC_BAD,           // existe mas não começa com "PSAR" (cópia ainda cifrada, truncada ou outro arquivo)
} StPsarc;
StPsarc st_psarc(const char *path);

// 1 se o arquivo existe e não está vazio.
int st_file_ok(const char *path);

// Resultado de um index.tsv de texturas HD ou vídeos HD, lido com as mesmas regras do jogo:
// linhas "nome<TAB>arquivo[<TAB>atraso]", '#' é comentário, "\r" no fim é ignorado e nada é aparado.
typedef struct {
    int folder;        // a pasta existe
    int index;         // a pasta tem index.tsv
    int listed;        // linhas válidas (com nome e arquivo)
    int missing;       // linhas válidas cujo arquivo não existe na pasta
    int bad;           // linhas sem TAB, com nome ou arquivo vazio, ou com caminho que sai da pasta
    int bom;           // o arquivo começa com BOM UTF-8 (o jogo não ignora: a 1ª linha falha)
    int bad_key;       // só texturas: linhas cujo nome não é hexadecimal
    char first_missing[96];  // nome do primeiro arquivo que falta, para mostrar na tela
} StIndex;

// is_texpack: 1 para data/texpack/<jogo> (nome é a chave em hexadecimal), 0 para data/videos/<jogo>.
void st_index(const char *folder, int is_texpack, StIndex *out);

// Dois arquivos que só funcionam juntos (ps2audio.bin + ps2audio_files.idx):
// 0 nenhum, 1 os dois, -1 só um deles.
int st_pair(const char *a, const char *b);

#ifdef __cplusplus
}
#endif
