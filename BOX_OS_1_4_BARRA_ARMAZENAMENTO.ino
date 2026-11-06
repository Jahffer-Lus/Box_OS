
#include <avr/wdt.h>
#include <EEPROM.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <avr/pgmspace.h>

// Declaracao antecipada: evita erro dos prototipos automaticos do Arduino.
struct DataHoraRTC;

// =============================================
// BOX OS 1.4 - BOX FILES + BARRA DE ARMAZENAMENTO + QWERTY + BUZZER + RTC + TERMINAL
// Arduino Uno R3 + BOX DISPLAY
// =============================================

#define JOY_X A0
#define JOY_Y A1
#define JOY_SW 2
#define BUZZER_PIN 6  // Buzzer passivo: positivo no D6, negativo no GND

// RTC MH-Real-Time Clock com chip DS1302:
// DAT -> D3, CLK -> D4, RST/CE -> D5
#define RTC_IO 3
#define RTC_SCLK 4
#define RTC_CE 5

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

// Textos fixos ficam na memoria Flash, nao na RAM.
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
    case 0: return F("🎮");
    case 1: return F("💻");
    case 2: return F("📊");
    case 3: return F("⚙️");
    default: return F("📁");
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

// =============================================
// ESTADOS
// =============================================

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

// Configuracoes persistentes na EEPROM (BOX FILES ocupa enderecos 0..1007).
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
bool rtcDisponivel = false;
char entradaDataHora[9] = "";
byte tamanhoEntradaDataHora = 0;

// Terminal: entrada e ultima resposta.
char terminalEntrada[17] = "";
byte terminalTamanho = 0;
char terminalResposta[25] = "DIGITE HELP";

int centroX = 512;
int centroY = 512;

const int LIMITE_JOYSTICK = 130;

unsigned long ultimoMovimento = 0;
unsigned long ultimoBotao = 0;
unsigned long ultimaAtualizacao = 0;

bool iniciado = false;
bool ultimoEstadoBotao = HIGH;

unsigned long inicioSistema = 0;

// =============================================
// BOX FILES 1.0 - EEPROM INTERNA
// 5 arquivos de 200 bytes = 1000 bytes.
// Cada arquivo guarda nome, tamanho e ate 185 bytes de texto.
// Os dois primeiros arquivos sao protegidos.
// =============================================
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

// Prototipos para deixar a ordem das funcoes explicita.
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

// Teclado QWERTY compacto: 4 linhas de teclas e uma linha de acoes.
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

// =============================================
// DESENHO SERIAL
// =============================================

void limpar() {
  Serial.println(F("CLEAR"));
}

// =============================================
// RTC DS1302 - COMUNICACAO DE 3 FIOS
// =============================================

byte decParaBCD(byte valor) {
  return ((valor / 10) << 4) | (valor % 10);
}

byte bcdParaDec(byte valor) {
  return ((valor >> 4) * 10) + (valor & 0x0F);
}

void rtcEscreverRegistro(byte endereco, byte valor) {
  digitalWrite(RTC_CE, LOW);
  digitalWrite(RTC_SCLK, LOW);
  pinMode(RTC_IO, OUTPUT);
  digitalWrite(RTC_CE, HIGH);
  shiftOut(RTC_IO, RTC_SCLK, LSBFIRST, endereco);
  shiftOut(RTC_IO, RTC_SCLK, LSBFIRST, valor);
  digitalWrite(RTC_CE, LOW);
}

byte rtcLerRegistro(byte endereco) {
  byte valor = 0;
  digitalWrite(RTC_CE, LOW);
  digitalWrite(RTC_SCLK, LOW);
  pinMode(RTC_IO, OUTPUT);
  digitalWrite(RTC_CE, HIGH);
  shiftOut(RTC_IO, RTC_SCLK, LSBFIRST, endereco | 0x01);
  pinMode(RTC_IO, INPUT);
  valor = shiftIn(RTC_IO, RTC_SCLK, LSBFIRST);
  digitalWrite(RTC_CE, LOW);
  pinMode(RTC_IO, OUTPUT);
  return valor;
}

