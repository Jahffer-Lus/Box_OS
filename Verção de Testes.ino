
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
#define BOTAO_EXTRA 13
#define BUZZER_PIN 6

#define TFT_CS 8
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
const byte TOTAL_CONFIG_PAGINAS = 2;
const byte TOTAL_CONFIG_POR_PAGINA[2] = { 3, 4 };

const __FlashStringHelper *nomeApp(byte indice) {
  switch (indice) {
    case 0: return F("BOX FILES");
    default: return F("CONFIG");
  }
}

const __FlashStringHelper *nomeOpcaoConfig(byte pagina, byte indice) {
  if (pagina == 0) {
    switch (indice) {
      case 0: return F("Tamanho da Fonte");
      case 1: return F("Rotacao da Tela");
      default: return F("Velocidade da Selecao");
    }
  }
  switch (indice) {
    case 0: return F("Data e Hora");
    case 1: return F("Desligar");
    case 2: return F("Reiniciar");
    default: return F("Voltar");
  }
}

enum Screen {
  MENU,
  CONFIG,
  AJUSTAR_FONTE,
  AJUSTAR_ROTACAO,
  AJUSTAR_VELOCIDADE,
  AJUSTAR_DATA_HORA,
  FILE_LIST,
  FILE_VIEW,
  FILE_EDIT
};

Screen tela = MENU;

byte selecionado = 0;
byte configSelecionada = 0;
byte configPagina = 0;
byte rtcCampoEdicao = 0;
byte rtcEdicao[6] = { 1, 1, 26, 12, 0, 0 };

byte tamanhoFonte = 1;
byte rotacaoTela = 3;
byte velocidadeSelecao = 1; // 0=lenta, 1=normal, 2=rapida.
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
byte corTextoAtual = 0;
byte tecladoColuna = 0;
byte tecladoLinha = 0;
bool editandoNomeArquivo = false;
bool tecladoSimbolos = false;
char nomeTemporario[9];
byte tamanhoNomeTemporario = 0;

void mostrarListaArquivos();
void atualizarBarraLeitorArquivo() {
  // Atualiza somente a faixa inferior de opcoes, sem limpar/redesenhar o documento.
  displayTFT.fillRect(4, 104, 124, 24, ST77XX_BLACK);
  const __FlashStringHelper *acao = F("VOLTAR");
  if (acaoArquivo == 0) acao = F("EDITAR");
  else if (acaoArquivo == 1) acao = F("RENOMEAR");
  else if (acaoArquivo == 2) acao = F("APAGAR");
  texto(5, 108, 1, CYAN, acao);
}

void mostrarLeitorArquivo();
void mostrarEditorArquivo();
void mostrarConfig();
const char TECLADO_QWERTY[4][11] PROGMEM = {
  "QWERTYUIOP",
  "ASDFGHJKL",
  "ZXCVBNM",
  "1234567890"
};

const char TECLADO_SIMBOLOS[4][11] PROGMEM = {
  "!@#$%^&*()",
  "?,.:;~+-=",
  "[]{}<>/",
  "1234567890"
};

const byte TECLADO_LINHAS = 4;
const byte TECLADO_LINHA_ACOES = 4;
const byte ACAO_DEL = 0;
const byte ACAO_ESPACO = 1;
const byte ACAO_ENTER = 2;
const byte ACAO_RGB = 3;
const byte ACAO_OK = 4;
const byte TOTAL_ACOES_TECLADO = 5;
const byte MARCA_VERMELHO = 1;
const byte MARCA_VERDE = 2;
const byte MARCA_AZUL = 3;
const byte MARCA_BRANCO = 4;


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
  EEPROM.update(CONFIG_EEPROM_BASE + 1, 4);
  EEPROM.update(CONFIG_EEPROM_BASE + 2, tamanhoFonte);
  EEPROM.update(CONFIG_EEPROM_BASE + 3, rotacaoTela);
  EEPROM.update(CONFIG_EEPROM_BASE + 4, velocidadeSelecao);
  EEPROM.update(CONFIG_EEPROM_BASE + 5,
    (byte)(CONFIG_MAGIC ^ tamanhoFonte ^ rotacaoTela ^ velocidadeSelecao ^ 4));
}

void carregarConfiguracoes() {
  byte assinatura = EEPROM.read(CONFIG_EEPROM_BASE);
  byte versao = EEPROM.read(CONFIG_EEPROM_BASE + 1);
  byte fonte = EEPROM.read(CONFIG_EEPROM_BASE + 2);
  byte valorAntigo = EEPROM.read(CONFIG_EEPROM_BASE + 3);
  byte checksum = EEPROM.read(CONFIG_EEPROM_BASE + 4);
  byte velocidade = EEPROM.read(CONFIG_EEPROM_BASE + 4);
  byte checksumV4 = EEPROM.read(CONFIG_EEPROM_BASE + 5);

  if (assinatura == CONFIG_MAGIC && versao == 4 &&
      fonte >= 1 && fonte <= 4 && valorAntigo <= 3 && velocidade <= 2 &&
      checksumV4 == (byte)(CONFIG_MAGIC ^ fonte ^ valorAntigo ^ velocidade ^ 4)) {
    tamanhoFonte = fonte;
    rotacaoTela = valorAntigo;
    velocidadeSelecao = velocidade;
  } else if (assinatura == CONFIG_MAGIC && versao == 3 &&
      fonte >= 1 && fonte <= 4 && valorAntigo <= 3 &&
      checksum == (byte)(CONFIG_MAGIC ^ fonte ^ valorAntigo ^ 3)) {
    tamanhoFonte = fonte;
    rotacaoTela = valorAntigo;
    velocidadeSelecao = 1;
    salvarConfiguracoes();
  } else if (assinatura == CONFIG_MAGIC && versao == 2 &&
             fonte >= 1 && fonte <= 4 && valorAntigo >= 1 &&
             valorAntigo <= 4 &&
             checksum == (byte)(CONFIG_MAGIC ^ fonte ^ valorAntigo)) {
    // Mantem a fonte das versoes anteriores e preserva a orientacao atual.
    tamanhoFonte = fonte;
    rotacaoTela = 3;
    velocidadeSelecao = 1;
    salvarConfiguracoes();
  } else {
    tamanhoFonte = 1;
    rotacaoTela = 3;
    velocidadeSelecao = 1;
    salvarConfiguracoes();
  }
}


