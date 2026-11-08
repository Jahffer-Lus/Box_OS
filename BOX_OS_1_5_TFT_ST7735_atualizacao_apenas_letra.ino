
#include <avr/wdt.h>
#include <EEPROM.h>
#include <string.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define JOY_X A0
#define JOY_Y A1
#define JOY_SW 7
#define BUZZER_PIN 6

#define TFT_CS 13
#define TFT_DC 11
#define TFT_RST 12
#define TFT_MOSI 10
#define TFT_SCLK 9
Adafruit_ST7735 displayTFT(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
#define CYAN  F("#55E6FF")
#define WHITE F("#FFFFFF")
#define GRAY  F("#AABBC8")
#define DARK  F("#28445E")
#define GREEN F("#36D17C")
#define YELLOW F("#F2D34F")
#define ORANGE F("#F39A38")
#define RED F("#F04B4B")

const byte TOTAL_APPS = 2;
const byte TOTAL_CONFIG = 5;

const __FlashStringHelper *nomeApp(byte indice) {
  switch (indice) {
    case 0: return F("BOX FILES");
    default: return F("CONFIG");
  }
}

const __FlashStringHelper *nomeOpcaoConfig(byte indice) {
  switch (indice) {
    case 0: return F("Tamanho da Fonte");
    case 1: return F("Rotacao da Tela");
    case 2: return F("Desligar");
    case 3: return F("Reiniciar");
    default: return F("Voltar");
  }
}


enum Screen {
  MENU,
  CONFIG,
  AJUSTAR_FONTE,
  AJUSTAR_ROTACAO,
  FILE_LIST,
  FILE_VIEW,
  FILE_EDIT
};

Screen tela = MENU;

byte selecionado = 0;
byte configSelecionada = 0;

byte tamanhoFonte = 1;
byte rotacaoTela = 3;
byte configPrimeira = 0;

const int CONFIG_EEPROM_BASE = 1008;
const byte CONFIG_MAGIC = 0xB7;

int centroX = 512;
int centroY = 512;

const int LIMITE_JOYSTICK = 130;

unsigned long ultimoMovimento = 0;
unsigned long ultimoBotao = 0;
bool iniciado = false;
bool ultimoEstadoBotao = HIGH;


const byte FILE_MAGIC_0 = 'B';
const byte FILE_MAGIC_1 = 'X';
const byte FILE_MAGIC_2 = 'F';
const int FILE_HEADER_SIZE = 8;
const byte FILE_TOTAL_SLOTS = 5;
const int FILE_SLOT_SIZE = 200;
const byte FILE_NAME_SIZE = 12;
const byte FILE_MAX_TEXT = 185;
const byte FILE_ACTIVE_MARK = 0xA5;
const int FILE_CONTENT_OFFSET = 15;

byte indicesArquivos[FILE_TOTAL_SLOTS];
byte qtdArquivos = 0;
bool temEspacoArquivo = false;
byte selecionadoArquivo = 0;
byte arquivoSlotAtual = 0;
byte acaoArquivo = 0;
byte paginaArquivo = 0;
char nomeArquivoAtual[FILE_NAME_SIZE + 1];
char conteudoArquivo[FILE_MAX_TEXT + 1];
uint16_t tamanhoConteudo = 0;
byte tecladoColuna = 0;
byte tecladoLinha = 0;
bool editandoNomeArquivo = false;
char nomeTemporario[9];
byte tamanhoNomeTemporario = 0;

void mostrarListaArquivos();
void mostrarLeitorArquivo();
void mostrarEditorArquivo();
void mostrarConfig();
const char TECLADO_QWERTY[4][11] PROGMEM = {
  "QWERTYUIOP",
  "ASDFGHJKL",
  "ZXCVBNM",
  "1234567890"
};

const byte TECLADO_LINHAS = 4;
const byte TECLADO_LINHA_ACOES = 4;
const byte ACAO_DEL = 0;
const byte ACAO_ESPACO = 1;
const byte ACAO_OK = 2;


void limpar() {
  // Pisca rapidamente a tela sem exibir a transição branca.
  displayTFT.enableDisplay(false);
  delay(35);
  displayTFT.enableDisplay(true);
  displayTFT.fillScreen(ST77XX_BLACK);
}

void mostrarTela() {
  // Mostra o quadro pronto de uma só vez.
  displayTFT.enableDisplay(true);
}


void salvarConfiguracoes() {
  EEPROM.update(CONFIG_EEPROM_BASE, CONFIG_MAGIC);
  EEPROM.update(CONFIG_EEPROM_BASE + 1, 3);
  EEPROM.update(CONFIG_EEPROM_BASE + 2, tamanhoFonte);
  EEPROM.update(CONFIG_EEPROM_BASE + 3, rotacaoTela);
  EEPROM.update(CONFIG_EEPROM_BASE + 4,
    (byte)(CONFIG_MAGIC ^ tamanhoFonte ^ rotacaoTela ^ 3));
}

void carregarConfiguracoes() {
  byte assinatura = EEPROM.read(CONFIG_EEPROM_BASE);
  byte versao = EEPROM.read(CONFIG_EEPROM_BASE + 1);
  byte fonte = EEPROM.read(CONFIG_EEPROM_BASE + 2);
  byte valorAntigo = EEPROM.read(CONFIG_EEPROM_BASE + 3);
  byte checksum = EEPROM.read(CONFIG_EEPROM_BASE + 4);

  if (assinatura == CONFIG_MAGIC && versao == 3 &&
      fonte >= 1 && fonte <= 4 && valorAntigo <= 3 &&
      checksum == (byte)(CONFIG_MAGIC ^ fonte ^ valorAntigo ^ 3)) {
    tamanhoFonte = fonte;
    rotacaoTela = valorAntigo;
  } else if (assinatura == CONFIG_MAGIC && versao == 2 &&
             fonte >= 1 && fonte <= 4 && valorAntigo >= 1 &&
             valorAntigo <= 4 &&
             checksum == (byte)(CONFIG_MAGIC ^ fonte ^ valorAntigo)) {
    // Mantem a fonte das versoes anteriores e preserva a orientacao atual.
    tamanhoFonte = fonte;
    rotacaoTela = 3;
    salvarConfiguracoes();
  } else {
    tamanhoFonte = 1;
    rotacaoTela = 3;
    salvarConfiguracoes();
  }
}


void somClique() {
  tone(BUZZER_PIN, 1450, 35);
}

void somNavegacao() {
  tone(BUZZER_PIN, 1050, 22);
}

void somSucesso() {
  tone(BUZZER_PIN, 880, 75);
  delay(85);
  tone(BUZZER_PIN, 1320, 95);
  delay(100);
  noTone(BUZZER_PIN);
}

void somErro() {
  tone(BUZZER_PIN, 230, 180);
  delay(190);
  noTone(BUZZER_PIN);
}

void somApagar() {
  tone(BUZZER_PIN, 700, 65);
  delay(75);
  tone(BUZZER_PIN, 420, 100);
  delay(105);
  noTone(BUZZER_PIN);
}

void somInicializacao() {
  const unsigned int notas[] = {523, 659, 784, 1047};
  for (byte i = 0; i < 4; i++) {
    tone(BUZZER_PIN, notas[i], 85);
    delay(105);
  }
  noTone(BUZZER_PIN);
}

byte ajustarTamanhoFonte(byte tamanhoBase) {
  int resultado = (int)tamanhoBase + (int)tamanhoFonte - 2;
  if (resultado < 1) resultado = 1;
  if (resultado > 6) resultado = 6;
  return (byte)resultado;
}

uint16_t corTFT(const __FlashStringHelper *cor) {
  char valor[8];
  strcpy_P(valor, (PGM_P)cor);
  return corTFT(valor);
}

uint16_t corTFT(const char *cor) {
  if (!cor || cor[0] != '#') return ST77XX_WHITE;
  char *fim;
  unsigned long rgb = strtoul(cor + 1, &fim, 16);
  byte r = (rgb >> 16) & 0xFF;
  byte g = (rgb >> 8) & 0xFF;
  byte b = rgb & 0xFF;
  return displayTFT.color565(r, g, b);
}

void desenharTextoTFT(int x, int y, int tamanho, uint16_t cor, const char *mensagem, bool centralizar) {
  if (!mensagem) return;
  displayTFT.setTextWrap(false);
  int16_t x1, y1;
  uint16_t largura, altura;

  // Diminui somente a linha atual se ela não couber na largura da TFT.
  while (tamanho > 1) {
    displayTFT.setTextSize(tamanho);
    displayTFT.getTextBounds(mensagem, 0, 0, &x1, &y1, &largura, &altura);
    if (largura <= displayTFT.width()) break;
    tamanho--;
  }

  displayTFT.setTextSize(tamanho);
  displayTFT.getTextBounds(mensagem, 0, 0, &x1, &y1, &largura, &altura);
  if (centralizar) x -= largura / 2;
  if (x < 0) x = 0;
  if (x + (int)largura > displayTFT.width()) x = displayTFT.width() - largura;
  if (y < 0) y = 0;
  if (y + (int)altura > displayTFT.height()) y = displayTFT.height() - altura;

  displayTFT.setTextColor(cor, ST77XX_BLACK);
  displayTFT.setCursor(x, y);
  displayTFT.print(mensagem);
}

void desenharTextoFlashTFT(int x, int y, int tamanho, uint16_t cor, const __FlashStringHelper *mensagem, bool centralizar) {
  char buffer[48];
  strncpy_P(buffer, (PGM_P)mensagem, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  desenharTextoTFT(x, y, tamanho, cor, buffer, centralizar);
}

void texto(int x, int y, int tamanho, const __FlashStringHelper *cor, const __FlashStringHelper *mensagem) {
  desenharTextoFlashTFT(x, y, ajustarTamanhoFonte(tamanho), corTFT(cor), mensagem, false);
}

void texto(int x, int y, int tamanho, const __FlashStringHelper *cor, const char *mensagem) {
  desenharTextoTFT(x, y, ajustarTamanhoFonte(tamanho), corTFT(cor), mensagem, false);
}

void linha(int x1, int y1, int x2, int y2, const __FlashStringHelper *cor) {
  displayTFT.drawLine(x1, y1, x2, y2, corTFT(cor));
}

void circulo(int x, int y, int raio, const __FlashStringHelper *cor) {
  displayTFT.fillCircle(x, y, raio, corTFT(cor));
}

void retangulo(int x, int y, int largura, int altura, const __FlashStringHelper *cor) {
  displayTFT.fillRect(x, y, largura, altura, corTFT(cor));
}

void retanguloArredondado(int x, int y, int largura, int altura, int raio, const __FlashStringHelper *cor) {
  displayTFT.fillRoundRect(x, y, largura, altura, raio, corTFT(cor));
}

void textoCentro(int x, int y, int tamanho, const __FlashStringHelper *cor, const char *mensagem) {
  desenharTextoTFT(x, y, tamanho, corTFT(cor), mensagem, true);
}

void textoCentro(int x, int y, int tamanho, const __FlashStringHelper *cor, const __FlashStringHelper *mensagem) {
  desenharTextoFlashTFT(x, y, tamanho, corTFT(cor), mensagem, true);
}

int lerDirecaoX() {
  int x = analogRead(JOY_X) - centroX;
  if (x < -LIMITE_JOYSTICK) return -1;
  if (x > LIMITE_JOYSTICK) return 1;
  return 0;
}

int lerDirecaoY() {
  int y = analogRead(JOY_Y) - centroY;
  if (y < -LIMITE_JOYSTICK) return -1;
  if (y > LIMITE_JOYSTICK) return 1;
  return 0;
}


void calibrarJoystick() {
  long sx = 0;
  long sy = 0;

  for (byte i = 0; i < 24; i++) {
    sx += analogRead(JOY_X);
    sy += analogRead(JOY_Y);
    delay(5);
  }

  centroX = sx / 24;
  centroY = sy / 24;
}

int lerDirecao() {
  int x = analogRead(JOY_X) - centroX;
  int y = analogRead(JOY_Y) - centroY;

  if (abs(y) >= abs(x)) {
    if (y < -LIMITE_JOYSTICK) return -1;
    if (y > LIMITE_JOYSTICK) return 1;
  } else {
    if (x < -LIMITE_JOYSTICK) return -1;
    if (x > LIMITE_JOYSTICK) return 1;
  }

  return 0;
}

bool botaoPressionado() {
  bool estado = digitalRead(JOY_SW);

  if (estado == LOW && ultimoEstadoBotao == HIGH &&
      millis() - ultimoBotao > 250) {
    ultimoEstadoBotao = LOW;
    ultimoBotao = millis();
    somClique();
    return true;
  }

  if (estado == HIGH) {
    ultimoEstadoBotao = HIGH;
  }

  return false;
}

const float vertices[8][3] PROGMEM = {
  {-1,-1,-1}, {1,-1,-1},
  {1,1,-1}, {-1,1,-1},
  {-1,-1,1}, {1,-1,1},
  {1,1,1}, {-1,1,1}
};

const byte arestas[12][2] PROGMEM = {
  {0,1},{1,2},{2,3},{3,0},
  {4,5},{5,6},{6,7},{7,4},
  {0,4},{1,5},{2,6},{3,7}
};

void desenharCubo(float angulo, float pulo) {
  int px[8], py[8];
  const int centroX = displayTFT.width() / 2;
  const int centroY = displayTFT.height() / 2 + 10;

  float ca = cos(angulo);
  float sa = sin(angulo);
  float cb = cos(angulo * 0.65);
  float sb = sin(angulo * 0.65);

  for (byte i = 0; i < 8; i++) {
    float x = pgm_read_float(&vertices[i][0]) * 19;
    float y = pgm_read_float(&vertices[i][1]) * 19;
    float z = pgm_read_float(&vertices[i][2]) * 19;

    float xr = x * ca - z * sa;
    float zr = x * sa + z * ca;
    float yr = y * cb - zr * sb;

    px[i] = centroX + (int)xr;
    py[i] = centroY - (int)yr - (int)pulo;
  }

  // Apaga somente a área do cubo; o título permanece visível.
  displayTFT.fillRect(centroX - 23, centroY - 34, 46, 70, ST77XX_BLACK);

  for (byte i = 0; i < 12; i++) {
    byte a = pgm_read_byte(&arestas[i][0]);
    byte b = pgm_read_byte(&arestas[i][1]);
    linha(px[a], py[a], px[b], py[b], CYAN);
  }
}

void mostrarMenu() {
  tela = MENU;
  limpar();

  texto(5, 3, 3, CYAN, F("APLICATIVOS"));

  linha(5, 38, 123, 38, GRAY);

  for (byte i = 0; i < TOTAL_APPS; i++) {
    int y = 42 + i * 18;

    if (i == selecionado) {
      linha(5, y - 3, 123, y - 3, CYAN);
      linha(5, y + 17, 123, y + 17, CYAN);
    }

    texto(10, y + 1, 2, i == selecionado ? CYAN : WHITE, nomeApp(i));
  }

  linha(5, 137, 123, 137, DARK);
  texto(5, 141, 1, GRAY, F("SETAS: NAVEGAR"));
  texto(5, 151, 1, CYAN, F("SW: ABRIR"));
  mostrarTela();
}


void mostrarConfig() {
  tela = CONFIG;
  limpar();
  texto(5, 5, 3, CYAN, F("CONFIG"));
  linha(5, 23, 123, 23, GRAY);

  if (configSelecionada < configPrimeira) configPrimeira = configSelecionada;
  if (configSelecionada >= configPrimeira + 5) configPrimeira = configSelecionada - 4;

  for (byte linhaMenu = 0; linhaMenu < 5; linhaMenu++) {
    byte item = configPrimeira + linhaMenu;
    if (item >= TOTAL_CONFIG) break;
    int y = 31 + linhaMenu * 20;
    if (item == configSelecionada) {
      linha(4, y - 3, 124, y - 3, CYAN);
      linha(4, y + 15, 124, y + 15, CYAN);
    }
    texto(8, y, 1, item == configSelecionada ? CYAN : WHITE,
          nomeOpcaoConfig(item));
  }
  texto(5, 137, 1, GRAY, F("SETAS: NAVEGAR"));
  texto(5, 148, 1, CYAN, F("SW: SELECIONAR"));
  mostrarTela();
}

void mostrarAjusteRotacao() {
  tela = AJUSTAR_ROTACAO;
  limpar();

  texto(5, 5, 3, CYAN, F("ROTACAO DA TELA"));
  linha(5, 23, 123, 23, GRAY);

  const __FlashStringHelper *opcoes[4] = {
    F("0 GRAUS"),
    F("90 GRAUS"),
    F("180 GRAUS"),
    F("270 GRAUS")
  };

  for (byte i = 0; i < 4; i++) {
    int y = 34 + i * 20;
    if (i == rotacaoTela) {
      linha(4, y - 3, 124, y - 3, CYAN);
      linha(4, y + 15, 124, y + 15, CYAN);
    }
    texto(9, y, 1, i == rotacaoTela ? CYAN : WHITE, opcoes[i]);
  }

  texto(5, 126, 1, GRAY, F("ESQ/DIR: GIRAR"));
  texto(5, 145, 1, CYAN, F("SW: VOLTAR"));
  mostrarTela();
}

void mostrarAjusteFonte() {
  tela = AJUSTAR_FONTE;
  limpar();

  texto(5, 5, 3, CYAN, F("TAMANHO DA FONTE"));
  linha(5, 23, 123, 23, GRAY);

  texto(8, 34, 1, GRAY, F("PREVIA DO TEXTO:"));
  texto(8, 58, 2, WHITE, F("BOX OS"));
  texto(8, 83, 1, WHITE, F("ABC abc 123"));

  char valor[3];
  snprintf_P(valor, sizeof(valor), PSTR("%d"), tamanhoFonte);
  texto(99, 53, 2, CYAN, valor);

  texto(5, 121, 1, GRAY, F("ESQ/DIR: ALTERAR"));
  texto(5, 145, 1, CYAN, F("SW: VOLTAR"));
  mostrarTela();
}

void desligarBOX() {
  limpar();
  texto(30, 65, 3, CYAN, F("BOX OS"));
  texto(31, 91, 2, WHITE, F("DESLIGADO"));
  mostrarTela();

  while (true) {
    delay(100);
  }
}


void reiniciarBOX() {
  for (byte repeticao = 0; repeticao < 3; repeticao++) {
    limpar();
    texto(20, 70, 2, CYAN, F("Reiniciando."));
    mostrarTela();
    delay(400);

    limpar();
    texto(20, 70, 2, CYAN, F("Reiniciando.."));
    mostrarTela();
    delay(400);

    limpar();
    texto(20, 70, 2, CYAN, F("Reiniciando..."));
    mostrarTela();
    delay(400);
  }

  wdt_enable(WDTO_15MS);

  while (true) {
  }
}


void abrirAplicativo() {
  switch (selecionado) {
    case 0: selecionadoArquivo = 0; mostrarListaArquivos(); break;
    default: configSelecionada = 0; mostrarConfig(); break;
  }
}







int enderecoSlot(byte slot) {
  return FILE_HEADER_SIZE + (int)slot * FILE_SLOT_SIZE;
}

bool arquivoExiste(byte slot) {
  if (slot >= FILE_TOTAL_SLOTS) return false;
  return EEPROM.read(enderecoSlot(slot)) == FILE_ACTIVE_MARK;
}

void escreverArquivo(byte slot, const char *nome, const char *conteudo, uint16_t tamanho) {
  if (slot >= FILE_TOTAL_SLOTS) return;
  if (tamanho > FILE_MAX_TEXT) tamanho = FILE_MAX_TEXT;

  int base = enderecoSlot(slot);
  EEPROM.update(base, 0);

  for (byte i = 0; i < FILE_NAME_SIZE; i++) {
    char c = nome[i];
    EEPROM.update(base + 1 + i, c);
    if (c == '\0') {
      for (byte j = i + 1; j < FILE_NAME_SIZE; j++) {
        EEPROM.update(base + 1 + j, 0);
      }
      break;
    }
  }

  EEPROM.update(base + 13, (byte)(tamanho & 0xFF));
  EEPROM.update(base + 14, (byte)(tamanho >> 8));

  for (uint16_t i = 0; i < tamanho; i++) {
    EEPROM.update(base + FILE_CONTENT_OFFSET + i, (byte)conteudo[i]);
  }

  EEPROM.update(base, FILE_ACTIVE_MARK);
}

void lerNomeArquivo(byte slot, char *destino) {
  int base = enderecoSlot(slot);
  for (byte i = 0; i < FILE_NAME_SIZE; i++) {
    destino[i] = (char)EEPROM.read(base + 1 + i);
  }
  destino[FILE_NAME_SIZE] = '\0';
}

void carregarArquivo(byte slot) {
  int base = enderecoSlot(slot);
  lerNomeArquivo(slot, nomeArquivoAtual);

  tamanhoConteudo = (uint16_t)EEPROM.read(base + 13);
  tamanhoConteudo |= ((uint16_t)EEPROM.read(base + 14) << 8);
  if (tamanhoConteudo > FILE_MAX_TEXT) tamanhoConteudo = 0;

  for (uint16_t i = 0; i < tamanhoConteudo; i++) {
    conteudoArquivo[i] = (char)EEPROM.read(base + FILE_CONTENT_OFFSET + i);
  }
  conteudoArquivo[tamanhoConteudo] = '\0';
  arquivoSlotAtual = slot;
}

void apagarArquivo(byte slot) {
  if (slot == 1 || slot >= FILE_TOTAL_SLOTS) return;
  EEPROM.update(enderecoSlot(slot), 0);
}

void escreverArquivoFlash(byte slot, PGM_P nome, PGM_P conteudo, uint16_t tamanho) {
  if (slot >= FILE_TOTAL_SLOTS) return;
  if (tamanho > FILE_MAX_TEXT) tamanho = FILE_MAX_TEXT;

  int base = enderecoSlot(slot);
  EEPROM.update(base, 0);
  for (byte i = 0; i < FILE_NAME_SIZE; i++) {
    byte c = pgm_read_byte(nome + i);
    EEPROM.update(base + 1 + i, c);
    if (c == 0) {
      for (byte j = i + 1; j < FILE_NAME_SIZE; j++) EEPROM.update(base + 1 + j, 0);
      break;
    }
  }
  EEPROM.update(base + 13, (byte)(tamanho & 0xFF));
  EEPROM.update(base + 14, (byte)(tamanho >> 8));
  for (uint16_t i = 0; i < tamanho; i++) {
    EEPROM.update(base + FILE_CONTENT_OFFSET + i, pgm_read_byte(conteudo + i));
  }
  EEPROM.update(base, FILE_ACTIVE_MARK);
}

void inicializarBoxFiles() {
  bool assinaturaValida =
    EEPROM.read(0) == FILE_MAGIC_0 &&
    EEPROM.read(1) == FILE_MAGIC_1 &&
    EEPROM.read(2) == FILE_MAGIC_2;

  byte versaoFormato = EEPROM.read(3);

  if (assinaturaValida && versaoFormato == 1) {
    EEPROM.update(enderecoSlot(0), 0);
    EEPROM.update(3, 2);
    return;
  }

  if (assinaturaValida && versaoFormato == 2) return;

  EEPROM.update(0, FILE_MAGIC_0);
  EEPROM.update(1, FILE_MAGIC_1);
  EEPROM.update(2, FILE_MAGIC_2);
  EEPROM.update(3, 2);
  for (byte i = 4; i < FILE_HEADER_SIZE; i++) EEPROM.update(i, 0);

  for (byte slot = 0; slot < FILE_TOTAL_SLOTS; slot++) {
    EEPROM.update(enderecoSlot(slot), 0);
  }

  static const char CREDITOS[] PROGMEM =
    "BOX OS. Criado por Jahffer. Assistencia de programacao: "
    "ChatGPT (OpenAI). Obrigado por usar o BOX OS!";

  escreverArquivoFlash(1, PSTR("CREDITOS.TXT"), CREDITOS, sizeof(CREDITOS) - 1);
}

void atualizarListaArquivos() {
  qtdArquivos = 0;
  temEspacoArquivo = false;

  for (byte slot = 0; slot < FILE_TOTAL_SLOTS; slot++) {
    if (arquivoExiste(slot)) {
      indicesArquivos[qtdArquivos++] = slot;
    } else if (slot != 1) {
      temEspacoArquivo = true;
    }
  }
}


void calcularArmazenamento(uint16_t &usados, uint16_t &capacidade) {
  usados = 0;
  capacidade = (FILE_TOTAL_SLOTS - 1) * FILE_SLOT_SIZE;

  for (byte slot = 0; slot < FILE_TOTAL_SLOTS; slot++) {
    if (slot == 1) continue;
    if (arquivoExiste(slot)) usados += FILE_SLOT_SIZE;
  }
}

void desenharBarraArmazenamento(bool mostrarDetalhes) {
  uint16_t usados = 0;
  uint16_t capacidade = 0;
  calcularArmazenamento(usados, capacidade);

  byte percentual = capacidade ? (usados * 100UL) / capacidade : 0;
  const __FlashStringHelper *cor = GREEN;
  if (percentual >= 90) cor = RED;
  else if (percentual >= 70) cor = ORANGE;
  else if (percentual >= 50) cor = YELLOW;

  texto(5, 24, 1, GRAY, F("USO EEPROM"));
  char porcentagem[6];
  snprintf_P(porcentagem, sizeof(porcentagem), PSTR("%u%%"), percentual);
  texto(96, 24, 1, cor, porcentagem);

  retangulo(5, 30, 118, 7, DARK);
  int larguraPreenchida = (118L * usados) / capacidade;
  if (larguraPreenchida > 0) {
    retangulo(5, 30, larguraPreenchida, 7, cor);
  }

  if (mostrarDetalhes) {
    char memoria[20];
    snprintf_P(memoria, sizeof(memoria), PSTR("%u/%u BYTES"), usados, capacidade);
    texto(5, 41, 1, WHITE, memoria);
  }
}

void mostrarListaArquivos() {
  tela = FILE_LIST;
  atualizarListaArquivos();
  limpar();

  texto(5, 3, 3, CYAN, F("BOX FILES"));
  linha(5, 20, 123, 20, GRAY);
  desenharBarraArmazenamento(false);

  byte totalItens = qtdArquivos + (temEspacoArquivo ? 1 : 0) + 1;
  byte itemVoltar = totalItens - 1;

  if (selecionadoArquivo >= totalItens) {
    selecionadoArquivo = itemVoltar;
  }

  byte primeiro = 0;
  if (selecionadoArquivo >= 5) {
    primeiro = selecionadoArquivo - 4;
  }

  for (byte linhaLista = 0; linhaLista < 5; linhaLista++) {
    byte item = primeiro + linhaLista;
    if (item >= totalItens) break;

    int y = 43 + linhaLista * 17;
    if (item == selecionadoArquivo) {
      linha(4, y - 3, 124, y - 3, CYAN);
      linha(4, y + 13, 124, y + 13, CYAN);
    }

    if (item < qtdArquivos) {
      char nome[FILE_NAME_SIZE + 1];
      lerNomeArquivo(indicesArquivos[item], nome);
      texto(8, y, 1, item == selecionadoArquivo ? CYAN : WHITE, nome);
    }
    else if (temEspacoArquivo && item == qtdArquivos) {
      texto(8, y, 1, item == selecionadoArquivo ? CYAN : WHITE,
            F("NOVO ARQUIVO"));
    }
    else {
      texto(8, y, 1, item == selecionadoArquivo ? CYAN : WHITE,
            F("VOLTAR"));
    }
  }

  char memoria[20];
  snprintf_P(memoria, sizeof(memoria), PSTR("%u/5 ARQUIVOS"), qtdArquivos);
  texto(5, 137, 1, GRAY, memoria);
  texto(5, 147, 1, CYAN, F("SW: ABRIR / SELECIONAR"));
  mostrarTela();
}

void criarArquivoNovo() {
  byte slotLivre = 255;
  for (byte slot = 0; slot < FILE_TOTAL_SLOTS; slot++) {
    if (slot == 1) continue;
    if (!arquivoExiste(slot)) {
      slotLivre = slot;
      break;
    }
  }

  if (slotLivre == 255) {
    somErro();
    limpar();
    texto(5, 55, 2, CYAN, F("MEMORIA CHEIA"));
    texto(5, 85, 1, WHITE, F("APAGUE UM ARQUIVO"));
    mostrarTela();
    delay(1000);
    mostrarListaArquivos();
    return;
  }

  char nome[FILE_NAME_SIZE + 1];
  snprintf_P(nome, sizeof(nome), PSTR("NOVO%02u.TXT"), slotLivre);
  escreverArquivo(slotLivre, nome, "", 0);
  somSucesso();
  carregarArquivo(slotLivre);
  editandoNomeArquivo = false;
  tecladoColuna = 0;
  tecladoLinha = 0;
  mostrarEditorArquivo();
}

void abrirArquivoSelecionado() {
  atualizarListaArquivos();

  byte totalItens = qtdArquivos + (temEspacoArquivo ? 1 : 0) + 1;
  byte itemVoltar = totalItens - 1;

  if (selecionadoArquivo >= totalItens) return;

  if (selecionadoArquivo == itemVoltar) {
    mostrarMenu();
    return;
  }

  if (temEspacoArquivo && selecionadoArquivo == qtdArquivos) {
    criarArquivoNovo();
    return;
  }

  if (selecionadoArquivo < qtdArquivos) {
    carregarArquivo(indicesArquivos[selecionadoArquivo]);
    paginaArquivo = 0;
    acaoArquivo = 0;
    mostrarLeitorArquivo();
  }
}

void mostrarLeitorArquivo() {
  tela = FILE_VIEW;
  limpar();
  texto(5, 5, 2, CYAN, nomeArquivoAtual);
  linha(5, 23, 123, 23, GRAY);

  uint16_t inicio = (uint16_t)paginaArquivo * 90;
  // A tela está em rotação 3 (160x128); reserva espaço para as ações.
  for (byte l = 0; l < 4; l++) {
    uint16_t pos = inicio + (uint16_t)l * 18;
    if (pos >= tamanhoConteudo) break;
    char trecho[19];
    byte n = 0;
    while (n < 18 && pos + n < tamanhoConteudo) {
      trecho[n] = conteudoArquivo[pos + n];
      n++;
    }
    trecho[n] = '\0';
    texto(5, 29 + l * 17, 1, WHITE, trecho);
  }

  byte totalPaginas = (tamanhoConteudo + 71) / 72;
  if (totalPaginas == 0) totalPaginas = 1;
  char paginaTexto[16];
  snprintf_P(paginaTexto, sizeof(paginaTexto), PSTR("PAG %u/%u"), paginaArquivo + 1, totalPaginas);
  texto(5, 99, 1, GRAY, paginaTexto);

  if (arquivoSlotAtual == 1) {
    texto(5, 116, 1, CYAN, F("SW: VOLTAR"));
  } else {
    const __FlashStringHelper *acao = F("VOLTAR");
    if (acaoArquivo == 0) acao = F("EDITAR");
    else if (acaoArquivo == 1) acao = F("RENOMEAR");
    else if (acaoArquivo == 2) acao = F("APAGAR");
    texto(5, 108, 1, CYAN, acao);
    texto(5, 119, 1, GRAY, F("ESQ/DIR: ACAO SW: OK"));
  }
  mostrarTela();
}

byte quantidadeTeclasQwerty(byte linhaTeclado) {
  switch (linhaTeclado) {
    case 0: return 10;
    case 1: return 9;
    case 2: return 7;
    case 3: return 10;
    default: return 0;
  }
}

int inicioLinhaQwerty(byte linhaTeclado) {
  if (linhaTeclado == 1) return 11;
  if (linhaTeclado == 2) return 23;
  return 5;
}

int centroTeclaQwerty(byte linhaTeclado, byte coluna) {
  return inicioLinhaQwerty(linhaTeclado) + coluna * 12 + 5;
}

char lerTeclaQwerty(byte linhaTeclado, byte coluna) {
  if (linhaTeclado >= TECLADO_LINHAS) return 0;
  if (coluna >= quantidadeTeclasQwerty(linhaTeclado)) return 0;
  return (char)pgm_read_byte(&TECLADO_QWERTY[linhaTeclado][coluna]);
}

byte colunaQwertyMaisProxima(byte linhaTeclado, int centroDesejado) {
  byte total = quantidadeTeclasQwerty(linhaTeclado);
  byte melhor = 0;
  int menorDistancia = 32767;

  for (byte c = 0; c < total; c++) {
    int distancia = abs(centroTeclaQwerty(linhaTeclado, c) - centroDesejado);
    if (distancia < menorDistancia) {
      menorDistancia = distancia;
      melhor = c;
    }
  }

  return melhor;
}

byte acaoQwertyMaisProxima(int centroDesejado) {
  const int centrosAcoes[3] = {20, 64, 108};
  byte melhor = 0;
  int menorDistancia = 32767;

  for (byte i = 0; i < 3; i++) {
    int distancia = abs(centrosAcoes[i] - centroDesejado);
    if (distancia < menorDistancia) {
      menorDistancia = distancia;
      melhor = i;
    }
  }

  return melhor;
}

void desenharItemTeclado(byte row, byte col, bool ativo) {

  if (row == TECLADO_LINHA_ACOES) {
    int x = (col == ACAO_DEL) ? 5 : (col == ACAO_ESPACO ? 40 : 93);
    int largura = (col == ACAO_ESPACO) ? 48 : 30;
    const char *rotulo = (col == ACAO_DEL) ? "DEL" :
                         (col == ACAO_ESPACO ? "ESPACO" : "OK");
    retanguloArredondado(x, 113, largura, 12, 2, ativo ? CYAN : DARK);
    textoCentro(x + largura / 2, 119, 1, ativo ? CYAN : WHITE, rotulo);
    return;
  }

  if (row >= TECLADO_LINHAS || col >= quantidadeTeclasQwerty(row)) return;
  int x = inicioLinhaQwerty(row) + col * 12;
  int y = 59 + row * 13;
  retanguloArredondado(x, y, 10, 11, 2, ativo ? CYAN : DARK);
  char etiqueta[2] = { lerTeclaQwerty(row, col), '\0' };
  textoCentro(x + 5, y + 6, 1, ativo ? CYAN : WHITE, etiqueta);
}

void mostrarEditorArquivo() {
  tela = FILE_EDIT;
  limpar();

  texto(5, 4, 2, CYAN,
        editandoNomeArquivo ? F("RENOMEAR") : F("EDITAR ARQUIVO"));

  texto(5, 21, 1, GRAY, nomeArquivoAtual);

  char trecho[19];
  byte n = 0;

  if (editandoNomeArquivo) {
    while (n < 18 && n < tamanhoNomeTemporario) {
      trecho[n] = nomeTemporario[n];
      n++;
    }
  } else {
    uint16_t inicio = tamanhoConteudo > 18 ? tamanhoConteudo - 18 : 0;
    while (n < 18 && inicio + n < tamanhoConteudo) {
      trecho[n] = conteudoArquivo[inicio + n];
      n++;
    }
  }
  trecho[n] = '\0';
  texto(5, 34, 1, WHITE, trecho);

  char contador[20];
  if (editandoNomeArquivo) {
    snprintf_P(contador, sizeof(contador), PSTR("%u/8 LETRAS"), tamanhoNomeTemporario);
  } else {
    snprintf_P(contador, sizeof(contador), PSTR("%u/%u CARACTERES"), tamanhoConteudo, FILE_MAX_TEXT);
  }
  texto(5, 46, 1, GRAY, contador);

  for (byte row = 0; row < TECLADO_LINHAS; row++) {
    for (byte col = 0; col < quantidadeTeclasQwerty(row); col++) {
      desenharItemTeclado(row, col, (tecladoLinha == row && tecladoColuna == col));
    }
  }
  for (byte col = 0; col < 3; col++) {
    desenharItemTeclado(TECLADO_LINHA_ACOES, col, (tecladoLinha == TECLADO_LINHA_ACOES && tecladoColuna == col));
  }

  texto(4, 132, 1, GRAY, F("JOY: MOVER  SW: USAR"));
  texto(4, 144, 1, CYAN, F("OK SALVA  DEL APAGA"));
  mostrarTela();
}

bool nomeArquivoEmUso(const char *nome, byte ignorarSlot) {
  for (byte slot = 0; slot < FILE_TOTAL_SLOTS; slot++) {
    if (slot == ignorarSlot || !arquivoExiste(slot)) continue;
    char existente[FILE_NAME_SIZE + 1];
    lerNomeArquivo(slot, existente);
    if (strcmp(nome, existente) == 0) return true;
  }
  return false;
}


// Atualiza somente a célula de pixels da nova letra no campo de pré-visualização.
// Quando o texto ultrapassa 18 caracteres, a linha precisa rolar e é redesenhada.
void atualizarPreviewAposDigitar(char caractere) {
  uint16_t tamanhoAtual = editandoNomeArquivo ? tamanhoNomeTemporario : tamanhoConteudo;
  byte tamanhoVisivel = tamanhoAtual > 18 ? 18 : (byte)tamanhoAtual;

  byte escala = ajustarTamanhoFonte(1);
  while (escala > 1 && (int)tamanhoVisivel * 6 * escala > displayTFT.width() - 10) {
    escala--;
  }

  if (tamanhoAtual <= 18) {
    int x = 5 + ((int)tamanhoAtual - 1) * 6 * escala;
    char letra[2] = {caractere, '\0'};
    desenharTextoTFT(x, 34, escala, corTFT(WHITE), letra, false);
    return;
  }

  // A linha visível mudou de conteúdo: redesenha apenas a faixa de pré-visualização.
  displayTFT.fillRect(5, 34, displayTFT.width() - 10, 8 * escala, ST77XX_BLACK);
  char trecho[19];
  byte inicio = (byte)(tamanhoAtual - 18);
  for (byte i = 0; i < 18; i++) {
    trecho[i] = editandoNomeArquivo ? nomeTemporario[inicio + i]
                                    : conteudoArquivo[inicio + i];
  }
  trecho[18] = '\0';
  desenharTextoTFT(5, 34, escala, corTFT(WHITE), trecho, false);
}

void selecionarTeclaEditor() {
  if (tecladoLinha == TECLADO_LINHA_ACOES) {
    if (tecladoColuna == ACAO_DEL) {
      if (editandoNomeArquivo) {
        if (tamanhoNomeTemporario > 0) {
          nomeTemporario[--tamanhoNomeTemporario] = '\0';
        }
      } else if (tamanhoConteudo > 0) {
        tamanhoConteudo--;
        conteudoArquivo[tamanhoConteudo] = '\0';
      }

      mostrarEditorArquivo();
      return;
    }

    if (tecladoColuna == ACAO_ESPACO) {
      if (!editandoNomeArquivo && tamanhoConteudo < FILE_MAX_TEXT) {
        conteudoArquivo[tamanhoConteudo++] = ' ';
        conteudoArquivo[tamanhoConteudo] = '\0';
      }

      mostrarEditorArquivo();
      return;
    }

    if (editandoNomeArquivo) {
      if (tamanhoNomeTemporario == 0) return;

      char novoNome[FILE_NAME_SIZE + 1];

      for (byte i = 0; i < tamanhoNomeTemporario; i++) {
        novoNome[i] = nomeTemporario[i];
      }
      strcpy(novoNome + tamanhoNomeTemporario, ".TXT");

      if (nomeArquivoEmUso(novoNome, arquivoSlotAtual)) {
        somErro();
        limpar();
        texto(8, 60, 1, CYAN, F("NOME JA EXISTE"));
        mostrarTela();
        delay(800);
        mostrarEditorArquivo();
        return;
      }

      escreverArquivo(
        arquivoSlotAtual,
        novoNome,
        conteudoArquivo,
        tamanhoConteudo
      );
      somSucesso();

      editandoNomeArquivo = false;
      carregarArquivo(arquivoSlotAtual);
      mostrarLeitorArquivo();
    } else {
      escreverArquivo(
        arquivoSlotAtual,
        nomeArquivoAtual,
        conteudoArquivo,
        tamanhoConteudo
      );
      somSucesso();

      carregarArquivo(arquivoSlotAtual);
      paginaArquivo = 0;
      acaoArquivo = 0;
      mostrarLeitorArquivo();
    }

    return;
  }

  char tecla = lerTeclaQwerty(tecladoLinha, tecladoColuna);
  if (tecla == 0) return;

  bool adicionou = false;
  if (editandoNomeArquivo) {
    if (tamanhoNomeTemporario < 8) {
      nomeTemporario[tamanhoNomeTemporario++] = tecla;
      nomeTemporario[tamanhoNomeTemporario] = '\0';
      adicionou = true;
    }
  } else if (tamanhoConteudo < FILE_MAX_TEXT) {
    conteudoArquivo[tamanhoConteudo++] = tecla;
    conteudoArquivo[tamanhoConteudo] = '\0';
    adicionou = true;
  }

  if (adicionou) {
    // Não redesenha o teclado nem o editor inteiro: somente a nova letra.
    atualizarPreviewAposDigitar(tecla);
  }
}

void executarAcaoLeitor() {
  if (arquivoSlotAtual == 1) {
    mostrarListaArquivos();
    return;
  }

  if (acaoArquivo == 0) {
    editandoNomeArquivo = false;
    tecladoColuna = 0;
    tecladoLinha = 0;
    mostrarEditorArquivo();
  }
  else if (acaoArquivo == 1) {
    editandoNomeArquivo = true;
    tamanhoNomeTemporario = 0;
    for (byte i = 0; i < FILE_NAME_SIZE && nomeArquivoAtual[i] != '.' && nomeArquivoAtual[i] != '\0'; i++) {
      if (tamanhoNomeTemporario < 8) nomeTemporario[tamanhoNomeTemporario++] = nomeArquivoAtual[i];
    }
    nomeTemporario[tamanhoNomeTemporario] = '\0';
    tecladoColuna = 0;
    tecladoLinha = 0;
    mostrarEditorArquivo();
  }
  else if (acaoArquivo == 2) {
    apagarArquivo(arquivoSlotAtual);
    somApagar();
    atualizarListaArquivos();
    if (selecionadoArquivo >= qtdArquivos + (temEspacoArquivo ? 1 : 0)) {
      selecionadoArquivo = 0;
    }
    mostrarListaArquivos();
  }
  else {
    mostrarListaArquivos();
  }
}


bool pulando = false;
bool gameOver = false;

unsigned long inicioPulo = 0;
unsigned long inicioObstaculo = 0;
unsigned long ultimaFrameDino = 0;

int obstaculoX = 120;
int pontos = 0;

void selecionarConfig() {
  switch (configSelecionada) {
    case 0: mostrarAjusteFonte(); break;
    case 1: mostrarAjusteRotacao(); break;
    case 2: desligarBOX(); break;
    case 3: reiniciarBOX(); break;
    default: mostrarMenu(); break;
  }
}

void atualizarNavegacao() {
  if (millis() - ultimoMovimento < 110) return;

  if (tela == FILE_EDIT) {
    int dx = lerDirecaoX();
    int dy = lerDirecaoY();
    if (dx == 0 && dy == 0) return;

    byte linhaAnterior = tecladoLinha;
    byte colunaAnterior = tecladoColuna;

    if (tecladoLinha == TECLADO_LINHA_ACOES) {
      if (dx != 0) {
        int novaAcao = (int)tecladoColuna + dx;
        if (novaAcao < 0) novaAcao = 0;
        if (novaAcao > 2) novaAcao = 2;
        tecladoColuna = novaAcao;
      }

      if (dy < 0) {
        const int centrosAcoes[3] = {20, 64, 108};
        int centroAtual = centrosAcoes[tecladoColuna];
        tecladoLinha = 3;
        tecladoColuna = colunaQwertyMaisProxima(3, centroAtual);
      }
    } else {
      if (dx != 0) {
        int novaColuna = (int)tecladoColuna + dx;
        int total = quantidadeTeclasQwerty(tecladoLinha);
        if (novaColuna < 0) novaColuna = 0;
        if (novaColuna >= total) novaColuna = total - 1;
        tecladoColuna = novaColuna;
      }

      if (dy != 0) {
        int centroAtual = centroTeclaQwerty(tecladoLinha, tecladoColuna);

        if (dy < 0 && tecladoLinha > 0) {
          tecladoLinha--;
          tecladoColuna = colunaQwertyMaisProxima(tecladoLinha, centroAtual);
        } else if (dy > 0 && tecladoLinha < 3) {
          tecladoLinha++;
          tecladoColuna = colunaQwertyMaisProxima(tecladoLinha, centroAtual);
        } else if (dy > 0 && tecladoLinha == 3) {
          tecladoLinha = TECLADO_LINHA_ACOES;
          tecladoColuna = acaoQwertyMaisProxima(centroAtual);
        }
      }
    }

    ultimoMovimento = millis();
    byte novaLinha = tecladoLinha;
    byte novaColuna = tecladoColuna;
    if (linhaAnterior != novaLinha || colunaAnterior != novaColuna) {
      // Redesenha a tecla antiga como NÃO selecionada e a nova como selecionada.
      // O estado global já contém a nova posição; por isso passamos o estado visual explicitamente.
      desenharItemTeclado(linhaAnterior, colunaAnterior, false);
      desenharItemTeclado(novaLinha, novaColuna, true);
    }
    return;
  }

  if (tela == FILE_LIST) {
    int dy = lerDirecaoY();
    if (dy == 0) return;
    atualizarListaArquivos();
    byte totalItens = qtdArquivos + (temEspacoArquivo ? 1 : 0) + 1;
    selecionadoArquivo = (selecionadoArquivo + totalItens + dy) % totalItens;
    ultimoMovimento = millis();
    mostrarListaArquivos();
    return;
  }

  if (tela == FILE_VIEW) {
    int dx = lerDirecaoX();
    int dy = lerDirecaoY();
    if (dx == 0 && dy == 0) return;
    if (arquivoSlotAtual != 1 && dx != 0) {
      acaoArquivo = (acaoArquivo + 4 + dx) % 4;
    }
    if (dy < 0 && paginaArquivo > 0) paginaArquivo--;
    if (dy > 0) {
      byte totalPaginas = (tamanhoConteudo + 89) / 90;
      if (totalPaginas == 0) totalPaginas = 1;
      if (paginaArquivo + 1 < totalPaginas) paginaArquivo++;
    }
    ultimoMovimento = millis();
    mostrarLeitorArquivo();
    return;
  }

  int direcao;
  if (tela == AJUSTAR_FONTE || tela == AJUSTAR_ROTACAO) {
    direcao = lerDirecaoX();
    if (direcao == 0) return;
  } else {
    direcao = lerDirecao();
    if (direcao == 0) return;
  }

  ultimoMovimento = millis();
  somNavegacao();

  if (tela == MENU) {
    selecionado = (selecionado + TOTAL_APPS + direcao) % TOTAL_APPS;
    mostrarMenu();
  }
  else if (tela == CONFIG) {
    configSelecionada = (configSelecionada + TOTAL_CONFIG + direcao) % TOTAL_CONFIG;
    mostrarConfig();
  }
  else if (tela == AJUSTAR_FONTE) {
    if (direcao < 0 && tamanhoFonte > 1) tamanhoFonte--;
    if (direcao > 0 && tamanhoFonte < 4) tamanhoFonte++;
    salvarConfiguracoes();
    mostrarAjusteFonte();
  }
  else if (tela == AJUSTAR_ROTACAO) {
    rotacaoTela = (rotacaoTela + 4 + direcao) % 4;
    displayTFT.setRotation(rotacaoTela);
    salvarConfiguracoes();
    mostrarAjusteRotacao();
  }
}

void atualizarBotao() {
  if (!botaoPressionado()) return;

  switch (tela) {
    case MENU:
      abrirAplicativo();
      break;
    case CONFIG:
      selecionarConfig();
      break;
    case AJUSTAR_FONTE:
    case AJUSTAR_ROTACAO:
      mostrarConfig();
      break;
    case FILE_LIST:
      abrirArquivoSelecionado();
      break;
    case FILE_VIEW:
      executarAcaoLeitor();
      break;
    case FILE_EDIT:
      selecionarTeclaEditor();
      break;
  }
}





void setup() {
  wdt_disable();
  pinMode(JOY_X, INPUT);
  pinMode(JOY_Y, INPUT);
  pinMode(JOY_SW, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  displayTFT.initR(INITR_BLACKTAB);
  displayTFT.setRotation(3);
  displayTFT.fillScreen(ST77XX_BLACK);
  calibrarJoystick();
  inicializarBoxFiles();
  carregarConfiguracoes();
  displayTFT.setRotation(rotacaoTela);
  iniciado = true;
  somInicializacao();
  mostrarMenu();
}


void loop() {
  if (!iniciado) return;
  atualizarNavegacao();
  atualizarBotao();
}