bool rtcLerDataHora(DataHoraRTC &dh) {
  byte segundosRaw = rtcLerRegistro(0x80);
  dh.segundo = bcdParaDec(segundosRaw & 0x7F);
  dh.minuto = bcdParaDec(rtcLerRegistro(0x82) & 0x7F);

  byte horaRaw = rtcLerRegistro(0x84);
  if (horaRaw & 0x80) { // modo 12 horas
    bool pm = horaRaw & 0x20;
    dh.hora = bcdParaDec(horaRaw & 0x1F);
    if (dh.hora == 12) dh.hora = 0;
    if (pm) dh.hora += 12;
  } else {
    dh.hora = bcdParaDec(horaRaw & 0x3F);
  }

  dh.dia = bcdParaDec(rtcLerRegistro(0x86) & 0x3F);
  dh.mes = bcdParaDec(rtcLerRegistro(0x88) & 0x1F);
  dh.diaSemana = bcdParaDec(rtcLerRegistro(0x8A) & 0x07);
  dh.ano = bcdParaDec(rtcLerRegistro(0x8C));

  return dh.segundo < 60 && dh.minuto < 60 && dh.hora < 24 &&
         dh.dia >= 1 && dh.dia <= 31 && dh.mes >= 1 && dh.mes <= 12 &&
         dh.ano <= 99;
}

void rtcGravarDataHora(const DataHoraRTC &dh) {
  rtcEscreverRegistro(0x8E, 0x00); // desativa protecao de escrita
  rtcEscreverRegistro(0x80, decParaBCD(dh.segundo) & 0x7F);
  rtcEscreverRegistro(0x82, decParaBCD(dh.minuto));
  rtcEscreverRegistro(0x84, decParaBCD(dh.hora)); // formato 24 horas
  rtcEscreverRegistro(0x86, decParaBCD(dh.dia));
  rtcEscreverRegistro(0x88, decParaBCD(dh.mes));
  rtcEscreverRegistro(0x8A, decParaBCD(dh.diaSemana ? dh.diaSemana : 1));
  rtcEscreverRegistro(0x8C, decParaBCD(dh.ano));
  rtcEscreverRegistro(0x8E, 0x80); // ativa protecao novamente
}

