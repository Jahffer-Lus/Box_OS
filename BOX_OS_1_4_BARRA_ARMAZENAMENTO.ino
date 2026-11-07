
#include <avr/wdt.h>
#include <EEPROM.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

struct DataHoraRTC;


#define JOY_X A0
#define JOY_Y A1
#define JOY_SW 2
#define BUZZER_PIN 6  // Buzzer passivo: positivo no D6, negativo no GND

#define RTC_IO 12
#define RTC_SCLK 13
#define RTC_CE 11

ThreeWire rtcFio(RTC_IO, RTC_SCLK, RTC_CE);
#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST 8
#define TFT_MOSI 4
#define TFT_SCLK 5
Adafruit_ST7735 displayTFT(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
RtcDS1302<ThreeWire> Rtc(rtcFio);

#define CYAN  F("#55E6FF")
#define WHITE F("#FFFFFF")
#define GRAY  F("#AABBC8")
#define DARK  F("#28445E")
#define GREEN F("#36D17C")
#define YELLOW F("#F2D34F")
#define ORANGE F("#F39A38")
#define RED F("#F04B4B")

const byte TOTAL_APPS = 5;
const byte TOTAL_CONFIG = 7;

const __FlashStringHelper *nomeApp(byte indice) {
  switch (indice) {
    case 0: return F("JOGOS");
    case 1: return F("TERMINAL");
    case 2: return F("MONITOR");
    case 3: return F("CONFIG");
    default: return F("BOX FILES");
  }
}

const __FlashStringHelper *iconeApp(byte indice) {
  switch (indice) {
    case 0: return F("J");
    case 1: return F("T");
    case 2: return F("M");
    case 3: return F("C");
    default: return F("F");
  }
}

const __FlashStringHelper *nomeOpcaoConfig(byte indice) {
  switch (indice) {
    case 0: return F("Tamanho da Fonte");
    case 1: return F("Tamanho dos Icones");
    case 2: return F("Ajustar Data");
    case 3: return F("Ajustar Hora");
    case 4: return F("Desligar");
    case 5: return F("Reiniciar");
    default: return F("Voltar");
  }
}


enum Screen {
  MENU,
  JOGOS,
  TERMINAL,
  MONITOR,
  CONFIG,
  DINO,
  AJUSTAR_FONTE,
  AJUSTAR_ICONES,
  AJUSTAR_DATA,
  AJUSTAR_HORA,
  TERM_EDIT,
  FILE_LIST,
  FILE_VIEW,
  FILE_EDIT
};

Screen tela = MENU;

byte selecionado = 0;
byte configSelecionada = 0;

byte tamanhoFonte = 2;
byte tamanhoIcone = 2;
byte configPrimeira = 0;

const int CONFIG_EEPROM_BASE = 1008;
const byte CONFIG_MAGIC = 0xB7;

struct DataHoraRTC {
  byte segundo;
  byte minuto;
  byte hora;
  byte dia;
  byte mes;
  byte ano;
  byte diaSemana;
};

DataHoraRTC dataHoraAtual;
DataHoraRTC dataHoraEdicao;
DataHoraRTC dataHoraMenuAnterior;
bool rtcDisponivel = false;
bool relogioMenuInicializado = false;
bool relogioMenuDisponivel = false;
char entradaDataHora[11] = "";
byte tamanhoEntradaDataHora = 0;

char terminalEntrada[17] = "";
byte terminalTamanho = 0;
char terminalResposta[25] = "DIGITE AJUDA";
bool terminalTecladoMinimizado = false;

int centroX = 512;
int centroY = 512;

const int LIMITE_JOYSTICK = 130;

unsigned long ultimoMovimento = 0;
unsigned long ultimoBotao = 0;
unsigned long ultimaAtualizacao = 0;
unsigned long ultimaAtualizacaoRelogio = 0;

bool iniciado = false;
bool ultimoEstadoBotao = HIGH;

unsigned long inicioSistema = 0;

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

void mostrarJogos();
void mostrarTerminal();
void mostrarMonitor();
void atualizarMonitor();
void mostrarListaArquivos();
void mostrarLeitorArquivo();
void mostrarEditorArquivo();
void mostrarTerminal();
void mostrarConfig();
void mostrarAjusteData();
void mostrarAjusteHora();

const char TECLADO_QWERTY[4][11] PROGMEM = {
  "QWERTYUIOP",  // 10 teclas
  "ASDFGHJKL",   // 9 teclas
  "ZXCVBNM",     // 7 teclas
  "1234567890"   // 10 teclas
};

const byte TECLADO_LINHAS = 4;
const byte TECLADO_LINHA_ACOES = 4;
const byte ACAO_DEL = 0;
const byte ACAO_ESPACO = 1;
const byte ACAO_OK = 2;


void limpar() {
  displayTFT.fillScreen(ST77XX_BLACK);
}


bool rtcLerDataHora(DataHoraRTC &dh) {
  if (!Rtc.IsDateTimeValid()) return false;

  RtcDateTime agora = Rtc.GetDateTime();

  dh.segundo = agora.Second();
  dh.minuto = agora.Minute();
  dh.hora = agora.Hour();
  dh.dia = agora.Day();
  dh.mes = agora.Month();
  dh.ano = agora.Year() - 2000;
  dh.diaSemana = agora.DayOfWeek();

  return dh.segundo < 60 && dh.minuto < 60 && dh.hora < 24 &&
         dh.dia >= 1 && dh.dia <= 31 && dh.mes >= 1 && dh.mes <= 12 &&
         dh.ano <= 99;
}

void rtcGravarDataHora(const DataHoraRTC &dh) {
  Rtc.SetIsWriteProtected(false);
  Rtc.SetIsRunning(true);
  Rtc.SetDateTime(RtcDateTime(
    2000 + dh.ano,
    dh.mes,
    dh.dia,
    dh.hora,
    dh.minuto,
    dh.segundo
  ));
  Rtc.SetIsWriteProtected(true);
}

void iniciarRTC() {
  Rtc.Begin();

  if (!Rtc.GetIsRunning()) {
    Rtc.SetIsRunning(true);
  }

  rtcDisponivel = rtcLerDataHora(dataHoraAtual);

  if (!rtcDisponivel) {
    dataHoraAtual.segundo = 0;
    dataHoraAtual.minuto = 0;
    dataHoraAtual.hora = 12;
    dataHoraAtual.dia = 1;
    dataHoraAtual.mes = 1;
    dataHoraAtual.ano = 26;
    dataHoraAtual.diaSemana = 1;
  }
}


void salvarConfiguracoes() {
  EEPROM.update(CONFIG_EEPROM_BASE, CONFIG_MAGIC);
  EEPROM.update(CONFIG_EEPROM_BASE + 1, 1); // versao
  EEPROM.update(CONFIG_EEPROM_BASE + 2, tamanhoFonte);
  EEPROM.update(CONFIG_EEPROM_BASE + 3, tamanhoIcone);
  EEPROM.update(CONFIG_EEPROM_BASE + 4,
    (byte)(CONFIG_MAGIC ^ tamanhoFonte ^ tamanhoIcone));
}

void carregarConfiguracoes() {
  byte assinatura = EEPROM.read(CONFIG_EEPROM_BASE);
  byte versao = EEPROM.read(CONFIG_EEPROM_BASE + 1);
  byte fonte = EEPROM.read(CONFIG_EEPROM_BASE + 2);
  byte icone = EEPROM.read(CONFIG_EEPROM_BASE + 3);
  byte checksum = EEPROM.read(CONFIG_EEPROM_BASE + 4);

  if (assinatura == CONFIG_MAGIC && versao == 1 &&
      fonte >= 1 && fonte <= 4 && icone >= 1 && icone <= 4 &&
      checksum == (byte)(CONFIG_MAGIC ^ fonte ^ icone)) {
    tamanhoFonte = fonte;
    tamanhoIcone = icone;
  } else {
    tamanhoFonte = 2;
    tamanhoIcone = 2;
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

void somPulo() {
  tone(BUZZER_PIN, 980, 85);
}

void somGameOver() {
  tone(BUZZER_PIN, 520, 130);
  delay(140);
  tone(BUZZER_PIN, 330, 220);
  delay(230);
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
  displayTFT.setTextSize(tamanho);
  displayTFT.setTextColor(cor, ST77XX_BLACK);
  int16_t x1, y1;
  uint16_t largura, altura;
  displayTFT.getTextBounds(mensagem, 0, 0, &x1, &y1, &largura, &altura);
  if (centralizar) x -= largura / 2;
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

void textoIcone(int x, int y, int tamanho, const __FlashStringHelper *cor, const __FlashStringHelper *mensagem) {
  int ajustado = (int)tamanho + (int)tamanhoIcone - 2;
  if (ajustado < 1) ajustado = 1;
  if (ajustado > 4) ajustado = 4;
  desenharTextoFlashTFT(x, y, ajustado, corTFT(cor), mensagem, false);
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

void textoIconeCentro(int x, int y, int tamanho, const __FlashStringHelper *cor, const __FlashStringHelper *mensagem) {
  int ajustado = (int)tamanho + (int)tamanhoIcone - 2;
  if (ajustado < 1) ajustado = 1;
  if (ajustado > 4) ajustado = 4;
  desenharTextoFlashTFT(x, y - (ajustado * 4), ajustado, corTFT(cor), mensagem, true);
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

  float ca = cos(angulo);
  float sa = sin(angulo);
  float cb = cos(angulo * 0.65);
  float sb = sin(angulo * 0.65);

  for (byte i = 0; i < 8; i++) {
    float x = pgm_read_float(&vertices[i][0]) * 23;
    float y = pgm_read_float(&vertices[i][1]) * 23;
    float z = pgm_read_float(&vertices[i][2]) * 23;

    float xr = x * ca - z * sa;
    float zr = x * sa + z * ca;
    float yr = y * cb - zr * sb;

    px[i] = 64 + (int)xr;
    py[i] = 78 - (int)yr - (int)pulo;
  }

  limpar();
  texto(37, 8, 3, CYAN, F("BOX OS"));

  for (byte i = 0; i < 12; i++) {
    byte a = pgm_read_byte(&arestas[i][0]);
    byte b = pgm_read_byte(&arestas[i][1]);

    linha(px[a], py[a], px[b], py[b], CYAN);
  }
}

void animacaoInicio() {
  for (byte frame = 0; frame < 30; frame++) {
    float p = frame / 29.0;
    float angulo = p * 6.28318;
    float pulo = 28.0 * sin(3.14159 * p);

    desenharCubo(angulo, pulo);
    delay(100);
  }
}


void mostrarDataHoraMenu() {
  retangulo(4, 23, 122, 14, F("#000000"));

  if (!rtcLerDataHora(dataHoraAtual)) {
    relogioMenuInicializado = true;
    relogioMenuDisponivel = false;
    texto(5, 25, 1, RED, F("RTC NAO DETECTADO"));
    return;
  }

  dataHoraMenuAnterior = dataHoraAtual;
  relogioMenuDisponivel = true;
  relogioMenuInicializado = true;

  char dataHoraTexto[21];
  snprintf_P(
    dataHoraTexto, sizeof(dataHoraTexto),
    PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
    dataHoraAtual.dia,
    dataHoraAtual.mes,
    2000 + dataHoraAtual.ano,
    dataHoraAtual.hora,
    dataHoraAtual.minuto,
    dataHoraAtual.segundo
  );
  texto(5, 25, 1, WHITE, dataHoraTexto);
}

bool dataHoraMenuMudou(const DataHoraRTC &a, const DataHoraRTC &b) {
  return a.segundo != b.segundo ||
         a.minuto != b.minuto ||
         a.hora != b.hora ||
         a.dia != b.dia ||
         a.mes != b.mes ||
         a.ano != b.ano ||
         a.diaSemana != b.diaSemana;
}

void mostrarMenu() {
  tela = MENU;
  limpar();

  texto(5, 3, 3, CYAN, F("APLICATIVOS"));
  mostrarDataHoraMenu();

  linha(5, 38, 123, 38, GRAY);

  for (byte i = 0; i < TOTAL_APPS; i++) {
    int y = 42 + i * 18;

    if (i == selecionado) {
      linha(5, y - 3, 123, y - 3, CYAN);
      linha(5, y + 17, 123, y + 17, CYAN);
    }

    retanguloArredondado(
      8, y - 1, 18, 18, 4,
      i == selecionado ? CYAN : DARK
    );
    textoIconeCentro(17, y + 8, 2, WHITE, iconeApp(i));
    texto(38, y + 1, 2, i == selecionado ? CYAN : WHITE, nomeApp(i));
  }

  linha(5, 137, 123, 137, DARK);
  texto(5, 141, 1, GRAY, F("SETAS: NAVEGAR"));
  texto(5, 151, 1, CYAN, F("SW: ABRIR"));
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
}

void desenharTecladoDataHora() {
  const int xTeclas[3] = {20, 55, 90};
  const int yTeclas[3] = {59, 76, 93};

  for (byte row = 0; row < 3; row++) {
    for (byte col = 0; col < 3; col++) {
      bool ativo = (tecladoLinha == row && tecladoColuna == col);
      int x = xTeclas[col];
      int y = yTeclas[row];
      retanguloArredondado(x, y, 25, 14, 3, ativo ? CYAN : DARK);
      char tecla[2] = {(char)('1' + row * 3 + col), '\0'};
      textoCentro(x + 12, y + 7, 1, ativo ? CYAN : WHITE, tecla);
    }
  }

  bool a0 = tecladoLinha == TECLADO_LINHA_ACOES && tecladoColuna == 0;
  bool a1 = tecladoLinha == TECLADO_LINHA_ACOES && tecladoColuna == 1;
  bool a2 = tecladoLinha == TECLADO_LINHA_ACOES && tecladoColuna == 2;
  bool a3 = tecladoLinha == TECLADO_LINHA_ACOES && tecladoColuna == 3;

  retanguloArredondado(3, 116, 25, 12, 2, a0 ? CYAN : DARK);
  textoCentro(15, 122, 1, a0 ? CYAN : WHITE, F("DEL"));
  retanguloArredondado(30, 116, 38, 12, 2, a1 ? CYAN : DARK);
  textoCentro(49, 122, 1, a1 ? CYAN : WHITE, F("0"));
  retanguloArredondado(70, 116, 30, 12, 2, a2 ? CYAN : DARK);
  textoCentro(85, 122, 1, a2 ? CYAN : WHITE, F("ENTER"));
  retanguloArredondado(102, 116, 24, 12, 2, a3 ? CYAN : DARK);
  textoCentro(114, 122, 1, a3 ? CYAN : WHITE, F("SAIR"));
}

void mostrarAjusteData() {
  tela = AJUSTAR_DATA;
  limpar();
  texto(5, 4, 2, CYAN, F("AJUSTAR DATA"));
  linha(5, 21, 123, 21, GRAY);

  if (!rtcDisponivel) {
    texto(5, 55, 1, WHITE, F("MODULO DE RELOGIO NAO DETECTADO"));
    texto(5, 75, 1, GRAY, F("VERIFIQUE DAT/CLK/RST"));
    texto(5, 145, 1, CYAN, F("SW: VOLTAR"));
    return;
  }

  texto(5, 27, 1, GRAY, F("DD/MM/AAAA:"));
  texto(62, 27, 1, WHITE, entradaDataHora);
  texto(5, 43, 1, GRAY, F("DIGITE DIA/MES/ANO"));
  desenharTecladoDataHora();
  texto(5, 137, 1, GRAY, F("JOY: MOVER  SW: USAR"));
  texto(5, 149, 1, CYAN, F("ENTER SALVA  DEL APAGA"));
}

void mostrarAjusteHora() {
  tela = AJUSTAR_HORA;
  limpar();
  texto(5, 4, 2, CYAN, F("AJUSTAR HORA"));
  linha(5, 21, 123, 21, GRAY);

  if (!rtcDisponivel) {
    texto(5, 55, 1, WHITE, F("MODULO DE RELOGIO NAO DETECTADO"));
    texto(5, 75, 1, GRAY, F("VERIFIQUE DAT/CLK/RST"));
    texto(5, 145, 1, CYAN, F("SW: VOLTAR"));
    return;
  }

  texto(5, 27, 1, GRAY, F("HHMMSS:"));
  texto(62, 27, 1, WHITE, entradaDataHora);
  texto(5, 43, 1, GRAY, F("DIGITE 6 NUMEROS"));
  desenharTecladoDataHora();
  texto(5, 137, 1, GRAY, F("JOY: MOVER  SW: USAR"));
  texto(5, 149, 1, CYAN, F("ENTER SALVA  DEL APAGA"));
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
}

void mostrarAjusteIcones() {
  tela = AJUSTAR_ICONES;
  limpar();

  texto(5, 5, 3, CYAN, F("TAMANHO DOS ICONES"));
  linha(5, 23, 123, 23, GRAY);

  texto(8, 34, 1, GRAY, F("PREVIA DO ICONE:"));
  textoIcone(18, 60, tamanhoIcone, WHITE, F("🎮"));
  texto(55, 69, 2, CYAN, F("JOGOS"));

  char valor[3];
  snprintf_P(valor, sizeof(valor), PSTR("%d"), tamanhoIcone);
  texto(99, 53, 2, CYAN, valor);

  texto(5, 121, 1, GRAY, F("ESQ/DIR: ALTERAR"));
  texto(5, 145, 1, CYAN, F("SW: VOLTAR"));
}


void desligarBOX() {
  limpar();
  texto(30, 65, 3, CYAN, F("BOX OS"));
  texto(31, 91, 2, WHITE, F("DESLIGADO"));

  while (true) {
    delay(100);
  }
}


void reiniciarBOX() {
  for (byte repeticao = 0; repeticao < 3; repeticao++) {
    limpar();
    texto(20, 70, 2, CYAN, F("Reiniciando."));
    delay(400);

    limpar();
    texto(20, 70, 2, CYAN, F("Reiniciando.."));
    delay(400);

    limpar();
    texto(20, 70, 2, CYAN, F("Reiniciando..."));
    delay(400);
  }

  wdt_enable(WDTO_15MS);

  while (true) {
  }
}


void abrirAplicativo() {
  switch (selecionado) {

    case 0:
      mostrarJogos();
      break;

    case 1:
      terminalEntrada[0] = '\0';
      terminalTamanho = 0;
      tecladoLinha = 0;
      tecladoColuna = 0;
      terminalTecladoMinimizado = false;
      strcpy(terminalResposta, "DIGITE AJUDA");
      mostrarTerminal();
      break;

    case 2:
      mostrarMonitor();
      break;

    case 3:
      configSelecionada = 0;
      mostrarConfig();
      break;

    case 4:
      selecionadoArquivo = 0;
      mostrarListaArquivos();
      break;
  }
}

void mostrarJogos() {
  tela = JOGOS;
  limpar();

  texto(5, 5, 3, CYAN, F("JOGOS"));
  linha(5, 23, 123, 23, GRAY);

  textoIcone(9, 53, tamanhoIcone, WHITE, F("🦖"));
  texto(39, 56, 2, CYAN, F("DINO_RUN"));

  linha(5, 138, 123, 138, DARK);
  texto(5, 145, 1, CYAN, F("SW: ABRIR"));
}

void mostrarTerminal() {
  tela = TERM_EDIT;
  limpar();
  texto(5, 4, 2, CYAN, F("TERMINAL"));
  linha(5, 21, 123, 21, GRAY);
  texto(5, 27, 1, GRAY, F("BOX>"));
  texto(32, 27, 1, WHITE, terminalEntrada);
  texto(5, 43, 1, CYAN, terminalResposta);

  if (terminalTecladoMinimizado) {
    textoCentro(64, 82, 2, CYAN, F("TECLADO OCULTO"));
    textoCentro(64, 106, 1, WHITE, F("SW: MOSTRAR TECLADO"));
    texto(5, 149, 1, GRAY, F("RESULTADOS DO TERMINAL"));
    return;
  }

  for (byte row = 0; row < 4; row++) {
    byte total = quantidadeTeclasQwerty(row);
    int xInicio = inicioLinhaQwerty(row);
    int y = 62 + row * 13;
    for (byte col = 0; col < total; col++) {
      int x = xInicio + col * 12;
      bool ativo = (tecladoLinha == row && tecladoColuna == col);
      retanguloArredondado(x, y, 10, 11, 2, ativo ? CYAN : DARK);
      char letra[2] = { lerTeclaQwerty(row, col), '\0' };
      textoCentro(x + 5, y + 6, 1, ativo ? CYAN : WHITE, letra);
    }
  }

  bool a0 = tecladoLinha == TECLADO_LINHA_ACOES && tecladoColuna == 0;
  bool a1 = tecladoLinha == TECLADO_LINHA_ACOES && tecladoColuna == 1;
  bool a2 = tecladoLinha == TECLADO_LINHA_ACOES && tecladoColuna == 2;
  bool a3 = tecladoLinha == TECLADO_LINHA_ACOES && tecladoColuna == 3;
  bool a4 = tecladoLinha == TECLADO_LINHA_ACOES && tecladoColuna == 4;
  retanguloArredondado(3, 116, 25, 12, 2, a0 ? CYAN : DARK);
  textoCentro(15, 122, 1, a0 ? CYAN : WHITE, F("DEL"));
  retanguloArredondado(30, 116, 38, 12, 2, a1 ? CYAN : DARK);
  textoCentro(49, 122, 1, a1 ? CYAN : WHITE, F("ESP"));
  retanguloArredondado(70, 116, 30, 12, 2, a2 ? CYAN : DARK);
  textoCentro(85, 122, 1, a2 ? CYAN : WHITE, F("ENTER"));
  retanguloArredondado(102, 116, 24, 12, 2, a3 ? CYAN : DARK);
  textoCentro(114, 122, 1, a3 ? CYAN : WHITE, F("SAIR"));

  retanguloArredondado(3, 132, 123, 12, 2, a4 ? CYAN : DARK);
  textoCentro(64, 138, 1, a4 ? CYAN : WHITE, F("MINIMIZAR"));
  texto(5, 149, 1, GRAY, F("JOY: MOVER  SW: SELECIONAR"));
}

void executarComandoTerminal() {
  if ((strcmp(terminalEntrada, "AJUDA") == 0 || strcmp(terminalEntrada, "HELP") == 0)) {
    strcpy(terminalResposta, "HORA DATA TEMPO LIMPAR");
  } else if ((strcmp(terminalEntrada, "HORA") == 0 || strcmp(terminalEntrada, "TIME") == 0)) {
    if (rtcLerDataHora(dataHoraAtual)) {
      snprintf_P(terminalResposta, sizeof(terminalResposta),
        PSTR("HORA %02u:%02u:%02u"), dataHoraAtual.hora,
        dataHoraAtual.minuto, dataHoraAtual.segundo);
    } else strcpy(terminalResposta, "MODULO DE RELOGIO NAO DETECTADO");
  } else if ((strcmp(terminalEntrada, "DATA") == 0 || strcmp(terminalEntrada, "DATE") == 0)) {
    if (rtcLerDataHora(dataHoraAtual)) {
      snprintf_P(terminalResposta, sizeof(terminalResposta),
        PSTR("DATA %02u/%02u/20%02u"), dataHoraAtual.dia,
        dataHoraAtual.mes, dataHoraAtual.ano);
    } else strcpy(terminalResposta, "MODULO DE RELOGIO NAO DETECTADO");
  } else if ((strcmp(terminalEntrada, "TEMPO") == 0 || strcmp(terminalEntrada, "UPTIME") == 0)) {
    unsigned long segs = (millis() - inicioSistema) / 1000UL;
    snprintf_P(terminalResposta, sizeof(terminalResposta),
      PSTR("ATIVO %lu SEG"), segs);
  } else if ((strcmp(terminalEntrada, "LIMPAR") == 0 || strcmp(terminalEntrada, "CLEAR") == 0)) {
    strcpy(terminalResposta, "TELA LIMPA");
  } else if (terminalTamanho == 0) {
    strcpy(terminalResposta, "DIGITE AJUDA");
  } else {
    strcpy(terminalResposta, "COMANDO DESCONHECIDO");
  }
  terminalEntrada[0] = '\0';
  terminalTamanho = 0;
}

void selecionarTeclaTerminal() {
  if (terminalTecladoMinimizado) {
    terminalTecladoMinimizado = false;
    tecladoLinha = 0;
    tecladoColuna = 0;
    mostrarTerminal();
    return;
  }

  if (tecladoLinha == TECLADO_LINHA_ACOES) {
    if (tecladoColuna == 0) {
      if (terminalTamanho > 0) terminalEntrada[--terminalTamanho] = '\0';
    } else if (tecladoColuna == 1) {
      if (terminalTamanho < 16) {
        terminalEntrada[terminalTamanho++] = ' ';
        terminalEntrada[terminalTamanho] = '\0';
      }
    } else if (tecladoColuna == 2) {
      executarComandoTerminal();
    } else if (tecladoColuna == 3) {
      mostrarMenu();
      return;
    } else if (tecladoColuna == 4) {
      terminalTecladoMinimizado = true;
    }
  } else {
    char tecla = lerTeclaQwerty(tecladoLinha, tecladoColuna);
    if (tecla && terminalTamanho < 16) {
      terminalEntrada[terminalTamanho++] = tecla;
      terminalEntrada[terminalTamanho] = '\0';
      somClique();
    }
  }
  mostrarTerminal();
}

void mostrarMonitor() {
  tela = MONITOR;
  limpar();

  texto(5, 5, 3, CYAN, F("MONITOR"));
  linha(5, 23, 123, 23, GRAY);

  desenharBarraArmazenamento(true);

  textoIcone(9, 57, tamanhoIcone, WHITE, F("📊"));
  texto(9, 76, 1, GRAY, F("TEMPO ATIVO:"));

  atualizarMonitor();
  texto(5, 151, 1, CYAN, F("SW: VOLTAR"));
}


void atualizarMonitor() {
  desenharBarraArmazenamento(true);

  unsigned long segundos = (millis() - inicioSistema) / 1000UL;

  unsigned int horas = segundos / 3600UL;
  byte minutos = (segundos / 60UL) % 60;
  byte seg = segundos % 60;

  char horario[12];

  snprintf_P(
    horario, sizeof(horario),
    PSTR("%02u:%02u:%02u"),
    horas, minutos, seg
  );

  texto(9, 98, 3, CYAN, horario);
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
  EEPROM.update(base, 0); // marca como indisponivel durante a gravacao

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
      textoIcone(8, y - 1, 2, WHITE, F("📄"));
      texto(29, y, 1, item == selecionadoArquivo ? CYAN : WHITE, nome);
    }
    else if (temEspacoArquivo && item == qtdArquivos) {
      textoIcone(8, y - 1, 2, WHITE, F("➕"));
      texto(29, y, 1, item == selecionadoArquivo ? CYAN : WHITE,
            F("NOVO ARQUIVO"));
    }
    else {
      textoIcone(8, y - 1, 2, WHITE, F("↩"));
      texto(29, y, 1, item == selecionadoArquivo ? CYAN : WHITE,
            F("VOLTAR"));
    }
  }

  char memoria[20];
  snprintf_P(memoria, sizeof(memoria), PSTR("%u/5 ARQUIVOS"), qtdArquivos);
  texto(5, 137, 1, GRAY, memoria);
  texto(5, 147, 1, CYAN, F("SW: ABRIR / SELECIONAR"));
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
  for (byte l = 0; l < 5; l++) {
    uint16_t pos = inicio + (uint16_t)l * 18;
    if (pos >= tamanhoConteudo) break;
    char trecho[19];
    byte n = 0;
    while (n < 18 && pos + n < tamanhoConteudo) {
      trecho[n] = conteudoArquivo[pos + n];
      n++;
    }
    trecho[n] = '\0';
    texto(5, 31 + l * 18, 1, WHITE, trecho);
  }

  byte totalPaginas = (tamanhoConteudo + 89) / 90;
  if (totalPaginas == 0) totalPaginas = 1;
  char paginaTexto[16];
  snprintf_P(paginaTexto, sizeof(paginaTexto), PSTR("PAG %u/%u"), paginaArquivo + 1, totalPaginas);
  texto(5, 124, 1, GRAY, paginaTexto);

  if (arquivoSlotAtual == 1) {
    texto(5, 143, 1, CYAN, F("SW: VOLTAR"));
  } else {
    const __FlashStringHelper *acao = F("VOLTAR");
    if (acaoArquivo == 0) acao = F("EDITAR");
    else if (acaoArquivo == 1) acao = F("RENOMEAR");
    else if (acaoArquivo == 2) acao = F("APAGAR");
    texto(5, 141, 1, CYAN, acao);
    texto(5, 151, 1, GRAY, F("ESQ/DIR: ACAO  SW: OK"));
  }
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
    byte total = quantidadeTeclasQwerty(row);
    int xInicio = inicioLinhaQwerty(row);
    int y = 59 + row * 13;

    for (byte col = 0; col < total; col++) {
      int x = xInicio + col * 12;
      bool ativo = (tecladoLinha == row && tecladoColuna == col);

      retanguloArredondado(
        x, y, 10, 11, 2,
        ativo ? CYAN : DARK
      );

      char etiqueta[2];
      etiqueta[0] = lerTeclaQwerty(row, col);
      etiqueta[1] = '\0';

      textoCentro(
        x + 5, y + 6, 1,
        ativo ? CYAN : WHITE,
        etiqueta
      );
    }
  }

  bool delAtivo = (tecladoLinha == TECLADO_LINHA_ACOES && tecladoColuna == ACAO_DEL);
  bool espacoAtivo = (tecladoLinha == TECLADO_LINHA_ACOES && tecladoColuna == ACAO_ESPACO);
  bool okAtivo = (tecladoLinha == TECLADO_LINHA_ACOES && tecladoColuna == ACAO_OK);

  retanguloArredondado(5, 113, 30, 12, 2, delAtivo ? CYAN : DARK);
  textoCentro(20, 119, 1, delAtivo ? CYAN : WHITE, "DEL");

  retanguloArredondado(40, 113, 48, 12, 2, espacoAtivo ? CYAN : DARK);
  textoCentro(64, 119, 1, espacoAtivo ? CYAN : WHITE, F("ESPACO"));

  retanguloArredondado(93, 113, 30, 12, 2, okAtivo ? CYAN : DARK);
  textoCentro(108, 119, 1, okAtivo ? CYAN : WHITE, "OK");

  texto(4, 132, 1, GRAY, F("JOY: MOVER  SW: USAR"));
  texto(4, 144, 1, CYAN, F("OK SALVA  DEL APAGA"));
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

  if (editandoNomeArquivo) {
    if (tamanhoNomeTemporario < 8) {
      nomeTemporario[tamanhoNomeTemporario++] = tecla;
      nomeTemporario[tamanhoNomeTemporario] = '\0';
    }
  } else if (tamanhoConteudo < FILE_MAX_TEXT) {
    conteudoArquivo[tamanhoConteudo++] = tecla;
    conteudoArquivo[tamanhoConteudo] = '\0';
  }

  mostrarEditorArquivo();
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

void desenharDino() {
  limpar();

  texto(5, 5, 2, CYAN, F("DINO_RUN"));

  char score[15];
  snprintf_P(score, sizeof(score), PSTR("PONTOS: %d"), pontos);
  texto(5, 24, 1, WHITE, score);

  linha(5, 126, 123, 126, GRAY);

  if (pulando) {
    textoIcone(18, 75, tamanhoIcone, WHITE, F("🦖"));
  } else {
    textoIcone(18, 105, tamanhoIcone, WHITE, F("🦖"));
  }

  if (obstaculoX > 0) {
    textoIcone(obstaculoX, 105, tamanhoIcone, WHITE, F("🌵"));
  }

  texto(5, 145, 1, CYAN, F("SW: SAIR"));
}

void iniciarDino() {
  tela = DINO;
  pulando = false;
  gameOver = false;

  pontos = 0;
  obstaculoX = 120;

  inicioObstaculo = millis();
  ultimaFrameDino = millis();

  desenharDino();
}

void atualizarDino() {
  if (gameOver) {
    if (botaoPressionado()) {
      mostrarJogos();
    }
    return;
  }

  unsigned long agora = millis();
  int y = analogRead(JOY_Y);

  if (y < centroY - LIMITE_JOYSTICK && !pulando) {
    pulando = true;
    inicioPulo = agora;
    somPulo();
  }

  if (pulando && agora - inicioPulo > 650) {
    pulando = false;
  }

  if (agora - ultimaFrameDino >= 120) {
    ultimaFrameDino = agora;

    if (obstaculoX > -15) {
      obstaculoX -= 3;
    } else {
      obstaculoX = 120;
      pontos++;
    }

    if (obstaculoX <= 28 && obstaculoX >= 12 && !pulando) {
      gameOver = true;
      somGameOver();

      limpar();
      texto(20, 48, 3, CYAN, F("FIM DE JOGO"));

      char score[15];
      snprintf_P(score, sizeof(score), PSTR("PONTOS: %d"), pontos);
      texto(37, 83, 2, WHITE, score);
      texto(12, 130, 1, GRAY, F("SW: VOLTAR"));
      return;
    }

    desenharDino();
  }

  if (digitalRead(JOY_SW) == LOW) {
    delay(30);
    if (digitalRead(JOY_SW) == LOW) {
      mostrarJogos();
      while (digitalRead(JOY_SW) == LOW) delay(5);
    }
  }
}


void selecionarConfig() {
  switch (configSelecionada) {
    case 0: mostrarAjusteFonte(); break;
    case 1: mostrarAjusteIcones(); break;
    case 2:
      rtcDisponivel = rtcLerDataHora(dataHoraEdicao);
      if (!rtcDisponivel) dataHoraEdicao = dataHoraAtual;
      entradaDataHora[0] = '\0';
      tamanhoEntradaDataHora = 0;
      tecladoLinha = 0;
      tecladoColuna = 0;
      mostrarAjusteData();
      break;
    case 3:
      rtcDisponivel = rtcLerDataHora(dataHoraEdicao);
      if (!rtcDisponivel) dataHoraEdicao = dataHoraAtual;
      entradaDataHora[0] = '\0';
      tamanhoEntradaDataHora = 0;
      tecladoLinha = 0;
      tecladoColuna = 0;
      mostrarAjusteHora();
      break;
    case 4: desligarBOX(); break;
    case 5: reiniciarBOX(); break;
    default: mostrarMenu(); break;
  }
}

bool dataValida(byte dia, byte mes, byte ano) {
  if (mes < 1 || mes > 12 || dia < 1) return false;
  byte diasMes[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  bool bissexto = ((ano % 4) == 0);
  if (bissexto) diasMes[1] = 29;
  return dia <= diasMes[mes - 1];
}

void confirmarDataHora() {
  byte esperado = (tela == AJUSTAR_DATA) ? 8 : 6;
  if (tamanhoEntradaDataHora != esperado) {
    somErro();
    texto(5, 43, 1, CYAN, F("QUANTIDADE INCORRETA"));
    delay(650);
    if (tela == AJUSTAR_DATA) mostrarAjusteData();
    else mostrarAjusteHora();
    return;
  }

  if (tela == AJUSTAR_DATA) {
    byte dia = (entradaDataHora[0] - '0') * 10 + (entradaDataHora[1] - '0');
    byte mes = (entradaDataHora[3] - '0') * 10 + (entradaDataHora[4] - '0');
    int anoCompleto = (entradaDataHora[6] - '0') * 1000 +
                      (entradaDataHora[7] - '0') * 100 +
                      (entradaDataHora[8] - '0') * 10 +
                      (entradaDataHora[9] - '0');
    if (anoCompleto < 2000 || anoCompleto > 2099 ||
        !dataValida(dia, mes, (byte)(anoCompleto - 2000))) {
      somErro();
      texto(5, 43, 1, CYAN, F("DATA INVALIDA"));
      delay(650);
      mostrarAjusteData();
      return;
    }
    dataHoraEdicao.dia = dia;
    dataHoraEdicao.mes = mes;
    dataHoraEdicao.ano = anoCompleto - 2000;
  } else {
    byte hora = (entradaDataHora[0] - '0') * 10 + (entradaDataHora[1] - '0');
    byte minuto = (entradaDataHora[2] - '0') * 10 + (entradaDataHora[3] - '0');
    byte segundo = (entradaDataHora[4] - '0') * 10 + (entradaDataHora[5] - '0');
    if (hora > 23 || minuto > 59 || segundo > 59) {
      somErro();
      texto(5, 43, 1, CYAN, F("HORA INVALIDA"));
      delay(650);
      mostrarAjusteHora();
      return;
    }
    dataHoraEdicao.hora = hora;
    dataHoraEdicao.minuto = minuto;
    dataHoraEdicao.segundo = segundo;
  }

  rtcGravarDataHora(dataHoraEdicao);
  rtcDisponivel = rtcLerDataHora(dataHoraAtual);
  if (rtcDisponivel) somSucesso();
  else somErro();
  mostrarConfig();
}

void selecionarTeclaDataHora() {
  if (!rtcDisponivel) {
    mostrarConfig();
    return;
  }

  if (tecladoLinha == TECLADO_LINHA_ACOES) {
    if (tecladoColuna == 0) {
      if (tamanhoEntradaDataHora > 0) {
        if (tela == AJUSTAR_DATA) {
          byte posicao = strlen(entradaDataHora);
          if (posicao > 0 && entradaDataHora[posicao - 1] == '/') posicao--;
          if (posicao > 0) posicao--;
          if (posicao > 0 && entradaDataHora[posicao - 1] == '/') posicao--;
          entradaDataHora[posicao] = '\0';
        } else {
          entradaDataHora[--tamanhoEntradaDataHora] = '\0';
        }
        tamanhoEntradaDataHora--;
        somApagar();
      }
    } else if (tecladoColuna == 1) {
      byte limite = (tela == AJUSTAR_DATA) ? 8 : 6;
      if (tamanhoEntradaDataHora < limite) {
        if (tela == AJUSTAR_DATA &&
            (tamanhoEntradaDataHora == 2 || tamanhoEntradaDataHora == 4)) {
          byte posicao = strlen(entradaDataHora);
          entradaDataHora[posicao++] = '/';
          entradaDataHora[posicao] = '\0';
        }
        byte posicao = strlen(entradaDataHora);
        entradaDataHora[posicao++] = '0';
        entradaDataHora[posicao] = '\0';
        tamanhoEntradaDataHora++;
      }
    } else if (tecladoColuna == 2) {
      confirmarDataHora();
      return;
    } else {
      mostrarConfig();
      return;
    }
  } else if (tecladoLinha < 3 && tecladoColuna < 3) {
    char tecla = '1' + tecladoLinha * 3 + tecladoColuna;
    byte limite = (tela == AJUSTAR_DATA) ? 8 : 6;
    if (tamanhoEntradaDataHora < limite) {
      if (tela == AJUSTAR_DATA &&
          (tamanhoEntradaDataHora == 2 || tamanhoEntradaDataHora == 4)) {
        byte posicao = strlen(entradaDataHora);
        entradaDataHora[posicao++] = '/';
        entradaDataHora[posicao] = '\0';
      }
      byte posicao = strlen(entradaDataHora);
      entradaDataHora[posicao++] = tecla;
      entradaDataHora[posicao] = '\0';
      tamanhoEntradaDataHora++;
    }
  }

  if (tela == AJUSTAR_DATA) mostrarAjusteData();
  else mostrarAjusteHora();
}


void atualizarNavegacao() {
  if (millis() - ultimoMovimento < 220) return;

  if (tela == AJUSTAR_DATA || tela == AJUSTAR_HORA) {
    int dx = lerDirecaoX();
    int dy = lerDirecaoY();
    if (dx == 0 && dy == 0) return;

    if (tecladoLinha == TECLADO_LINHA_ACOES) {
      if (dx != 0) {
        int novaColuna = (int)tecladoColuna + dx;
        if (novaColuna < 0) novaColuna = 0;
        if (novaColuna > 3) novaColuna = 3;
        tecladoColuna = novaColuna;
      }
      if (dy < 0) {
        const int centrosAcoes[4] = {15, 49, 85, 114};
        int centro = centrosAcoes[tecladoColuna];
        int melhor = 0;
        int menor = 32767;
        const int centrosNumeros[3] = {32, 67, 102};
        for (byte i = 0; i < 3; i++) {
          int distancia = abs(centrosNumeros[i] - centro);
          if (distancia < menor) { menor = distancia; melhor = i; }
        }
        tecladoLinha = 2;
        tecladoColuna = melhor;
      }
    } else {
      if (dx != 0) {
        int novaColuna = (int)tecladoColuna + dx;
        if (novaColuna < 0) novaColuna = 0;
        if (novaColuna > 2) novaColuna = 2;
        tecladoColuna = novaColuna;
      }
      if (dy < 0 && tecladoLinha > 0) {
        tecladoLinha--;
      } else if (dy > 0 && tecladoLinha < 2) {
        tecladoLinha++;
      } else if (dy > 0 && tecladoLinha == 2) {
        const int centrosAcoes[4] = {15, 49, 85, 114};
        const int centrosNumeros[3] = {32, 67, 102};
        int centro = centrosNumeros[tecladoColuna];
        int melhor = 0;
        int menor = 32767;
        for (byte i = 0; i < 4; i++) {
          int distancia = abs(centrosAcoes[i] - centro);
          if (distancia < menor) { menor = distancia; melhor = i; }
        }
        tecladoLinha = TECLADO_LINHA_ACOES;
        tecladoColuna = melhor;
      }
    }

    ultimoMovimento = millis();
    somNavegacao();
    if (tela == AJUSTAR_DATA) mostrarAjusteData();
    else mostrarAjusteHora();
    return;
  }

  if (tela == TERM_EDIT) {
    if (terminalTecladoMinimizado) return;

    int dx = lerDirecaoX();
    int dy = lerDirecaoY();
    if (dx == 0 && dy == 0) return;
    if (tecladoLinha == TECLADO_LINHA_ACOES) {
      if (dx != 0) {
        int nc = (int)tecladoColuna + dx;
        if (nc < 0) nc = 0;
        if (nc > 4) nc = 4;
        tecladoColuna = nc;
      }
      if (dy < 0) {
        const int centros[5] = {15, 49, 85, 114, 64};
        int centro = centros[tecladoColuna];
        tecladoLinha = 3;
        tecladoColuna = colunaQwertyMaisProxima(3, centro);
      }
    } else {
      if (dx != 0) {
        int nc = (int)tecladoColuna + dx;
        int total = quantidadeTeclasQwerty(tecladoLinha);
        if (nc < 0) nc = 0;
        if (nc >= total) nc = total - 1;
        tecladoColuna = nc;
      }
      if (dy < 0 && tecladoLinha > 0) {
        int centro = centroTeclaQwerty(tecladoLinha, tecladoColuna);
        tecladoLinha--;
        tecladoColuna = colunaQwertyMaisProxima(tecladoLinha, centro);
      } else if (dy > 0 && tecladoLinha < 3) {
        int centro = centroTeclaQwerty(tecladoLinha, tecladoColuna);
        tecladoLinha++;
        tecladoColuna = colunaQwertyMaisProxima(tecladoLinha, centro);
      } else if (dy > 0 && tecladoLinha == 3) {
        int centro = centroTeclaQwerty(tecladoLinha, tecladoColuna);
        const int centros[5] = {15, 49, 85, 114, 64};
        byte melhor = 0;
        int menor = 32767;
        for (byte i = 0; i < 5; i++) {
          int d = abs(centros[i] - centro);
          if (d < menor) { menor = d; melhor = i; }
        }
        tecladoLinha = TECLADO_LINHA_ACOES;
        tecladoColuna = melhor;
      }
    }
    ultimoMovimento = millis();
    mostrarTerminal();
    return;
  }

  if (tela == FILE_EDIT) {
    int dx = lerDirecaoX();
    int dy = lerDirecaoY();
    if (dx == 0 && dy == 0) return;

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
    mostrarEditorArquivo();
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

  if (tela == JOGOS) {
    int direcaoJogo = lerDirecao();
    if (direcaoJogo == 0) return;
    ultimoMovimento = millis();
    somNavegacao();
    iniciarDino();
    return;
  }

  int direcao;
  if (tela == AJUSTAR_FONTE || tela == AJUSTAR_ICONES) {
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
  else if (tela == AJUSTAR_ICONES) {
    if (direcao < 0 && tamanhoIcone > 1) tamanhoIcone--;
    if (direcao > 0 && tamanhoIcone < 4) tamanhoIcone++;
    salvarConfiguracoes();
    mostrarAjusteIcones();
  }
}

void atualizarBotao() {
  if (!botaoPressionado()) return;

  switch (tela) {
    case MENU:
      abrirAplicativo();
      break;
    case JOGOS:
      iniciarDino();
      break;
    case TERM_EDIT:
      selecionarTeclaTerminal();
      break;
    case MONITOR:
      mostrarMenu();
      break;
    case CONFIG:
      selecionarConfig();
      break;
    case AJUSTAR_FONTE:
    case AJUSTAR_ICONES:
      mostrarConfig();
      break;
    case AJUSTAR_DATA:
    case AJUSTAR_HORA:
      selecionarTeclaDataHora();
      break;
    case DINO:
      mostrarJogos();
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
  displayTFT.setRotation(0);
  displayTFT.fillScreen(ST77XX_BLACK);
  calibrarJoystick();
  inicializarBoxFiles();
  carregarConfiguracoes();
  iniciarRTC();
  inicioSistema = millis();
  iniciado = true;
  somInicializacao();
  animacaoInicio();
  mostrarMenu();
}


void loop() {
  if (!iniciado) return;
  if (tela == DINO) {
    atualizarDino();
    return;
  }
  if (tela == MONITOR && millis() - ultimaAtualizacao >= 1000) {
    ultimaAtualizacao = millis();
    atualizarMonitor();
  }
  if (tela == MENU && millis() - ultimaAtualizacaoRelogio >= 100) {
    ultimaAtualizacaoRelogio = millis();
    DataHoraRTC horarioAtual;
    bool horarioDisponivel = rtcLerDataHora(horarioAtual);
    bool mudou = !relogioMenuInicializado || (horarioDisponivel != relogioMenuDisponivel);
    if (horarioDisponivel && relogioMenuDisponivel && dataHoraMenuMudou(horarioAtual, dataHoraMenuAnterior)) mudou = true;
    if (mudou) mostrarMenu();
  }
  atualizarNavegacao();
  atualizarBotao();
}

void loop() {
  lerSerial();

  if (!iniciado) return;

  if (tela == DINO) {
    atualizarDino();
    return;
  }

  if (tela == MONITOR) {
    if (millis() - ultimaAtualizacao >= 1000) {
      ultimaAtualizacao = millis();
      atualizarMonitor();
    }
  }

  if (tela == MENU && millis() - ultimaAtualizacaoRelogio >= 100) {
    ultimaAtualizacaoRelogio = millis();

    DataHoraRTC horarioAtual;
    bool horarioDisponivel = rtcLerDataHora(horarioAtual);

    bool mudou = !relogioMenuInicializado ||
                 (horarioDisponivel != relogioMenuDisponivel);

    if (horarioDisponivel && relogioMenuDisponivel &&
        dataHoraMenuMudou(horarioAtual, dataHoraMenuAnterior)) {
      mudou = true;
    }

    if (mudou) {
      mostrarMenu();
    }
  }

  atualizarNavegacao();
  atualizarBotao();
}