// Buzzer ATIVO: nao reproduz frequencias; usa apenas pulsos HIGH/LOW.
void bipAtivo(unsigned int duracao) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(duracao);
  digitalWrite(BUZZER_PIN, LOW);
}

void somClique() {
  bipAtivo(35);
}

void somNavegacao() {
  bipAtivo(22);
}

void somSucesso() {
  bipAtivo(75);
  delay(85);
  bipAtivo(95);
}

void somErro() {
  bipAtivo(180);
  delay(70);
  bipAtivo(180);
}

void somApagar() {
  bipAtivo(65);
  delay(75);
  bipAtivo(100);
}

void somInicializacao() {
  // Sequencia de pulsos: buzzer ativo nao permite variar o tom.
  bipAtivo(65);
  delay(45);
  bipAtivo(65);
  delay(45);
  bipAtivo(65);
  delay(45);
  bipAtivo(150);
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


// ================= RELÓGIO DS1302 =================
// DS1302: CE/RST=D5, DAT/I/O=D3, CLK/SCLK=D4
#define RTC_DAT 3
#define RTC_CLK 4
#define RTC_CE  5

byte rtcUltimoSegundo = 255;
byte rtcUltimoMinuto = 255;
byte rtcUltimaHora = 255;
byte rtcUltimoDia = 255;
byte rtcUltimoMes = 255;
byte rtcUltimoAno = 255;
bool rtcDisponivel = false;

byte rtcBCD(byte valor) {
  return ((valor >> 4) * 10) + (valor & 0x0F);
}

byte rtcParaBCD(byte valor) {
  return ((valor / 10) << 4) | (valor % 10);
}

void rtcIniciarTransacao() {
  digitalWrite(RTC_CE, LOW);
  digitalWrite(RTC_CLK, LOW);
  pinMode(RTC_DAT, OUTPUT);
  digitalWrite(RTC_CE, HIGH);
}

void rtcFinalizarTransacao() {
  digitalWrite(RTC_CE, LOW);
  pinMode(RTC_DAT, INPUT);
}

void rtcEnviarByte(byte valor) {
  for (byte i = 0; i < 8; i++) {
    digitalWrite(RTC_DAT, (valor >> i) & 1);
    digitalWrite(RTC_CLK, HIGH);
    digitalWrite(RTC_CLK, LOW);
  }
}

byte rtcLerByte() {
  byte valor = 0;
  pinMode(RTC_DAT, INPUT);
  for (byte i = 0; i < 8; i++) {
    if (digitalRead(RTC_DAT)) valor |= (1 << i);
    digitalWrite(RTC_CLK, HIGH);
    digitalWrite(RTC_CLK, LOW);
  }
  return valor;
}

byte rtcLerRegistrador(byte endereco) {
  rtcIniciarTransacao();
  rtcEnviarByte(endereco | 0x01);
  byte valor = rtcLerByte();
  rtcFinalizarTransacao();
  return valor;
}

void rtcEscreverRegistrador(byte endereco, byte valor) {
  rtcIniciarTransacao();
  rtcEnviarByte(endereco & 0xFE);
  rtcEnviarByte(valor);
  rtcFinalizarTransacao();
}

void rtcDesproteger() {
  rtcEscreverRegistrador(0x8E, 0x00); // desativa proteção de escrita
}

void rtcDefinirHoraCompilacao() {
  // Usado somente se o oscilador estiver parado (bit CH ligado).
  const char meses[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  byte mes = 1;
  for (byte i = 0; i < 12; i++) {
    if (strncmp(__DATE__ , meses + i * 3, 3) == 0) {
      mes = i + 1;
      break;
    }
  }
  byte dia = (byte)atoi(__DATE__ + 4);
  byte ano = (byte)(atoi(__DATE__ + 7) % 100);
  byte hora = (byte)atoi(__TIME__);
  byte minuto = (byte)atoi(__TIME__ + 3);
  byte segundo = (byte)atoi(__TIME__ + 6);

  rtcDesproteger();
  rtcEscreverRegistrador(0x80, rtcParaBCD(segundo)); // segundos, CH=0
  rtcEscreverRegistrador(0x82, rtcParaBCD(minuto));
  rtcEscreverRegistrador(0x84, rtcParaBCD(hora)); // formato 24 h
  rtcEscreverRegistrador(0x86, rtcParaBCD(dia));
  rtcEscreverRegistrador(0x88, rtcParaBCD(mes));
  rtcEscreverRegistrador(0x8A, rtcParaBCD(1)); // dia da semana
  rtcEscreverRegistrador(0x8C, rtcParaBCD(ano));
  rtcEscreverRegistrador(0x8E, 0x00);
}

bool rtcLerDataHora(byte &seg, byte &minu, byte &hora,
                    byte &dia, byte &mes, byte &ano) {
  byte s = rtcLerRegistrador(0x81);
  if (s & 0x80) return false; // oscilador parado
  seg = rtcBCD(s & 0x7F);
  minu = rtcBCD(rtcLerRegistrador(0x83) & 0x7F);
  byte h = rtcLerRegistrador(0x85);
  if (h & 0x80) { // modo 12 horas: converte para 24 h
    byte h12 = rtcBCD(h & 0x1F);
    bool pm = h & 0x20;
    hora = (h12 % 12) + (pm ? 12 : 0);
  } else {
    hora = rtcBCD(h & 0x3F);
  }
  dia = rtcBCD(rtcLerRegistrador(0x87) & 0x3F);
  mes = rtcBCD(rtcLerRegistrador(0x89) & 0x1F);
  ano = rtcBCD(rtcLerRegistrador(0x8D));
  return (seg < 60 && minu < 60 && hora < 24 &&
          dia >= 1 && dia <= 31 && mes >= 1 && mes <= 12);
}

void rtcDesenharDigito(int x, int y, char digito) {
  displayTFT.fillRect(x, y, 6, 8, ST77XX_BLACK);
  displayTFT.setTextSize(1);
  displayTFT.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  displayTFT.setCursor(x, y);
  displayTFT.write(digito);
}

void rtcDesenharNumero(int x, int y, byte valor, byte quantidade) {
  char buffer[4];
  if (quantidade == 2) snprintf(buffer, sizeof(buffer), "%02u", valor);
  else snprintf(buffer, sizeof(buffer), "%04u", valor);
  for (byte i = 0; i < quantidade; i++) {
    rtcDesenharDigito(x + i * 6, y, buffer[i]);
  }
}

void rtcDesenharSeparadores() {
  // Separadores fixos: nunca são redesenhados durante a contagem.
  displayTFT.setTextSize(1);
  displayTFT.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  displayTFT.setCursor(17, 21); displayTFT.print('/');
  displayTFT.setCursor(35, 21); displayTFT.print('/');
  displayTFT.setCursor(17, 30); displayTFT.print(':');
  displayTFT.setCursor(35, 30); displayTFT.print(':');
}

void atualizarRelogioMenu() {
  if (tela != MENU) return;

  byte seg, minu, hora, dia, mes, ano;
  if (!rtcLerDataHora(seg, minu, hora, dia, mes, ano)) return;

  // Só redesenha os algarismos quando o valor correspondente muda.
  if (dia != rtcUltimoDia) {
    rtcDesenharNumero(5, 21, dia, 2);
    rtcUltimoDia = dia;
  }
  if (mes != rtcUltimoMes) {
    rtcDesenharNumero(23, 21, mes, 2);
    rtcUltimoMes = mes;
  }
  if (ano != rtcUltimoAno) {
    rtcDesenharNumero(41, 21, 2000 + ano, 4);
    rtcUltimoAno = ano;
  }
  if (hora != rtcUltimaHora) {
    rtcDesenharNumero(5, 30, hora, 2);
    rtcUltimaHora = hora;
  }
  if (minu != rtcUltimoMinuto) {
    rtcDesenharNumero(23, 30, minu, 2);
    rtcUltimoMinuto = minu;
  }
  if (seg != rtcUltimoSegundo) {
    rtcDesenharNumero(41, 30, seg, 2);
    rtcUltimoSegundo = seg;
  }
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
  // O botão extra no D13 funciona em paralelo com o SW do joystick.
  // Ambos usam INPUT_PULLUP: pressionado = LOW.
  bool estado = (digitalRead(JOY_SW) == LOW || digitalRead(BOTAO_EXTRA) == LOW);

  if (estado && ultimoEstadoBotao == HIGH &&
      millis() - ultimoBotao > 250) {
    ultimoEstadoBotao = LOW;
    ultimoBotao = millis();
    somClique();
    return true;
  }

  if (!estado) {
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

// Redesenha somente uma opção do menu, sem tocar nos textos fixos.
void desenharItemMenu(byte indice) {
  if (indice >= TOTAL_APPS) return;
  int y = 42 + indice * 18;
  bool ativo = (indice == selecionado);

  // Limpa apenas a área ocupada pelo texto dessa opção.
  displayTFT.fillRect(5, y + 1, 118, 16, ST77XX_BLACK);
  texto(8, y + 1, 2, ativo ? CYAN : WHITE, nomeApp(indice));

  // Atualiza as duas linhas do destaque sem redesenhar o restante da tela.
  uint16_t cor = ativo ? corTFT(CYAN) : ST77XX_BLACK;
  displayTFT.drawFastHLine(5, y - 3, 119, cor);
  displayTFT.drawFastHLine(5, y + 17, 119, cor);
}

void mostrarMenu() {
  tela = MENU;
  limpar();

  texto(5, 3, 3, CYAN, F("APLICATIVOS"));
  rtcUltimoSegundo = rtcUltimoMinuto = rtcUltimaHora = 255;
  rtcUltimoDia = rtcUltimoMes = rtcUltimoAno = 255;
  rtcDesenharSeparadores();
  atualizarRelogioMenu();
  linha(5, 38, 123, 38, GRAY);

  for (byte i = 0; i < TOTAL_APPS; i++) {
    desenharItemMenu(i);
  }

  linha(5, 137, 123, 137, DARK);
  mostrarTela();
}


// Redesenha somente o texto e o destaque de uma opção da configuração.
void desenharItemConfig(byte item) {
  if (item >= TOTAL_CONFIG_POR_PAGINA[configPagina]) return;
  int y = 34 + item * 24;
  bool ativo = (item == configSelecionada);
  displayTFT.fillRect(4, y - 2, 120, 17, ST77XX_BLACK);
  texto(7, y, 1, ativo ? CYAN : WHITE, nomeOpcaoConfig(configPagina, item));
  uint16_t cor = ativo ? corTFT(CYAN) : ST77XX_BLACK;
  displayTFT.drawFastHLine(4, y - 4, 120, cor);
  displayTFT.drawFastHLine(4, y + 15, 120, cor);
}

void mostrarConfig() {
  tela = CONFIG;
  limpar();
  texto(5, 5, 3, CYAN, F("CONFIG"));
  char pagina[12];
  snprintf_P(pagina, sizeof(pagina), PSTR("PAG %u/2"), configPagina + 1);
  texto(83, 9, 1, GRAY, pagina);
  linha(5, 25, 123, 25, GRAY);
  for (byte i = 0; i < TOTAL_CONFIG_POR_PAGINA[configPagina]; i++) desenharItemConfig(i);
  mostrarTela();
}

void mostrarAjusteDataHora() {
  tela = AJUSTAR_DATA_HORA;
  limpar();
  texto(5, 5, 2, CYAN, F("DATA E HORA"));
  linha(5, 23, 123, 23, GRAY);
  const char *rotulos[6] = {"DIA", "MES", "ANO", "HORA", "MIN", "SEG"};
  for (byte i = 0; i < 6; i++) {
    int x = 5 + (i % 3) * 42;
    int y = 43 + (i / 3) * 35;
    texto(x, y, 1, i == rtcCampoEdicao ? CYAN : WHITE, rotulos[i]);
    char valor[5];
    snprintf(valor, sizeof(valor), "%02u", rtcEdicao[i]);
    texto(x, y + 13, 2, i == rtcCampoEdicao ? CYAN : WHITE, valor);
    if (i == rtcCampoEdicao) displayTFT.drawRoundRect(x - 2, y - 3, 36, 31, 3, corTFT(CYAN));
  }
  texto(5, 119, 1, GRAY, F("SW: SALVAR"));
  mostrarTela();
}

void iniciarAjusteDataHora() {
  byte seg, minu, hora, dia, mes, ano;
  if (!rtcLerDataHora(seg, minu, hora, dia, mes, ano)) {
    dia = 1; mes = 1; ano = 26; hora = 12; minu = 0; seg = 0;
  }
  rtcEdicao[0] = dia; rtcEdicao[1] = mes; rtcEdicao[2] = ano;
  rtcEdicao[3] = hora; rtcEdicao[4] = minu; rtcEdicao[5] = seg;
  rtcCampoEdicao = 0;
  mostrarAjusteDataHora();
}

void ajustarCampoDataHora(int direcao) {
  byte v = rtcEdicao[rtcCampoEdicao];
  byte minimo = 0, maximo = 59;
  if (rtcCampoEdicao == 0) { minimo = 1; maximo = 31; }
  else if (rtcCampoEdicao == 1) { minimo = 1; maximo = 12; }
  else if (rtcCampoEdicao == 2) { minimo = 0; maximo = 99; }
  else if (rtcCampoEdicao == 3) { minimo = 0; maximo = 23; }
  if (direcao > 0) v = (v >= maximo) ? minimo : v + 1;
  else v = (v <= minimo) ? maximo : v - 1;
  rtcEdicao[rtcCampoEdicao] = v;
  byte dias = 31;
  if (rtcEdicao[1] == 4 || rtcEdicao[1] == 6 || rtcEdicao[1] == 9 || rtcEdicao[1] == 11) dias = 30;
  if (rtcEdicao[1] == 2) dias = ((rtcEdicao[2] % 4) == 0) ? 29 : 28;
  if (rtcEdicao[0] > dias) rtcEdicao[0] = dias;
  mostrarAjusteDataHora();
}

void salvarDataHoraRTC() {
  rtcDesproteger();
  rtcEscreverRegistrador(0x80, rtcParaBCD(rtcEdicao[5]));
  rtcEscreverRegistrador(0x82, rtcParaBCD(rtcEdicao[4]));
  rtcEscreverRegistrador(0x84, rtcParaBCD(rtcEdicao[3]));
  rtcEscreverRegistrador(0x86, rtcParaBCD(rtcEdicao[0]));
  rtcEscreverRegistrador(0x88, rtcParaBCD(rtcEdicao[1]));
  rtcEscreverRegistrador(0x8A, rtcParaBCD(1));
  rtcEscreverRegistrador(0x8C, rtcParaBCD(rtcEdicao[2]));
  rtcEscreverRegistrador(0x8E, 0x00);
  rtcUltimoSegundo = rtcUltimoMinuto = rtcUltimaHora = 255;
  rtcUltimoDia = rtcUltimoMes = rtcUltimoAno = 255;
  somSucesso();
  mostrarConfig();
}

void mostrarAjusteVelocidade() {
  tela = AJUSTAR_VELOCIDADE;
  limpar();

  texto(5, 5, 2, CYAN, F("VELOCIDADE")); 
  linha(5, 23, 123, 23, GRAY);
  const __FlashStringHelper *opcoes[3] = { F("LENTA"), F("NORMAL"), F("RAPIDA") };
  for (byte i = 0; i < 3; i++) {
    int y = 42 + i * 22;
    if (i == velocidadeSelecao) {
      linha(4, y - 4, 124, y - 4, CYAN);
      linha(4, y + 14, 124, y + 14, CYAN);
    }
    texto(9, y, 2, i == velocidadeSelecao ? CYAN : WHITE, opcoes[i]);
  }
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
  if (slot >= FILE_TOTAL_SLOTS) return;
  EEPROM.update(enderecoSlot(slot), 0);
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
    char nomeAntigo[FILE_NAME_SIZE + 1];
    lerNomeArquivo(1, nomeAntigo);
    if (strcmp(nomeAntigo, "CREDITOS.TXT") == 0) EEPROM.update(enderecoSlot(1), 0);
    return;
  }

  if (assinaturaValida && versaoFormato == 2) {
    char nomeAntigo[FILE_NAME_SIZE + 1];
    lerNomeArquivo(1, nomeAntigo);
    if (strcmp(nomeAntigo, "CREDITOS.TXT") == 0) EEPROM.update(enderecoSlot(1), 0);
    return;
  }

  EEPROM.update(0, FILE_MAGIC_0);
  EEPROM.update(1, FILE_MAGIC_1);
  EEPROM.update(2, FILE_MAGIC_2);
  EEPROM.update(3, 2);
  for (byte i = 4; i < FILE_HEADER_SIZE; i++) EEPROM.update(i, 0);

  for (byte slot = 0; slot < FILE_TOTAL_SLOTS; slot++) {
    EEPROM.update(enderecoSlot(slot), 0);
  }

}

void atualizarListaArquivos() {
  qtdArquivos = 0;
  temEspacoArquivo = false;

  for (byte slot = 0; slot < FILE_TOTAL_SLOTS; slot++) {
    if (arquivoExiste(slot)) {
      indicesArquivos[qtdArquivos++] = slot;
    } else {
      temEspacoArquivo = true;
    }
  }
}


void calcularArmazenamento(uint16_t &usados, uint16_t &capacidade) {
  usados = 0;
  capacidade = FILE_TOTAL_SLOTS * FILE_SLOT_SIZE;

  for (byte slot = 0; slot < FILE_TOTAL_SLOTS; slot++) {
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

// Atualiza somente uma linha visível da lista, incluindo seu destaque.
void desenharItemListaArquivo(byte item, byte primeiro) {
  if (item < primeiro) return;
  byte linhaLista = item - primeiro;
  if (linhaLista >= 5) return;

  byte totalItens = qtdArquivos + (temEspacoArquivo ? 1 : 0) + 1;
  if (item >= totalItens) return;

  int y = 43 + linhaLista * 17;
  bool ativo = (item == selecionadoArquivo);

  // Limpa apenas a área do texto desta linha.
  displayTFT.fillRect(4, y - 1, 120, 12, ST77XX_BLACK);

  if (item < qtdArquivos) {
    char nome[FILE_NAME_SIZE + 1];
    lerNomeArquivo(indicesArquivos[item], nome);
    texto(7, y, 1, ativo ? CYAN : WHITE, nome);
  } else if (temEspacoArquivo && item == qtdArquivos) {
    texto(7, y, 1, ativo ? CYAN : WHITE, F("NOVO ARQUIVO"));
  } else {
    texto(7, y, 1, ativo ? CYAN : WHITE, F("VOLTAR"));
  }

  uint16_t cor = ativo ? corTFT(CYAN) : ST77XX_BLACK;
  displayTFT.drawFastHLine(4, y - 3, 120, cor);
  displayTFT.drawFastHLine(4, y + 13, 120, cor);
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
    desenharItemListaArquivo(item, primeiro);
  }

  char memoria[20];
  snprintf_P(memoria, sizeof(memoria), PSTR("%u/5 ARQUIVOS"), qtdArquivos);
  texto(5, 137, 1, GRAY, memoria);
  mostrarTela();
}

void criarArquivoNovo() {
  byte slotLivre = 255;
  for (byte slot = 0; slot < FILE_TOTAL_SLOTS; slot++) {
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
  corTextoAtual = 0;
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
    corTextoAtual = ultimaCorConteudo();
    paginaArquivo = 0;
    acaoArquivo = 0;
    mostrarLeitorArquivo();
  }
}


uint16_t corMarcadorTFT(byte cor) {
  if (cor == MARCA_VERMELHO) return displayTFT.color565(255, 55, 55);
  if (cor == MARCA_VERDE) return displayTFT.color565(45, 230, 110);
  if (cor == MARCA_AZUL) return displayTFT.color565(65, 145, 255);
  return ST77XX_WHITE;
}
byte ultimaCorConteudo() {
  byte cor=0;
  for(uint16_t i=0;i<tamanhoConteudo;i++){
    byte v=(byte)conteudoArquivo[i];
    if(v>=MARCA_VERMELHO&&v<=MARCA_BRANCO) cor=(v==MARCA_BRANCO)?0:v;
  }
  return cor;
}
uint16_t contarCaracteresVisiveis() {
  uint16_t total=0;
  for(uint16_t i=0;i<tamanhoConteudo;i++){
    byte v=(byte)conteudoArquivo[i];
    if(v<MARCA_VERMELHO||v>MARCA_BRANCO) total++;
  }
  return total;
}
bool adicionarCaractereColorido(char c) {
  if(tamanhoConteudo>=FILE_MAX_TEXT) return false;
  byte anterior=ultimaCorConteudo();
  byte marcador=corTextoAtual==0?MARCA_BRANCO:corTextoAtual;
  if(anterior!=corTextoAtual){
    conteudoArquivo[tamanhoConteudo++]=(char)marcador;
  }
  if(tamanhoConteudo>=FILE_MAX_TEXT){
    if((byte)conteudoArquivo[tamanhoConteudo-1]==marcador) tamanhoConteudo--;
    conteudoArquivo[tamanhoConteudo]='\0';
    return false;
  }
  conteudoArquivo[tamanhoConteudo++]=c;
  conteudoArquivo[tamanhoConteudo]='\0';
  return true;
}
void desenharTrechoColorido(uint16_t inicioVisivel,uint16_t limite,int x,int y) {
  byte cor=0,coluna=0,linhaAtual=0;
  uint16_t visivel=0;
  displayTFT.setTextSize(1);
  displayTFT.setTextWrap(false);
  for(uint16_t i=0;i<tamanhoConteudo&&visivel<inicioVisivel+limite;i++){
    byte v=(byte)conteudoArquivo[i];
    if(v>=MARCA_VERMELHO&&v<=MARCA_BRANCO){cor=(v==MARCA_BRANCO)?0:v;continue;}
    if(visivel++<inicioVisivel) continue;
    if(v=='\n'){coluna=0;linhaAtual++;continue;}
    if(coluna>=18){coluna=0;linhaAtual++;}
    if(linhaAtual>=4) break;
    displayTFT.setTextColor(cor==0?ST77XX_WHITE:corMarcadorTFT(cor),ST77XX_BLACK);
    displayTFT.setCursor(x+coluna*6,y+linhaAtual*17);
    displayTFT.write((char)v);
    coluna++;
  }
}

void mostrarLeitorArquivo() {
  tela = FILE_VIEW;
  limpar();
  texto(5, 5, 2, CYAN, nomeArquivoAtual);
  linha(5, 23, 123, 23, GRAY);

  uint16_t totalVisivel=contarCaracteresVisiveis();
  uint16_t inicio=(uint16_t)paginaArquivo*72;
  desenharTrechoColorido(inicio,72,5,29);
  byte totalPaginas=(totalVisivel+71)/72;
  if(totalPaginas==0) totalPaginas=1;
  char paginaTexto[16];
  snprintf_P(paginaTexto, sizeof(paginaTexto), PSTR("PAG %u/%u"), paginaArquivo + 1, totalPaginas);
  texto(5, 99, 1, GRAY, paginaTexto);

  const __FlashStringHelper *acao = F("VOLTAR");
  if (acaoArquivo == 0) acao = F("EDITAR");
  else if (acaoArquivo == 1) acao = F("RENOMEAR");
  else if (acaoArquivo == 2) acao = F("APAGAR");
  texto(5, 108, 1, CYAN, acao);
  mostrarTela();
}

byte quantidadeTeclasQwerty(byte linhaTeclado) {
  switch (linhaTeclado) {
    case 0: return 10;
    case 1: return 9;
    case 2: return 8;
    case 3: return 10;
    default: return 0;
  }
}

int inicioLinhaQwerty(byte linhaTeclado) {
  if (linhaTeclado == 1) return 11;
  if (linhaTeclado == 2) return 11;
  return 5;
}

int centroTeclaQwerty(byte linhaTeclado, byte coluna) {
  return inicioLinhaQwerty(linhaTeclado) + coluna * 12 + 5;
}

char lerTeclaQwerty(byte linhaTeclado, byte coluna) {
  if (linhaTeclado >= TECLADO_LINHAS) return 0;
  if (coluna >= quantidadeTeclasQwerty(linhaTeclado)) return 0;
  if (linhaTeclado == 2 && coluna == 0) return 0; // Tecla de alternancia ABC/SIMBOLOS.
  byte indice = (linhaTeclado == 2) ? coluna - 1 : coluna;
  if (tecladoSimbolos) return (char)pgm_read_byte(&TECLADO_SIMBOLOS[linhaTeclado][indice]);
  return (char)pgm_read_byte(&TECLADO_QWERTY[linhaTeclado][indice]);
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
  const int centrosAcoes[TOTAL_ACOES_TECLADO] = {12, 38, 64, 90, 115};
  byte melhor = 0;
  int menorDistancia = 32767;

  for (byte i = 0; i < TOTAL_ACOES_TECLADO; i++) {
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
    const int posicoes[TOTAL_ACOES_TECLADO] = {1,26,52,78,104};
    const int larguras[TOTAL_ACOES_TECLADO] = {23,24,24,24,23};
    const char *rotulos[TOTAL_ACOES_TECLADO] = {"DEL","ESP","ENT","RGB","OK"};
    int x = posicoes[col], largura = larguras[col];

    // Teclas de acao vazadas, iguais as teclas de caracteres.
    // O contorno fica azul normalmente e acompanha a cor de texto ao selecionar.
    uint16_t corContorno = displayTFT.color565(37, 139, 255);
    if (ativo) {
      if (corTextoAtual == MARCA_VERMELHO || corTextoAtual == MARCA_VERDE)
        corContorno = corMarcadorTFT(corTextoAtual);
      else
        corContorno = ST77XX_WHITE;
    }
    displayTFT.drawRoundRect(x, 113, largura, 12, 2, corContorno);
    desenharTextoTFT(x + (largura - (strlen(rotulos[col]) * 6)) / 2, 116, 1,
                     ST77XX_WHITE, rotulos[col], false);
    return;
  }

  if (row >= TECLADO_LINHAS || col >= quantidadeTeclasQwerty(row)) return;
  int x = inicioLinhaQwerty(row) + col * 12;
  int y = 55 + row * 13; // Posição vertical original do teclado.
  // Teclas ficam vazadas, somente com contorno azul.
  // A seleção usa a cor escolhida no RGB; azul vira branco para continuar visível.
  displayTFT.drawRect(x, y, 10, 11, ST77XX_BLACK);
  uint16_t corContorno = displayTFT.color565(37, 139, 255); // azul padrão
  if (ativo) {
    if (corTextoAtual == MARCA_AZUL) corContorno = ST77XX_WHITE;
    else if (corTextoAtual == MARCA_VERMELHO || corTextoAtual == MARCA_VERDE)
      corContorno = corMarcadorTFT(corTextoAtual);
    else corContorno = ST77XX_WHITE;
  }
  displayTFT.drawRect(x, y, 10, 11, corContorno);
  const __FlashStringHelper *corLetra = F("#FFFFFF");
  if (row == 2 && col == 0) {
    textoCentro(x + 5, y + 2, 1, corLetra, tecladoSimbolos ? F("A") : F("?"));
    return;
  }
  char etiqueta[2] = { lerTeclaQwerty(row, col), '\0' };
  textoCentro(x + 5, y + 2, 1, corLetra, etiqueta);
}

void redesenharTeclasCaracteres() {
  // Alterna letras/simbolos redesenhando somente a area das teclas.
  for (byte row = 0; row < TECLADO_LINHAS; row++) {
    for (byte col = 0; col < quantidadeTeclasQwerty(row); col++) {
      desenharItemTeclado(row, col, (tecladoLinha == row && tecladoColuna == col));
    }
  }
}

void mostrarEditorArquivo() {
  tela = FILE_EDIT;
  limpar();

  texto(5, 4, 2, CYAN,
        editandoNomeArquivo ? F("RENOMEAR") : F("EDITAR ARQUIVO"));

  texto(5, 21, 1, GRAY, nomeArquivoAtual);

  if(editandoNomeArquivo){
    char trecho[19]; byte n=0;
    while(n<18&&n<tamanhoNomeTemporario){trecho[n]=nomeTemporario[n];n++;}
    trecho[n]='\0'; texto(5,34,1,WHITE,trecho);
  }else desenharPreviewConteudo();
  char contador[20];
  if(editandoNomeArquivo) snprintf_P(contador,sizeof(contador),PSTR("%u/8 LETRAS"),tamanhoNomeTemporario);
  else snprintf_P(contador,sizeof(contador),PSTR("%u/%u BYTES"),tamanhoConteudo,FILE_MAX_TEXT);
  texto(5,46,1,YELLOW,contador);

  for (byte row = 0; row < TECLADO_LINHAS; row++) {
    for (byte col = 0; col < quantidadeTeclasQwerty(row); col++) {
      desenharItemTeclado(row, col, (tecladoLinha == row && tecladoColuna == col));
    }
  }
  for (byte col = 0; col < TOTAL_ACOES_TECLADO; col++) {
    desenharItemTeclado(TECLADO_LINHA_ACOES, col, (tecladoLinha == TECLADO_LINHA_ACOES && tecladoColuna == col));
  }

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



void desenharPreviewConteudo(){
  displayTFT.fillRect(5,32,displayTFT.width()-10,12,ST77XX_BLACK);
  uint16_t inicio=0,visivel=0;
  for(uint16_t i=0;i<tamanhoConteudo;i++){
    byte v=(byte)conteudoArquivo[i];
    if(v>=MARCA_VERMELHO&&v<=MARCA_BRANCO) continue;
    if(v=='\n') inicio=visivel+1;
    visivel++;
  }
  uint16_t qtd=visivel-inicio;
  uint16_t comeco=qtd>18?visivel-18:inicio;
  desenharTrechoColorido(comeco,18,5,34);
}

// Atualiza somente a célula de pixels da nova letra no campo de pré-visualização.
// Quando o texto ultrapassa 18 caracteres, a linha precisa rolar e é redesenhada.
void atualizarPreviewAposDigitar(char caractere){
  if(editandoNomeArquivo){
    displayTFT.fillRect(5,32,displayTFT.width()-10,12,ST77XX_BLACK);
    byte inicio=tamanhoNomeTemporario>18?tamanhoNomeTemporario-18:0;
    char trecho[19]; byte n=0;
    for(byte i=inicio;i<tamanhoNomeTemporario&&n<18;i++) trecho[n++]=nomeTemporario[i];
    trecho[n]='\0'; texto(5,34,1,WHITE,trecho);
  }else desenharPreviewConteudo();
  displayTFT.fillRect(4,45,displayTFT.width()-8,10,ST77XX_BLACK);
  char contador[20];
  snprintf_P(contador,sizeof(contador),PSTR("%u/%u BYTES"),tamanhoConteudo,FILE_MAX_TEXT);
  texto(5,46,1,YELLOW,contador);
}

void selecionarTeclaEditor() {
  if (tecladoLinha == 2 && tecladoColuna == 0) {
    tecladoSimbolos = !tecladoSimbolos;
    somNavegacao();
    // Mantem documento, contador e botoes intactos; atualiza apenas as teclas.
    redesenharTeclasCaracteres();
    return;
  }

  if (tecladoLinha == TECLADO_LINHA_ACOES) {
    if(tecladoColuna==ACAO_DEL){
      if(editandoNomeArquivo){if(tamanhoNomeTemporario>0)nomeTemporario[--tamanhoNomeTemporario]='\0';}
      else if(tamanhoConteudo>0){
        tamanhoConteudo--;
        byte removido=(byte)conteudoArquivo[tamanhoConteudo];
        if(removido<MARCA_VERMELHO||removido>MARCA_BRANCO){
          if(tamanhoConteudo>0){
            byte ant=(byte)conteudoArquivo[tamanhoConteudo-1];
            if(ant>=MARCA_VERMELHO&&ant<=MARCA_BRANCO)tamanhoConteudo--;
          }
        }
        conteudoArquivo[tamanhoConteudo]='\0';
        corTextoAtual=ultimaCorConteudo();
      }
      // Atualiza somente a prévia e o contador; mantém o teclado intacto.
      atualizarPreviewAposDigitar('\0'); return;
    }
    if(tecladoColuna==ACAO_ESPACO||tecladoColuna==ACAO_ENTER){
      if(!editandoNomeArquivo){
        char c=tecladoColuna==ACAO_ESPACO?' ':'\n';
        adicionarCaractereColorido(c);
        atualizarPreviewAposDigitar(c);
      }
      return;
    }
    if(tecladoColuna==ACAO_RGB){
      corTextoAtual=(corTextoAtual+1)%4;
      desenharItemTeclado(TECLADO_LINHA_ACOES,ACAO_RGB,true);
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
  } else {
    adicionou = adicionarCaractereColorido(tecla);
  }

  if (adicionou) {
    // Não redesenha o teclado nem o editor inteiro: somente a nova letra.
    atualizarPreviewAposDigitar(tecla);
  }
}

void executarAcaoLeitor() {
  if (acaoArquivo == 0) {
    editandoNomeArquivo = false;
    corTextoAtual = ultimaCorConteudo();
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
  if (configPagina == 0) {
    if (configSelecionada == 0) mostrarAjusteFonte();
    else if (configSelecionada == 1) mostrarAjusteRotacao();
    else mostrarAjusteVelocidade();
  } else {
    if (configSelecionada == 0) iniciarAjusteDataHora();
    else if (configSelecionada == 1) desligarBOX();
    else if (configSelecionada == 2) reiniciarBOX();
    else mostrarMenu();
  }
}

void atualizarNavegacao() {
  const unsigned int intervalosSelecao[3] = { 260, 110, 45 };
  if (millis() - ultimoMovimento < intervalosSelecao[velocidadeSelecao]) return;

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
        if (novaAcao >= TOTAL_ACOES_TECLADO) novaAcao = TOTAL_ACOES_TECLADO - 1;
        tecladoColuna = novaAcao;
      }

      if (dy < 0) {
        const int centrosAcoes[TOTAL_ACOES_TECLADO] = {12, 38, 64, 90, 115};
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
    byte anterior = selecionadoArquivo;
    byte primeiroAnterior = (anterior >= 5) ? anterior - 4 : 0;

    selecionadoArquivo = (selecionadoArquivo + totalItens + dy) % totalItens;
    byte primeiroNovo = (selecionadoArquivo >= 5) ? selecionadoArquivo - 4 : 0;

    ultimoMovimento = millis();
    somNavegacao();

    if (primeiroAnterior != primeiroNovo) {
      // A janela visível mudou: reposiciona as linhas da lista.
      mostrarListaArquivos();
    } else {
      // A janela não mudou: atualiza somente a linha antiga e a nova.
      desenharItemListaArquivo(anterior, primeiroNovo);
      desenharItemListaArquivo(selecionadoArquivo, primeiroNovo);
      // O display ja esta ligado; nao reenviar DISPON durante redesenho parcial.
    }
    return;
  }

  if (tela == FILE_VIEW) {
    int dx = lerDirecaoX();
    int dy = lerDirecaoY();
    if (dx == 0 && dy == 0) return;
    bool paginaMudou = false;
    if (dx != 0) acaoArquivo = (acaoArquivo + 4 + dx) % 4;
    if (dy < 0 && paginaArquivo > 0) { paginaArquivo--; paginaMudou = true; }
    if (dy > 0) {
      byte totalPaginas = (contarCaracteresVisiveis() + 71) / 72;
      if (totalPaginas == 0) totalPaginas = 1;
      if (paginaArquivo + 1 < totalPaginas) { paginaArquivo++; paginaMudou = true; }
    }
    ultimoMovimento = millis();
    if (paginaMudou) mostrarLeitorArquivo();
    else if (dx != 0) atualizarBarraLeitorArquivo();
    return;
  }

  int direcao;
  if (tela == AJUSTAR_FONTE || tela == AJUSTAR_ROTACAO || tela == AJUSTAR_VELOCIDADE) {
    direcao = lerDirecaoX();
    if (direcao == 0) return;
  } else {
    direcao = lerDirecao();
    if (direcao == 0) return;
  }

  ultimoMovimento = millis();
  somNavegacao();

  if (tela == MENU) {
    byte anterior = selecionado;
    selecionado = (selecionado + TOTAL_APPS + direcao) % TOTAL_APPS;

    // Atualiza somente a opção que perdeu o foco e a que recebeu o foco.
    desenharItemMenu(anterior);
    desenharItemMenu(selecionado);
    // Atualizacao parcial: nao reenviar DISPON ao controlador.
  }
  else if (tela == CONFIG) {
    byte anterior = configSelecionada;
    configSelecionada = (configSelecionada + TOTAL_CONFIG + direcao) % TOTAL_CONFIG;

    // Atualiza somente a opção antiga e a nova.
    desenharItemConfig(anterior);
    desenharItemConfig(configSelecionada);
    // Atualizacao parcial: nao reenviar DISPON ao controlador.
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
  else if (tela == AJUSTAR_VELOCIDADE) {
    if (direcao < 0 && velocidadeSelecao > 0) velocidadeSelecao--;
    if (direcao > 0 && velocidadeSelecao < 2) velocidadeSelecao++;
    salvarConfiguracoes();
    mostrarAjusteVelocidade();
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
    case AJUSTAR_VELOCIDADE:
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
  pinMode(BOTAO_EXTRA, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(RTC_CE, OUTPUT);
  pinMode(RTC_CLK, OUTPUT);
  pinMode(RTC_DAT, INPUT);
  digitalWrite(RTC_CE, LOW);
  digitalWrite(RTC_CLK, LOW);
  rtcDesproteger();
  if (rtcLerRegistrador(0x81) & 0x80) rtcDefinirHoraCompilacao();
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
  atualizarRelogioMenu();
}