void iniciarRTC() {
  pinMode(RTC_CE, OUTPUT);
  pinMode(RTC_SCLK, OUTPUT);
  pinMode(RTC_IO, OUTPUT);
  digitalWrite(RTC_CE, LOW);
  digitalWrite(RTC_SCLK, LOW);

  // Considera o RTC disponivel somente se a leitura retornar uma data valida.
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

// =============================================
// CONFIGURACOES NA EEPROM DO UNO
// BOX FILES usa 8 + 5*200 = 1008 bytes.
// Guardamos as preferencias nos 16 bytes restantes.
// =============================================

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

// =============================================
// SONS DO BOX OS - BUZZER PASSIVO NO D6
// tone() gera frequencias; a duracao nao bloqueia o loop.
// =============================================

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

// Converte o nível de fonte (1 a 4) em um tamanho proporcional.
// O nível 2 mantém aproximadamente o tamanho normal do BOX OS.
byte ajustarTamanhoFonte(byte tamanhoBase) {
  int resultado = (int)tamanhoBase + (int)tamanhoFonte - 2;
  if (resultado < 1) resultado = 1;
  if (resultado > 6) resultado = 6;
  return (byte)resultado;
}

void enviarTextoSerial(
  int x, int y, int tamanho,
  const __FlashStringHelper *cor,
  const __FlashStringHelper *mensagem
) {
  Serial.print(F("TEXT|"));
  Serial.print(x); Serial.print('|');
  Serial.print(y); Serial.print('|');
  Serial.print(tamanho); Serial.print('|');
  Serial.print(cor); Serial.print('|');
  Serial.println(mensagem);
}

void enviarTextoSerial(
  int x, int y, int tamanho,
  const __FlashStringHelper *cor, const char *mensagem
) {
  Serial.print(F("TEXT|"));
  Serial.print(x); Serial.print('|');
  Serial.print(y); Serial.print('|');
  Serial.print(tamanho); Serial.print('|');
  Serial.print(cor); Serial.print('|');
  Serial.println(mensagem);
}

// Textos fixos usam F() e ficam na Flash. Textos variaveis usam RAM.
void texto(int x, int y, int tamanho,
           const __FlashStringHelper *cor,
           const __FlashStringHelper *mensagem) {
  enviarTextoSerial(x, y, ajustarTamanhoFonte(tamanho), cor, mensagem);
}

void texto(int x, int y, int tamanho,
           const __FlashStringHelper *cor, const char *mensagem) {
  enviarTextoSerial(x, y, ajustarTamanhoFonte(tamanho), cor, mensagem);
}

void textoIcone(int x, int y, int tamanho,
                const __FlashStringHelper *cor,
                const __FlashStringHelper *mensagem) {
  int ajustado = (int)tamanho + (int)tamanhoIcone - 2;
  if (ajustado < 1) ajustado = 1;
  if (ajustado > 6) ajustado = 6;
  enviarTextoSerial(x, y, ajustado, cor, mensagem);
}

void linha(
  int x1, int y1, int x2, int y2,
  const __FlashStringHelper *cor
) {
  Serial.print(F("LINE|"));
  Serial.print(x1);
  Serial.print('|');
  Serial.print(y1);
  Serial.print('|');
  Serial.print(x2);
  Serial.print('|');
  Serial.print(y2);
  Serial.print('|');
  Serial.println(cor);
}

void circulo(
  int x, int y, int raio,
  const __FlashStringHelper *cor
) {
  Serial.print(F("CIRCLE|"));
  Serial.print(x);
  Serial.print('|');
  Serial.print(y);
  Serial.print('|');
  Serial.print(raio);
  Serial.print('|');
  Serial.println(cor);
}

void retangulo(int x, int y, int largura, int altura, const __FlashStringHelper *cor) {
  Serial.print(F("RECT|"));
  Serial.print(x);
  Serial.print('|');
  Serial.print(y);
  Serial.print('|');
  Serial.print(largura);
  Serial.print('|');
  Serial.print(altura);
  Serial.print('|');
  Serial.println(cor);
}

// Retangulo com cantos arredondados, desenhado pelo BOX DISPLAY.
void retanguloArredondado(
  int x, int y, int largura, int altura, int raio,
  const __FlashStringHelper *cor
) {
  Serial.print(F("RRECT|"));
  Serial.print(x);
  Serial.print('|');
  Serial.print(y);
  Serial.print('|');
  Serial.print(largura);
  Serial.print('|');
  Serial.print(altura);
  Serial.print('|');
  Serial.print(raio);
  Serial.print('|');
  Serial.println(cor);
}

// Envia o emoji centralizado no quadrado arredondado.
void textoIconeCentro(
  int x, int y, int tamanho,
  const __FlashStringHelper *cor,
  const __FlashStringHelper *mensagem
) {
  int ajustado = (int)tamanho + (int)tamanhoIcone - 2;
  if (ajustado < 1) ajustado = 1;
  if (ajustado > 6) ajustado = 6;

  Serial.print(F("TEXTC|"));
  Serial.print(x);
  Serial.print('|');
  Serial.print(y);
  Serial.print('|');
  Serial.print(ajustado);
  Serial.print('|');
  Serial.print(cor);
  Serial.print('|');
  Serial.println(mensagem);
}

void textoCentro(
  int x, int y, int tamanho,
  const __FlashStringHelper *cor,
  const char *mensagem
) {
  Serial.print(F("TEXTC|"));
  Serial.print(x);
  Serial.print('|');
  Serial.print(y);
  Serial.print('|');
  Serial.print(tamanho);
  Serial.print('|');
  Serial.print(cor);
  Serial.print('|');
  Serial.println(mensagem);
}

void textoCentro(
  int x, int y, int tamanho,
  const __FlashStringHelper *cor,
  const __FlashStringHelper *mensagem
) {
  Serial.print(F("TEXTC|"));
  Serial.print(x);
  Serial.print('|');
  Serial.print(y);
  Serial.print('|');
  Serial.print(tamanho);
  Serial.print('|');
  Serial.print(cor);
  Serial.print('|');
  Serial.println(mensagem);
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

// =============================================
// CALIBRACAO
// =============================================

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

// Retorna -1, 0 ou 1
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

// =============================================
// ANIMACAO DE INICIO: CUBO
// =============================================

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

// =============================================
// MENU PRINCIPAL
// =============================================

void mostrarMenu() {
  tela = MENU;
  limpar();

  texto(5, 5, 3, CYAN, F("APLICATIVOS"));
  linha(5, 23, 123, 23, GRAY);

  for (byte i = 0; i < TOTAL_APPS; i++) {
    int y = 31 + i * 21;

    if (i == selecionado) {
      linha(5, y - 4, 123, y - 4, CYAN);
      linha(5, y + 20, 123, y + 20, CYAN);
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

// =============================================
// CONFIGURACOES
// =============================================

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
  // Teclado numerico em grade 3x3: 1 a 9.
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

  // Acoes inferiores: DEL, 0, ENTER e SAIR.
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
    texto(5, 55, 1, WHITE, F("RTC NAO DETECTADO"));
    texto(5, 75, 1, GRAY, F("VERIFIQUE DAT/CLK/RST"));
    texto(5, 145, 1, CYAN, F("SW: VOLTAR"));
    return;
  }

  texto(5, 27, 1, GRAY, F("DDMMAAAA:"));
  texto(62, 27, 1, WHITE, entradaDataHora);
  texto(5, 43, 1, GRAY, F("DIGITE 8 NUMEROS"));
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
    texto(5, 55, 1, WHITE, F("RTC NAO DETECTADO"));
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

// =============================================
// DESLIGAR
// =============================================

void desligarBOX() {
  limpar();
  texto(30, 65, 3, CYAN, F("BOX OS"));
  texto(31, 91, 2, WHITE, F("DESLIGADO"));

  // O RESET fisico liga novamente
  while (true) {
    delay(100);
  }
}

// =============================================
// REINICIAR
// =============================================

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

// =============================================
// ABRIR APLICATIVOS
// =============================================

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
      strcpy(terminalResposta, "DIGITE HELP");
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
  retanguloArredondado(3, 116, 25, 12, 2, a0 ? CYAN : DARK);
  textoCentro(15, 122, 1, a0 ? CYAN : WHITE, F("DEL"));
  retanguloArredondado(30, 116, 38, 12, 2, a1 ? CYAN : DARK);
  textoCentro(49, 122, 1, a1 ? CYAN : WHITE, F("ESP"));
  retanguloArredondado(70, 116, 30, 12, 2, a2 ? CYAN : DARK);
  textoCentro(85, 122, 1, a2 ? CYAN : WHITE, F("ENTER"));
  retanguloArredondado(102, 116, 24, 12, 2, a3 ? CYAN : DARK);
  textoCentro(114, 122, 1, a3 ? CYAN : WHITE, F("SAIR"));
  texto(5, 137, 1, GRAY, F("JOY: MOVER  SW: USAR"));
  texto(5, 149, 1, CYAN, F("HELP TIME DATE UPTIME"));
}

void executarComandoTerminal() {
  if (strcmp(terminalEntrada, "HELP") == 0) {
    strcpy(terminalResposta, "TIME DATE UPTIME CLEAR");
  } else if (strcmp(terminalEntrada, "TIME") == 0) {
    if (rtcLerDataHora(dataHoraAtual)) {
      snprintf_P(terminalResposta, sizeof(terminalResposta),
        PSTR("HORA %02u:%02u:%02u"), dataHoraAtual.hora,
        dataHoraAtual.minuto, dataHoraAtual.segundo);
    } else strcpy(terminalResposta, "RTC NAO DETECTADO");
  } else if (strcmp(terminalEntrada, "DATE") == 0) {
    if (rtcLerDataHora(dataHoraAtual)) {
      snprintf_P(terminalResposta, sizeof(terminalResposta),
        PSTR("DATA %02u/%02u/20%02u"), dataHoraAtual.dia,
        dataHoraAtual.mes, dataHoraAtual.ano);
    } else strcpy(terminalResposta, "RTC NAO DETECTADO");
  } else if (strcmp(terminalEntrada, "UPTIME") == 0) {
    unsigned long segs = (millis() - inicioSistema) / 1000UL;
    snprintf_P(terminalResposta, sizeof(terminalResposta),
      PSTR("ATIVO %lu SEG"), segs);
  } else if (strcmp(terminalEntrada, "CLEAR") == 0) {
    strcpy(terminalResposta, "TELA LIMPA");
  } else if (terminalTamanho == 0) {
    strcpy(terminalResposta, "DIGITE HELP");
  } else {
    strcpy(terminalResposta, "COMANDO DESCONHECIDO");
  }
  terminalEntrada[0] = '\0';
  terminalTamanho = 0;
}

void selecionarTeclaTerminal() {
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
    } else {
      mostrarMenu();
      return;
    }
  } else {
    char tecla = lerTeclaQwerty(tecladoLinha, tecladoColuna);
    if (tecla && terminalTamanho < 16) {
      terminalEntrada[terminalTamanho++] = tecla;
      terminalEntrada[terminalTamanho] = '\0';
    }
  }
  somClique();
  mostrarTerminal();
}

void mostrarMonitor() {
  tela = MONITOR;
  limpar();

  texto(5, 5, 3, CYAN, F("MONITOR"));
  linha(5, 23, 123, 23, GRAY);

  textoIcone(9, 47, tamanhoIcone, WHITE, F("📊"));
  texto(9, 75, 1, GRAY, F("TEMPO ATIVO:"));

  atualizarMonitor();
  texto(5, 151, 1, CYAN, F("SW: VOLTAR"));
}

// =============================================
// MONITOR DE ATIVIDADE
// =============================================

void atualizarMonitor() {
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

  // Apaga a area do horario
  texto(9, 98, 3, CYAN, horario);
}

// =============================================
// BOX FILES 1.0 - FUNCOES DA EEPROM
// =============================================

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
  if (slot < 2 || slot >= FILE_TOTAL_SLOTS) return;
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
  bool formatoValido =
    EEPROM.read(0) == FILE_MAGIC_0 &&
    EEPROM.read(1) == FILE_MAGIC_1 &&
    EEPROM.read(2) == FILE_MAGIC_2 &&
    EEPROM.read(3) == 1;

  if (formatoValido) return;

  // Primeira inicializacao: cria os dois documentos protegidos.
  EEPROM.update(0, FILE_MAGIC_0);
  EEPROM.update(1, FILE_MAGIC_1);
  EEPROM.update(2, FILE_MAGIC_2);
  EEPROM.update(3, 1);
  for (byte i = 4; i < FILE_HEADER_SIZE; i++) EEPROM.update(i, 0);

  for (byte slot = 0; slot < FILE_TOTAL_SLOTS; slot++) {
    EEPROM.update(enderecoSlot(slot), 0);
  }

  static const char LEIA_ME[] PROGMEM =
    "JOGOS: DINO_RUN. Joystick pula e SW sai. TERMINAL: em construcao. "
    "MONITOR: tempo ligado. CONFIG: ajusta fonte e icones. "
    "BOX FILES: cria, le, edita e apaga textos.";

  static const char CREDITOS[] PROGMEM =
    "BOX OS. Criado por Jahffer. Assistencia de programacao: "
    "ChatGPT (OpenAI). Obrigado por usar o BOX OS!";

  escreverArquivoFlash(0, PSTR("LEIA-ME.TXT"), LEIA_ME, sizeof(LEIA_ME) - 1);
  escreverArquivoFlash(1, PSTR("CREDITOS.TXT"), CREDITOS, sizeof(CREDITOS) - 1);
}

void atualizarListaArquivos() {
  qtdArquivos = 0;
  temEspacoArquivo = false;

  for (byte slot = 0; slot < FILE_TOTAL_SLOTS; slot++) {
    if (arquivoExiste(slot)) {
      indicesArquivos[qtdArquivos++] = slot;
    } else if (slot >= 2) {
      temEspacoArquivo = true;
    }
  }
}


void desenharBarraArmazenamento() {
  // O BOX FILES usa cinco slots fixos de 200 bytes na EEPROM.
  // Um slot ativo conta como 20% da capacidade reservada.
  byte usados = 0;
  for (byte slot = 0; slot < FILE_TOTAL_SLOTS; slot++) {
    if (arquivoExiste(slot)) usados++;
  }

  byte percentual = (usados * 100) / FILE_TOTAL_SLOTS;
  const __FlashStringHelper *cor = GREEN;
  if (percentual >= 90) cor = RED;
  else if (percentual >= 70) cor = ORANGE;
  else if (percentual >= 50) cor = YELLOW;

  texto(5, 24, 1, GRAY, F("USADO"));
  char porcentagem[6];
  snprintf_P(porcentagem, sizeof(porcentagem), PSTR("%u%%"), percentual);
  texto(96, 24, 1, cor, porcentagem);

  // Fundo da barra e preenchimento proporcional ao uso real dos slots.
  retangulo(5, 30, 118, 7, DARK);
  int larguraPreenchida = (118L * usados) / FILE_TOTAL_SLOTS;
  if (larguraPreenchida > 0) {
    retangulo(5, 30, larguraPreenchida, 7, cor);
  }
}

void mostrarListaArquivos() {
  tela = FILE_LIST;
  atualizarListaArquivos();
  limpar();

  // Cabecalho fixo: titulo, barra de armazenamento e depois a lista.
  texto(5, 3, 3, CYAN, F("BOX FILES"));
  linha(5, 20, 123, 20, GRAY);
  desenharBarraArmazenamento();

  // Arquivos + opcao NOVO (se houver espaco) + opcao VOLTAR.
  byte totalItens = qtdArquivos + (temEspacoArquivo ? 1 : 0) + 1;
  byte itemVoltar = totalItens - 1;

  if (selecionadoArquivo >= totalItens) {
    selecionadoArquivo = itemVoltar;
  }

  // Cinco linhas compactas abaixo da barra.
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
  for (byte slot = 2; slot < FILE_TOTAL_SLOTS; slot++) {
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
  snprintf_P(nome, sizeof(nome), PSTR("NOVO%02u.TXT"), slotLivre - 1);
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

  // A ultima opcao sempre retorna ao menu principal.
  if (selecionadoArquivo == itemVoltar) {
    mostrarMenu();
    return;
  }

  // A opcao NOVO aparece somente quando existe um espaco livre.
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

  if (arquivoSlotAtual < 2) {
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
  // Centraliza as linhas de 9 e 7 teclas.
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

  // Quatro linhas do teclado QWERTY.
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

  // Linha de acoes: DEL, ESPACO e OK.
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
  // A linha inferior possui tres botoes de acao.
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

    // ACAO_OK: salvar texto ou confirmar nome.
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

  // Tecla alfanumerica selecionada.
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
  if (arquivoSlotAtual < 2) {
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

// =============================================
// DINO RUN
// =============================================

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

  // O obstaculo e a tela atualizam juntos, em intervalos regulares.
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

  // A velocidade nao depende mais da velocidade do loop do Arduino.
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
      texto(20, 48, 3, CYAN, F("GAME OVER"));

      char score[15];
      snprintf_P(score, sizeof(score), PSTR("PONTOS: %d"), pontos);
      texto(37, 83, 2, WHITE, score);
      texto(12, 130, 1, GRAY, F("SW: VOLTAR"));
      return;
    }

    desenharDino();
  }

  // O botao SW sai do jogo e retorna a lista de jogos.
  if (digitalRead(JOY_SW) == LOW) {
    delay(30);
    if (digitalRead(JOY_SW) == LOW) {
      mostrarJogos();
      while (digitalRead(JOY_SW) == LOW) delay(5);
    }
  }
}

// =============================================
// MENU DE CONFIGURACOES
// =============================================

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
    byte mes = (entradaDataHora[2] - '0') * 10 + (entradaDataHora[3] - '0');
    int anoCompleto = (entradaDataHora[4] - '0') * 1000 +
                      (entradaDataHora[5] - '0') * 100 +
                      (entradaDataHora[6] - '0') * 10 +
                      (entradaDataHora[7] - '0');
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
        entradaDataHora[--tamanhoEntradaDataHora] = '\0';
        somApagar();
      }
    } else if (tecladoColuna == 1) {
      byte limite = (tela == AJUSTAR_DATA) ? 8 : 6;
      if (tamanhoEntradaDataHora < limite) {
        entradaDataHora[tamanhoEntradaDataHora++] = '0';
        entradaDataHora[tamanhoEntradaDataHora] = '\0';
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
      entradaDataHora[tamanhoEntradaDataHora++] = tecla;
      entradaDataHora[tamanhoEntradaDataHora] = '\0';
    }
  }

  if (tela == AJUSTAR_DATA) mostrarAjusteData();
  else mostrarAjusteHora();
}

// =============================================
// CONTROLES
// =============================================

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
    int dx = lerDirecaoX();
    int dy = lerDirecaoY();
    if (dx == 0 && dy == 0) return;
    if (tecladoLinha == TECLADO_LINHA_ACOES) {
      if (dx != 0) {
        int nc = (int)tecladoColuna + dx;
        if (nc < 0) nc = 0;
        if (nc > 3) nc = 3;
        tecladoColuna = nc;
      }
      if (dy < 0) {
        const int centros[4] = {15, 49, 85, 114};
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
        const int centros[4] = {15, 49, 85, 114};
        byte melhor = 0;
        int menor = 32767;
        for (byte i = 0; i < 4; i++) {
          int d = abs(centros[i] - centro);
          if (d < menor) { menor = d; melhor = i; }
        }
        tecladoLinha = TECLADO_LINHA_ACOES;
        tecladoColuna = melhor;
      }
    }
    ultimoMovimento = millis();
    somNavegacao();
    if (tela == TERM_EDIT) mostrarTerminal();
    else if (tela == AJUSTAR_DATA) mostrarAjusteData();
    else mostrarAjusteHora();
    ultimoMovimento = millis();
    somNavegacao();
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
    if (arquivoSlotAtual >= 2 && dx != 0) {
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

  // Na tela JOGOS, qualquer movimento do joystick inicia o DINO_RUN.
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

// =============================================
// SERIAL: INICIAR TELA
// =============================================

void lerSerial() {
  static char comando[16];
  static byte posicao = 0;

  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\r') continue;

    if (c == '\n') {
      comando[posicao] = '\0';

      if (strcmp_P(comando, PSTR("START")) == 0 && !iniciado) {
        iniciado = true;
        inicioSistema = millis();

        somInicializacao();
        animacaoInicio();
        mostrarMenu();

        Serial.println(F("BOX_STARTED"));
      }

      if (strcmp_P(comando, PSTR("PING")) == 0) {
        Serial.println(F("BOX_READY"));
      }

      posicao = 0;
    }
    else if (posicao < sizeof(comando) - 1) {
      comando[posicao++] = c;
    }
  }
}

// =============================================
// SETUP
// =============================================

void setup() {
  wdt_disable();

  Serial.begin(115200);

  pinMode(JOY_X, INPUT);
  pinMode(JOY_Y, INPUT);
  pinMode(JOY_SW, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);
  pinMode(RTC_CE, OUTPUT);
  pinMode(RTC_SCLK, OUTPUT);
  pinMode(RTC_IO, OUTPUT);

  calibrarJoystick();
  inicializarBoxFiles();
  carregarConfiguracoes();
  iniciarRTC();

  delay(1000);

  Serial.println(F("BOX_READY"));
}

// =============================================
// LOOP
// =============================================

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

  atualizarNavegacao();
  atualizarBotao();
}
