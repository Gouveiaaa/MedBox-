#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <LittleFS.h>
#include <time.h>
#include <stdlib.h>
#include "secrets.h"

// Prototipos explicitos para evitar geracao incorreta pelo ArduinoDroid.
void raiz();
void mostrarLogin();
void enviarLogin();
void sair();
void salvar();
void limparGaveta();
void silenciar();
void testarLeds();
void paginaLogin(bool invalido, bool bloqueado);
void atualizarOled(bool forcar);
void confirmarDose();
void desfazerConfirmacao();

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// HW-364A: OLED interno SDA = GPIO14 (D6) e SCL = GPIO12 (D5).
// Altere para false SOMENTE se seu hardware usar o OLED em D2/D1.
constexpr bool OLED_HW364A = true;
constexpr uint8_t OLED_SDA_PIN = OLED_HW364A ? 14 : 4;
constexpr uint8_t OLED_SCL_PIN = OLED_HW364A ? 12 : 5;
constexpr uint8_t OLED_ENDERECO = 0x3C;
// Pinos escolhidos para nao usar GPIO0, GPIO2, GPIO15 nem os pinos do OLED.
constexpr uint8_t LED_PINS[] = { 5, 4, 13 }; // D1, D2, D7
// Sinalizacao exclusivamente luminosa: nenhum buzzer e acionado.
constexpr unsigned long DURACAO_ALERTA_LED = 60000UL;
constexpr unsigned long INTERVALO_LED = 500UL;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ESP8266WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "a.st1.ntp.br", -10800, 60000);
bool oledDisponivel = false;
bool horaSincronizada = false;
bool mdnsAtivo = false;
bool wifiEstavaConectado = false;
// Uma sessao de administrador em RAM, expira apos 30 minutos sem requisicoes.
constexpr unsigned long SESSAO_INATIVIDADE_MS = 30UL * 60UL * 1000UL;
constexpr unsigned long BLOQUEIO_LOGIN_MS = 60UL * 1000UL;
String sessaoToken;
String csrfToken;
unsigned long ultimaAtividadeSessao = 0;
unsigned long primeiraFalhaLogin = 0;
uint8_t falhasLogin = 0;
bool testeLedsAtivo = false;
unsigned long inicioTesteLeds = 0;
constexpr unsigned long DURACAO_ETAPA_TESTE_MS = 3000UL;
constexpr unsigned long DURACAO_TESTE_LEDS_MS = 3UL * DURACAO_ETAPA_TESTE_MS;
unsigned long ultimaTentativaWiFi = 0;
unsigned long ultimaTentativaNTP = 0;
struct Gaveta {
  uint8_t hora, minuto;
  String remedio;
  long ultimoDia; // evita repetição no mesmo dia durante execução
  unsigned long inicioAlertaLed;
  bool alertaLedAtivo;
};
Gaveta gavetas[3] = {
  {255, 0, "", -1, 0, false},
  {255, 0, "", -1, 0, false},
  {255, 0, "", -1, 0, false}
};
// Hora 255 significa gaveta sem medicamento cadastrado.

bool armazenamentoDisponivel=false;
constexpr uint8_t MAX_CONFIRMACOES=24;
struct ConfirmacaoDose {
  uint32_t instanteLocal;
  uint8_t gaveta;
  uint16_t minutoProgramado;
  String medicamento;
};
ConfirmacaoDose confirmacoes[MAX_CONFIRMACOES];
uint8_t totalConfirmacoes=0;

void adicionarConfirmacao(const ConfirmacaoDose &r) {
  if(totalConfirmacoes==MAX_CONFIRMACOES) {
    for(uint8_t i=1;i<MAX_CONFIRMACOES;i++)confirmacoes[i-1]=confirmacoes[i];
    totalConfirmacoes--;
  }
  confirmacoes[totalConfirmacoes++]=r;
}
void carregarConfirmacoes() {
  if(!armazenamentoDisponivel)return;
  File f=LittleFS.open("/confirmacoes.txt","r");
  if(!f)return;
  while(f.available()) {
    String linha=f.readStringUntil('\n');linha.trim();
    int a=linha.indexOf('|'),b=linha.indexOf('|',a+1),d=linha.indexOf('|',b+1);
    if(a<1||b<=a||d<=b)continue;
    String quando=linha.substring(0,a),idx=linha.substring(a+1,b);
    String horario=linha.substring(b+1,d),nome=linha.substring(d+1);
    if(quando.length()!=10||idx.length()!=1||idx[0]<'0'||idx[0]>'2'||
       horario.length()<1||horario.length()>4||nome.length()<1||nome.length()>40||
       nome.indexOf('|')>=0)continue;
    bool valido=true;
    for(unsigned int i=0;i<quando.length();i++)if(quando[i]<'0'||quando[i]>'9')valido=false;
    for(unsigned int i=0;i<horario.length();i++)if(horario[i]<'0'||horario[i]>'9')valido=false;
    if(!valido)continue;
    uint32_t t=strtoul(quando.c_str(),nullptr,10);
    uint16_t min=(uint16_t)horario.toInt();
    if(t<1700000000UL||min>=1440)continue;
    ConfirmacaoDose r={t,(uint8_t)(idx[0]-'0'),min,nome};
    adicionarConfirmacao(r);
  }
  f.close();
}
bool persistirConfirmacao(const ConfirmacaoDose &novo) {
  if(!armazenamentoDisponivel)return false;
  File f=LittleFS.open("/confirmacoes.tmp","w");
  if(!f)return false;
  bool gravou=true;
  uint8_t inicio=(totalConfirmacoes==MAX_CONFIRMACOES)?1:0;
  for(uint8_t i=inicio;i<totalConfirmacoes;i++) {
    const ConfirmacaoDose &r=confirmacoes[i];
    String linha=String(r.instanteLocal)+"|"+String(r.gaveta)+"|"+
                 String(r.minutoProgramado)+"|"+r.medicamento;
    if(f.println(linha)==0)gravou=false;
  }
  String nova=String(novo.instanteLocal)+"|"+String(novo.gaveta)+"|"+
              String(novo.minutoProgramado)+"|"+novo.medicamento;
  if(f.println(nova)==0)gravou=false;
  f.flush();f.close();
  if(!gravou||!LittleFS.rename("/confirmacoes.tmp","/confirmacoes.txt")) {
    LittleFS.remove("/confirmacoes.tmp");
    return false;
  }
  adicionarConfirmacao(novo);
  return true;
}
// Encontra exatamente o registro que corresponde a esta gaveta, horario e dia.
int localizarConfirmacaoHoje(uint8_t indice,uint32_t dia) {
  if(indice>=3 || gavetas[indice].hora>=24)return -1;
  const uint16_t horario=gavetas[indice].hora*60+gavetas[indice].minuto;
  for(int i=totalConfirmacoes-1;i>=0;i--){
    const ConfirmacaoDose &r=confirmacoes[i];
    if(r.gaveta==indice && r.minutoProgramado==horario &&
       r.instanteLocal/86400UL==dia)return i;
  }
  return -1;
}
bool confirmadaHoje(uint8_t indice,uint32_t dia) {
  return localizarConfirmacaoHoje(indice,dia)>=0;
}

// So altera o historico em RAM depois que a gravacao na flash for concluida.
bool removerConfirmacaoPersistida(uint8_t posicao) {
  if(!armazenamentoDisponivel || posicao>=totalConfirmacoes)return false;
  File f=LittleFS.open("/confirmacoes.tmp","w");
  if(!f)return false;
  bool gravou=true;
  for(uint8_t i=0;i<totalConfirmacoes;i++){
    if(i==posicao)continue;
    const ConfirmacaoDose &r=confirmacoes[i];
    const String linha=String(r.instanteLocal)+"|"+String(r.gaveta)+"|"+
                      String(r.minutoProgramado)+"|"+r.medicamento;
    if(f.println(linha)==0)gravou=false;
  }
  f.flush();
  f.close();
  if(!gravou || !LittleFS.rename("/confirmacoes.tmp","/confirmacoes.txt")){
    LittleFS.remove("/confirmacoes.tmp");
    return false;
  }
  for(uint8_t i=posicao+1;i<totalConfirmacoes;i++)confirmacoes[i-1]=confirmacoes[i];
  totalConfirmacoes--;
  return true;
}




void salvarConfiguracao() {
  File f = LittleFS.open("/gavetas.txt", "w");
  if (!f) return;
  for (int i=0;i<3;i++) {
    f.println(String(gavetas[i].hora) + "|" + String(gavetas[i].minuto) + "|" + gavetas[i].remedio);
  }
  f.close();
}
void carregarConfiguracao() {
  File f = LittleFS.open("/gavetas.txt", "r");
  if (!f) return;
  for (int i=0;i<3 && f.available();i++) {
    String linha=f.readStringUntil('\n'); linha.trim();
    int a=linha.indexOf('|'), b=linha.indexOf('|',a+1);
    if (a<0 || b<0) continue;
    int h=linha.substring(0,a).toInt(), m=linha.substring(a+1,b).toInt();
    String nome=linha.substring(b+1);
    if (h==255) {
      gavetas[i].hora=255; gavetas[i].minuto=0; gavetas[i].remedio="";
    } else if (h>=0 && h<24 && m>=0 && m<60 && nome.length()>0 && nome.length()<=40) {
      gavetas[i].hora=h; gavetas[i].minuto=m; gavetas[i].remedio=nome;
    }
  }
  f.close();
}
void tela(const String &a, const String &b, const String &c) {
  if (!oledDisponivel) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0); display.println(F("=== MEDBOX SMART ==="));
  display.setCursor(0, 18); display.println(a);
  display.setCursor(0, 32); display.println(b);
  display.setCursor(0, 48); display.println(c);
  display.display();
}
// Adafruit GFX usa fonte ASCII: translitera caracteres portugueses e corta
// nomes longos para nao estourar a largura fisica do display 128x64.
String textoOled(const String &entrada, uint8_t limite) {
  String saida;
  saida.reserve(limite);
  for (unsigned int i=0; i<entrada.length() && saida.length()<limite; i++) {
    uint8_t c=(uint8_t)entrada[i];
    if (c>=32 && c<=126) {
      saida+=(char)c;
    } else if (c==0xC3 && i+1<entrada.length()) {
      uint8_t d=(uint8_t)entrada[++i];
      char letra='?';
      if ((d>=0xA0 && d<=0xA5)) letra='a';
      else if (d>=0x80 && d<=0x85) letra='A';
      else if (d>=0xA8 && d<=0xAB) letra='e';
      else if (d>=0x88 && d<=0x8B) letra='E';
      else if (d>=0xAC && d<=0xAF) letra='i';
      else if (d>=0x8C && d<=0x8F) letra='I';
      else if (d>=0xB2 && d<=0xB6) letra='o';
      else if (d>=0x92 && d<=0x96) letra='O';
      else if (d>=0xB9 && d<=0xBC) letra='u';
      else if (d>=0x99 && d<=0x9C) letra='U';
      else if (d==0xA7) letra='c';
      else if (d==0x87) letra='C';
      saida+=letra;
    } else if (c>=0x80) {
      // Pula bytes de continuacao UTF-8; desconhecidos viram '?'.
      if ((c&0xC0)==0x80) continue;
      saida+='?';
    }
  }
  return saida;
}

// OLED principal: relogio e proximo lembrete atualizados automaticamente.
// Alerta real tem prioridade durante os mesmos 60 segundos de pisca do LED.
void atualizarOled(bool forcar) {
  if (!oledDisponivel) return;
  static unsigned long ultimaAtualizacao=0;
  unsigned long agora=millis();
  if (!forcar && agora-ultimaAtualizacao<1000UL) return;
  ultimaAtualizacao=agora;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print(F("MEDBOX"));
  if(horaSincronizada){
    uint32_t dia=timeClient.getEpochTime()/86400UL;
    uint8_t ok=0,ativas=0;
    for(uint8_t i=0;i<3;i++){
      if(gavetas[i].hora<24&&gavetas[i].remedio.length()>0){
        ativas++; if(confirmadaHoje(i,dia))ok++;
      }
    }
    display.setCursor(40,0);
    display.print(ok);display.print('/');display.print(ativas);
  }

  if (horaSincronizada) {
    char horario[9];
    snprintf(horario, sizeof(horario), "%02d:%02d:%02d",
             timeClient.getHours(), timeClient.getMinutes(), timeClient.getSeconds());
    display.setCursor(78,0);
    display.print(horario);
  } else {
    display.setCursor(78,0);
    display.print(F("--:--:--"));
  }
  display.drawFastHLine(0,10,128,SSD1306_WHITE);

  int8_t ativa=-1;
  for (uint8_t i=0;i<3;i++) {
    if (gavetas[i].alertaLedAtivo) { ativa=i; break; }
  }
  if (ativa>=0) {
    display.setCursor(0,16); display.print(F("HORA DO REMEDIO!"));
    display.setCursor(0,29); display.print(F("Gaveta ")); display.print(ativa+1);
    display.setCursor(0,42); display.print(textoOled(gavetas[ativa].remedio,21));
    display.setCursor(0,55); display.print(F("LED por 1 min"));
  } else if (!horaSincronizada) {
    display.setCursor(0,16); display.print(F("Aguardando horario NTP"));
    display.setCursor(0,30);
    display.print(WiFi.status()==WL_CONNECTED ? F("Wi-Fi conectado") : F("Conectando Wi-Fi"));
    display.setCursor(0,45); display.print(F("Proximo: sem hora"));
  } else {
    int8_t proxima=-1;
    uint16_t espera=1441;
    const uint16_t minutoAtual=timeClient.getHours()*60+timeClient.getMinutes();
    for (uint8_t i=0;i<3;i++) {
      const Gaveta &g=gavetas[i];
      if (g.hora>=24 || g.remedio.length()==0) continue;
      uint16_t agendado=g.hora*60+g.minuto;
      uint16_t falta=(agendado+1440-minutoAtual)%1440;
      if (falta==0) falta=1440; // Ja nao esta em alerta: proximo e amanha.
      if (falta<espera) { espera=falta; proxima=i; }
    }
    if (proxima<0) {
      display.setCursor(0,18); display.print(F("Nenhuma gaveta ativa"));
      display.setCursor(0,34); display.print(F("Cadastre pelo site"));
    } else {
      const Gaveta &g=gavetas[proxima];
      display.setCursor(0,16); display.print(F("Prox: Gaveta ")); display.print(proxima+1);
      display.setCursor(0,29);
      if (g.hora<10) display.print('0');
      display.print(g.hora); display.print(':');
      if (g.minuto<10) display.print('0');
      display.print(g.minuto);
      display.print(F(" em "));
      display.print(espera/60);
      display.print(F("h"));
      display.print(espera%60);
      display.print(F("m"));
      display.setCursor(0,42); display.print(textoOled(g.remedio,21));
    }
    display.setCursor(0,55);
    if (WiFi.status()==WL_CONNECTED) {
      display.print(F("IP "));
      display.print(WiFi.localIP());
    } else {
      display.print(F("Wi-Fi desconectado"));
    }
  }
  display.display();
}

String escapeHtml(const String &s) {
  String r;
  for (unsigned int i=0; i<s.length(); i++) {
    char c=s[i];
    if(c=='&') r+="&amp;";
    else if(c=='<') r+="&lt;";
    else if(c=='>') r+="&gt;";
    else if(c=='"') r+="&quot;";
    else if(c=='\'') r+="&#39;";
    else r+=c;
  }
  return r;
}
// Sem TLS, estas protecoes valem apenas para uma rede local confiavel.
void cabecalhosSemCache() {
  server.sendHeader("Cache-Control", "no-store, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader("Referrer-Policy", "no-referrer");
  server.sendHeader("X-Frame-Options", "DENY");
}

String novoToken() {
  static const char digitos[] = "0123456789abcdef";
  uint8_t bytes[16];
  ESP.random(bytes, sizeof(bytes)); // Fonte de entropia do ESP8266 com Wi-Fi ligado.
  String token;
  token.reserve(32);
  for (uint8_t valor : bytes) {
    token += digitos[valor >> 4];
    token += digitos[valor & 15];
  }
  return token;
}

void redirecionar(const char* caminho) {
  cabecalhosSemCache();
  server.sendHeader("Location", caminho, true);
  server.send(303, "text/plain", "");
}

bool cookieDeSessaoValido() {
  if (sessaoToken.length() != 32) return false;
  const String cookie = server.header("Cookie");
  const String nome = "medbox_sid=";
  int inicio = 0;
  while (inicio < static_cast<int>(cookie.length())) {
    int fim = cookie.indexOf(';', inicio);
    if (fim < 0) fim = cookie.length();
    String parte = cookie.substring(inicio, fim);
    parte.trim();
    if (parte == nome + sessaoToken) return true;
    inicio = fim + 1;
  }
  return false;
}

bool sessaoValida() {
  if (sessaoToken.length() != 32) return false;
  if (millis() - ultimaAtividadeSessao >= SESSAO_INATIVIDADE_MS) {
    sessaoToken = "";
    csrfToken = "";
    return false;
  }
  if (!cookieDeSessaoValido()) return false;
  ultimaAtividadeSessao = millis();
  return true;
}

bool autorizado() {
  if (sessaoValida()) return true;
  redirecionar("/login");
  return false;
}

bool csrfValido() {
  if (csrfToken.length() != 32 || server.arg("csrf") != csrfToken) {
    cabecalhosSemCache();
    server.send(403, "text/plain; charset=utf-8", "Solicitacao invalida. Atualize a pagina e tente novamente.");
    return false;
  }
  return true;
}

void paginaLogin(bool invalido, bool bloqueado) {
  String html;
  html.reserve(3300);
  html += F(R"MEDBOX(<!doctype html><html lang="pt-BR"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#145bd7"><title>MedBox | Entrar</title>
<style>
*{box-sizing:border-box}html{-webkit-text-size-adjust:100%}
body{margin:0;min-height:100vh;min-height:100dvh;display:grid;place-items:center;background:#f5f7fb;color:#17233b;font:15px/1.5 system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;padding:22px}
main{width:100%;max-width:410px;background:#fff;border:1px solid #e3e8f1;border-radius:18px;padding:32px;box-shadow:0 18px 54px #17294a12}
.logo{height:48px;width:48px;display:grid;place-items:center;border-radius:14px;background:#145bd7;color:white;font-size:29px;font-weight:800;margin-bottom:20px}
h1{font-size:26px;letter-spacing:-.7px;margin:0;color:#142643}
p{color:#64748b;font-size:14px;margin:7px 0 23px}
label{display:block;font-size:13px;font-weight:650;margin:14px 0 6px;color:#394a62}
input{display:block;width:100%;min-width:0;background:white;border:1px solid #ccd5e3;border-radius:10px;padding:12px;font:inherit;font-size:16px;color:#192940}
input:focus{outline:2px solid #a4c6ff;outline-offset:1px;border-color:#3c7cf5}
button{display:block;width:100%;min-height:48px;margin-top:23px;border:0;border-radius:10px;background:#145bd7;color:#fff;font:650 16px system-ui;cursor:pointer}
button:focus-visible{outline:3px solid #9bc4ff;outline-offset:2px}
.aviso{padding:10px 12px;border-radius:9px;border:1px solid #f1c6c9;background:#fff4f4;color:#982c3a;font-size:13px}
footer{text-align:center;color:#7a879b;font-size:12px;margin-top:23px}
@media(max-width:420px){main{padding:25px 22px}}
</style></head><body><main><div class="logo" aria-hidden="true">+</div>
<h1>Entrar no MedBox</h1><p>Identifique-se para acessar suas gavetas e horários.</p>)MEDBOX");
  if (bloqueado) {
    html += F("<div class='aviso' role='alert'>Muitas tentativas. Tente novamente em 1 minuto.</div>");
  } else if (invalido) {
    html += F("<div class='aviso' role='alert'>Usuario ou senha incorretos.</div>");
  }
  html += F(R"MEDBOX(<form method="post" action="/login">
<label for="usuario">Usuário</label><input id="usuario" name="usuario" maxlength="64" autocomplete="username" required autofocus>
<label for="senha">Senha</label><input id="senha" name="senha" type="password" maxlength="128" autocomplete="current-password" required>
<button type="submit">Entrar</button></form>
<footer>Acesso reservado aos responsáveis pelo MedBox</footer></main></body></html>)MEDBOX");
  cabecalhosSemCache();
  server.send(200, "text/html; charset=utf-8", html);
}

void mostrarLogin() {
  if (sessaoValida()) {
    redirecionar("/");
    return;
  }
  paginaLogin(false, false);
}

void enviarLogin() {
  // Rate limit simples na memoria para desencorajar tentativas repetidas.
  const unsigned long agora = millis();
  if (falhasLogin > 0 && agora - primeiraFalhaLogin >= BLOQUEIO_LOGIN_MS) {
    falhasLogin = 0;
  }
  if (falhasLogin >= 5) {
    paginaLogin(false, true);
    return;
  }
  String usuario = server.arg("usuario");
  String senha = server.arg("senha");
  if (usuario == MEDBOX_ADMIN_USER && senha == MEDBOX_ADMIN_PASSWORD) {
    falhasLogin = 0;
    sessaoToken = novoToken();
    csrfToken = novoToken();
    ultimaAtividadeSessao = millis();
    cabecalhosSemCache();
    server.sendHeader("Set-Cookie", "medbox_sid=" + sessaoToken + "; Path=/; HttpOnly; SameSite=Strict");
    server.sendHeader("Location", "/", true);
    server.send(303, "text/plain", "");
    return;
  }
  if (falhasLogin == 0) primeiraFalhaLogin = agora;
  falhasLogin++;
  paginaLogin(true, falhasLogin >= 5);
}

void sair() {
  if (!autorizado() || !csrfValido()) return;
  sessaoToken = "";
  csrfToken = "";
  cabecalhosSemCache();
  server.sendHeader("Set-Cookie", "medbox_sid=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict");
  server.sendHeader("Location", "/login", true);
  server.send(303, "text/plain", "");
}
void atualizarLeds() {
  const unsigned long agora = millis();
  if (testeLedsAtivo && agora - inicioTesteLeds >= DURACAO_TESTE_LEDS_MS) {
    testeLedsAtivo = false;
  }
  uint8_t indiceTeste = 255;
  if (testeLedsAtivo) indiceTeste = (agora - inicioTesteLeds) / DURACAO_ETAPA_TESTE_MS;
  for (uint8_t i=0; i<3; i++) {
    Gaveta &g=gavetas[i];
    bool aceso=false;
    if (g.alertaLedAtivo && agora - g.inicioAlertaLed >= DURACAO_ALERTA_LED) {
      g.alertaLedAtivo=false;
    }
    if (testeLedsAtivo) {
      // Cada LED pisca durante 3 segundos, um por vez, total 9 segundos.
      aceso=(indiceTeste==i) && ((agora / INTERVALO_LED) % 2 == 0);
    } else {
      aceso=g.alertaLedAtivo && ((agora / INTERVALO_LED) % 2 == 0);
    }
    digitalWrite(LED_PINS[i], aceso ? HIGH : LOW);
  }
}

void testarLeds() {
  if (!autorizado() || !csrfValido()) return;
  for (uint8_t i=0; i<3; i++) {
    if (gavetas[i].alertaLedAtivo) {
      cabecalhosSemCache();
      server.send(409, "text/plain; charset=utf-8",
                  "Ha um lembrete ativo. Silencie-o antes de iniciar o teste de LEDs.");
      return;
    }
  }
  inicioTesteLeds=millis();
  testeLedsAtivo=true;
  atualizarLeds();
  redirecionar("/?teste=1#alertas");
}

void limparGaveta() {
  if (!autorizado() || !csrfValido()) return;
  String index=server.arg("index");
  if (index.length()!=1 || index[0]<'0' || index[0]>'2') {
    cabecalhosSemCache();
    server.send(400, "text/plain; charset=utf-8", "Gaveta invalida");
    return;
  }
  uint8_t i=index[0]-'0';
  gavetas[i].hora=255;
  gavetas[i].minuto=0;
  gavetas[i].remedio="";
  gavetas[i].ultimoDia=-1;
  gavetas[i].alertaLedAtivo=false;
  salvarConfiguracao();
  atualizarLeds();
  atualizarOled(true);
  redirecionar("/#gavetas");
}

void silenciar() {
  if (!autorizado() || !csrfValido()) return;
  // Interrompe os LEDs ativos, sem cancelar os lembretes futuros.
  testeLedsAtivo=false;
  for (uint8_t i=0; i<3; i++) gavetas[i].alertaLedAtivo=false;
  atualizarLeds();
  atualizarOled(true);
  redirecionar("/#alertas");
}
bool inteiroValido(const String &s, int minV, int maxV) {
  if(s.length()==0 || s.length()>2) return false;
  for(unsigned int i=0;i<s.length();i++) if(s[i]<'0'||s[i]>'9') return false;
  int v=s.toInt();
  return v>=minV && v<=maxV;
}
void confirmarDose() {
  if(!autorizado()||!csrfValido())return;
  if(!horaSincronizada||!armazenamentoDisponivel){
    cabecalhosSemCache();
    server.send(503,"text/plain; charset=utf-8",
                "Relogio ou armazenamento indisponivel. Confirmacao nao registrada.");
    return;
  }
  const String indiceStr=server.arg("index");
  if(!inteiroValido(indiceStr,0,2)){
    cabecalhosSemCache();
    server.send(400,"text/plain; charset=utf-8","Gaveta invalida");
    return;
  }
  uint8_t i=(uint8_t)indiceStr.toInt();
  Gaveta &g=gavetas[i];
  uint32_t agora=timeClient.getEpochTime(),dia=agora/86400UL;
  uint16_t minutos=(agora%86400UL)/60UL;
  uint16_t agendado=g.hora*60+g.minuto;
  if(g.hora>=24||g.remedio.length()==0||
     server.arg("dia")!=String(dia)||
     server.arg("horario")!=String(agendado)||
     minutos<agendado||confirmadaHoje(i,dia)){
    cabecalhosSemCache();
    server.send(409,"text/plain; charset=utf-8",
                "Dose ou formulario desatualizado. Atualize o painel.");
    return;
  }
  ConfirmacaoDose r={agora,i,agendado,g.remedio};
  if(!persistirConfirmacao(r)){
    cabecalhosSemCache();
    server.send(500,"text/plain; charset=utf-8","Falha ao salvar a confirmacao.");
    return;
  }
  g.alertaLedAtivo=false;
  g.ultimoDia=(long)dia;
  atualizarLeds();
  atualizarOled(true);
  redirecionar("/#historico");
}
// Desmarca uma confirmacao feita por engano, mesmo que seja de dia anterior.
// Revalida todos os identificadores para nao apagar outro registro se a tela
// estiver desatualizada. A acao nao dispara novamente o LED.
void desfazerConfirmacao() {
  if(!autorizado() || !csrfValido())return;
  if(!armazenamentoDisponivel){
    cabecalhosSemCache();
    server.send(503,"text/plain; charset=utf-8",
                "Armazenamento indisponivel. Confirmacao nao alterada.");
    return;
  }
  const String posStr=server.arg("registro");
  if(!inteiroValido(posStr,0,MAX_CONFIRMACOES-1)){
    cabecalhosSemCache();
    server.send(400,"text/plain; charset=utf-8","Registro invalido");
    return;
  }
  uint8_t pos=(uint8_t)posStr.toInt();
  if(pos>=totalConfirmacoes){
    cabecalhosSemCache();
    server.send(409,"text/plain; charset=utf-8","Registro nao encontrado. Atualize o painel.");
    return;
  }
  const ConfirmacaoDose &r=confirmacoes[pos];
  if(server.arg("instante")!=String(r.instanteLocal) ||
     server.arg("gaveta")!=String(r.gaveta) ||
     server.arg("horario")!=String(r.minutoProgramado)){
    cabecalhosSemCache();
    server.send(409,"text/plain; charset=utf-8","O historico mudou. Atualize o painel.");
    return;
  }
  if(!removerConfirmacaoPersistida(pos)){
    cabecalhosSemCache();
    server.send(500,"text/plain; charset=utf-8",
                "Nao foi possivel desmarcar. O registro original foi preservado.");
    return;
  }
  // Mantem ultimoDia: desfazer nao reinicia um LED passado nem altera horários.
  atualizarOled(true);
  redirecionar("/#historico");
}

void raiz() {
  if (!autorizado()) return;
  const bool horarioOK=horaSincronizada;
  const unsigned long horarioLocal=horarioOK ? timeClient.getEpochTime() : 0UL;
  String html;
  html.reserve(14500);
  html+=F(R"MBUI(<!doctype html><html lang="pt-BR"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#156dc7"><title>MedBox | Painel</title>
<style>
*{box-sizing:border-box}html{-webkit-text-size-adjust:100%} [hidden]{display:none!important}
body{margin:0;background:#f2f6fc;color:#11203a;font:15px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif}
a{color:inherit;text-decoration:none}
.confirmstatus{margin-top:12px;border-radius:9px;padding:10px 11px;background:#eff4fa;color:#435a76;font-size:13px}
.confirmstatus.ok{background:#e7f8ef;color:#166e45}
.confirmstatus.due{background:#fff4e4;color:#865314}
.confirmform{margin-top:11px}
.confirmbtn{width:100%;background:#17965a;color:#fff}
.confirmbtn:disabled{background:#c9d4de;color:#526477;cursor:not-allowed}
.history-row{display:flex;align-items:flex-start;justify-content:space-between;gap:12px;padding:10px 0;border-top:1px solid #e6ecf3;font-size:13px}
.history-row small{color:#687b94;white-space:nowrap}
.history-row b{color:#155a38}
.undoform{margin-top:8px}
.undobtn{width:100%;min-height:38px;background:#fff;color:#a13a49;border:1px solid #dfb7bf;font-size:13px}
.history-undo{flex:0 0 118px;text-align:right}
.history-undo .undobtn{margin-top:7px}
@media(max-width:410px){.history-row{flex-direction:column}.history-undo{width:100%;flex:auto;text-align:left}}
header{background:linear-gradient(105deg,#155aaf,#217acb);color:#fff;padding:14px 24px}
.head{max-width:1500px;margin:auto;display:flex;align-items:center;justify-content:space-between;gap:12px}
.logo{display:flex;align-items:center;gap:14px}
.mark{display:grid;place-items:center;width:54px;height:54px;border-radius:16px;background:#ffffff24;font-size:31px}
.brand{font-weight:800;font-size:clamp(23px,4vw,32px);letter-spacing:-1px;line-height:1}
.sub{font-size:12px;color:#e2ecff;margin-top:5px}
.head-right{display:flex;align-items:center;gap:14px}
.wifi{font-size:13px;text-align:right}.wifi small{display:block;color:#e2ecff;font-size:11px}
.online{display:inline-block;width:9px;height:9px;background:#24d768;border-radius:50%;margin-right:7px}
.settings{display:grid;place-items:center;background:#ffffff24;border:1px solid #ffffff44;border-radius:11px;width:44px;height:44px;font-size:23px}
.shell{max-width:1500px;margin:auto;display:grid;grid-template-columns:235px minmax(0,1fr)}
aside{padding:22px 12px;background:#fff;border-right:1px solid #e4eaf4;min-height:calc(100vh - 82px)}
nav{display:grid;gap:6px}nav a{display:flex;align-items:center;gap:12px;padding:11px 13px;border-radius:10px;color:#485970;font-weight:570;white-space:nowrap}
nav a:hover,nav a.active{background:#e6f1ff;color:#0c55a3;font-weight:720}
nav span{width:20px;text-align:center;font-size:19px}
main{min-width:0;padding:22px;display:grid;gap:16px;align-content:start}
.summary{display:grid;grid-template-columns:1.3fr 1fr;gap:16px}
.card{background:#fff;border:1px solid #e4eaf4;border-radius:15px;padding:19px;box-shadow:0 5px 19px #24487508;min-width:0}
.overline{color:#63748c;font-size:13px;margin:0 0 7px}
.clock{font-size:clamp(34px,5.5vw,58px);letter-spacing:-2px;font-weight:800;line-height:1.2;font-variant-numeric:tabular-nums}
.info{border-radius:10px;background:#e7f9f0;color:#20754c;padding:11px 13px;margin-top:15px;font-size:13px}
.next h2,.bottom h2{font-size:16px;color:#1a548f;margin:0 0 11px}
.next strong{font-size:19px}
.nextbox{display:flex;align-items:center;gap:12px;background:#fff5df;border-radius:11px;padding:16px}
.pill{font-size:28px}.muted{color:#65768e;font-size:13px}
.counter{font-weight:800;font-size:20px;font-variant-numeric:tabular-nums}
.grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:16px}
.drawer{--tint:#fdf0f1;--accent:#d9213e}
.drawer:nth-child(2){--tint:#edf5ff;--accent:#1766c8}
.drawer:nth-child(3){--tint:#ecfbf2;--accent:#179b5f}
.drawer-head,.drug{border-radius:9px;background:var(--tint);padding:11px 13px}
.drawer-head{color:var(--accent);font-size:16px;font-weight:750;margin-bottom:8px}
.drug{font-weight:750;min-height:58px;display:flex;align-items:center;gap:9px;overflow-wrap:anywhere}
.drug span{font-size:23px}
.time-label{margin:13px 0 0;font-size:12px;color:#5c6d85}
.timetext{font-size:22px;font-weight:770;font-variant-numeric:tabular-nums}
.tag{margin-top:8px;background:#eef3fa;border-radius:8px;color:#536884;padding:8px 9px;font-size:12px}
.buttons{display:grid;grid-template-columns:1fr 1fr;gap:9px;margin-top:13px}
button{font:inherit;font-weight:650;cursor:pointer;border-radius:9px;border:0;min-height:43px;padding:9px 12px}
button:focus-visible,a:focus-visible,input:focus-visible{outline:3px solid #9bc5ff;outline-offset:2px}
.blue{color:#fff;background:#1674df}.red{color:#fff;background:#e63d4f}
.outline{background:#fff;color:#155fbe;border:1px solid #1b65bc}
.editor{margin-top:14px;padding:12px;border-radius:10px;background:#f7f9fc}
label{font-size:12px;font-weight:650;color:#4b5e78;display:block;margin:8px 0 4px}
input{width:100%;background:#fff;border:1px solid #cbd6e6;padding:10px;border-radius:8px;font-size:16px;color:#13233f}
.two{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.editor button{width:100%;margin-top:13px}
.bottom{display:grid;grid-template-columns:1fr 1fr;gap:16px}
.bottom .buttons{grid-template-columns:1fr 1fr}
.tip{background:#e9f4ff;border-radius:9px;color:#355f8b;padding:12px;font-size:13px}
.hint{font-size:12px;color:#718096;margin-top:9px}
.logout{border:0;background:transparent;color:#a53949;min-height:30px;padding:5px 0;font-size:13px}
footer{font-size:12px;color:#718096;text-align:center;padding-bottom:12px}
@media(max-width:1120px){.grid{grid-template-columns:1fr 1fr}.shell{grid-template-columns:192px minmax(0,1fr)}}
@media(max-width:800px){header{padding:13px 16px}.shell{display:block}aside{min-height:0;padding:8px 12px;overflow-x:auto;border-right:0;border-bottom:1px solid #e4eaf4}nav{display:flex;width:max-content;gap:5px}nav a{padding:9px 12px;font-size:13px}main{padding:16px}.summary,.bottom{grid-template-columns:1fr}.grid{grid-template-columns:1fr 1fr}}
@media(max-width:510px){.grid{grid-template-columns:1fr}.wifi small{font-size:10px}.brand{font-size:23px}.sub{font-size:10px}.mark{width:43px;height:43px;font-size:24px}.head-right{gap:8px}.settings{width:37px;height:37px}.card{padding:15px}main{padding:12px}}
</style></head><body>
<header><div class="head"><div class="logo"><div class="mark" aria-hidden="true">💊</div><div><div class="brand">MedBox</div><div class="sub">Caixa de Remédios Inteligente</div></div></div><div class="head-right"><div class="wifi"><div><span class="online"></span>Conectado</div><small>IP: )MBUI");
  html+=WiFi.localIP().toString();
  html+=F(R"MBUI(</small></div><a class="settings" href="#sobre" aria-label="Informações e configurações">⚙</a></div></div></header>
<div class="shell"><aside><nav aria-label="Navegação">
<a class="active" href="#inicio"><span>⌂</span>Início</a>
<a href="#gavetas"><span>💊</span>Medicamentos</a>
<a href="#horarios"><span>◷</span>Horários</a>
<a href="#alertas"><span>♧</span>Alertas</a>
<a href="#historico"><span>✓</span>Histórico</a>
<a href="#sobre"><span>ⓘ</span>Sobre</a>
</nav></aside><main id="inicio"><div class="summary">
<section class="card"><p class="overline" id="data">Aguardando sincronização do horário</p>
<div class="clock" id="relogio" data-local=")MBUI");
  html+=String(horarioLocal);
  html+=F(R"MBUI(" data-dia=")MBUI");
  html+=String(horarioOK ? horarioLocal/86400UL : 0UL);
  html+=F(R"MBUI(">--:--:--</div><div class="info">● Wi-Fi conectado. )MBUI");
  if (horarioOK) html+=F("Relógio sincronizado.");
  else html+=F("Aguardando o relógio pela internet.");
  html+=F(R"MBUI(</div></section><section class="card next"><h2>◷ Próximo medicamento</h2><div class="nextbox">
<div class="pill">💊</div><div><strong id="prox-nome">Aguardando horários</strong>
<div class="muted" id="prox-gaveta">Configure uma das gavetas</div><div id="prox-hora"></div></div></div>
<div class="muted" style="margin-top:12px">Faltam</div><div class="counter" id="contagem">--:--:--</div>
</section></div><div id="gavetas" class="grid">)MBUI");
  const uint32_t diaAtual=horarioOK ? horarioLocal/86400UL : 0UL;
  const uint16_t minutoAtual=horarioOK ? (horarioLocal%86400UL)/60UL : 0;
  for (uint8_t i=0;i<3;i++) {
    const Gaveta &g=gavetas[i];
    const bool cadastrada=(g.hora<24 && g.remedio.length()>0);
    const int posConfirmacao=cadastrada && horarioOK ?
                           localizarConfirmacaoHoje(i,diaAtual) : -1;
    const bool confirmada=posConfirmacao>=0;
    const bool disponivel=cadastrada && horarioOK && !confirmada &&
                           minutoAtual>=g.hora*60+g.minuto;
    html+=F("<section class='card drawer dose' data-min='");
    html+=String(cadastrada ? (g.hora*60+g.minuto) : -1);
    html+=F("' data-nome='");
    html+=escapeHtml(g.remedio);
    html+=F("' data-gaveta='");
    html+=String(i+1);
    html+=F("' data-confirmada='");
    html+=confirmada ? F("1") : F("0");
    html+=F("'><div class='drawer-head'>▣ Gaveta ");
    html+=String(i+1);
    html+=F("</div><div class='drug'><span>💊</span>");
    html+=cadastrada ? escapeHtml(g.remedio) : F("Nenhum medicamento");
    html+=F("</div><div class='time-label'>Horário:</div><div class='timetext'>");
    if(cadastrada){
      if(g.hora<10) html+='0';
      html+=String(g.hora);
      html+=':';
      if(g.minuto<10) html+='0';
      html+=String(g.minuto);
    } else html+=F("--:--");
    html+=F("</div><div class='tag'>");
    html+=cadastrada ? F("◷ Horário agendado") : F("Configure o horário para ativar o LED");
    html+=F("</div><div class='confirmstatus");
    if(confirmada)html+=F(" ok");
    else if(disponivel)html+=F(" due");
    html+=F("' aria-live='polite'>");
    if(!cadastrada)html+=F("Nenhum medicamento cadastrado");
    else if(confirmada)html+=F("✓ Confirmada pelo usuario hoje");
    else if(!horarioOK)html+=F("Aguardando relogio NTP");
    else if(disponivel)html+=F("Horario passou: aguardando confirmacao");
    else html+=F("Agendada: ainda nao confirmada");
    html+=F("</div>");
    if(cadastrada && !confirmada){
      html+=F("<form class='confirmform' method='post' action='/confirmar-dose'><input type='hidden' name='csrf' value='");
      html+=csrfToken;
      html+=F("'><input type='hidden' name='index' value='");
      html+=String(i);
      html+=F("'><input type='hidden' name='dia' value='");
      html+=String(diaAtual);
      html+=F("'><input type='hidden' name='horario' value='");
      html+=String(g.hora*60+g.minuto);
      html+=F("'><button type='submit' class='confirmbtn'");
      if(!disponivel)html+=F(" disabled");
      html+=F(">✓ Confirmar dose tomada</button></form>");
    }
    if(confirmada){
      const ConfirmacaoDose &reg=confirmacoes[posConfirmacao];
      html+=F("<form class='undoform' method='post' action='/desfazer-confirmacao'><input type='hidden' name='csrf' value='");
      html+=csrfToken;
      html+=F("'><input type='hidden' name='registro' value='");
      html+=String(posConfirmacao);
      html+=F("'><input type='hidden' name='instante' value='");
      html+=String(reg.instanteLocal);
      html+=F("'><input type='hidden' name='gaveta' value='");
      html+=String(reg.gaveta);
      html+=F("'><input type='hidden' name='horario' value='");
      html+=String(reg.minutoProgramado);
      html+=F("'><button type='submit' class='undobtn'>↶ Desfazer confirmação</button></form>");
    }
    html+=F("<div class='buttons'><button type='button' class='blue edit'>✎ Editar</button>");
    html+=F("<form action='/limpar' method='post'><input type='hidden' name='csrf' value='");
    html+=csrfToken;
    html+=F("'><input type='hidden' name='index' value='");
    html+=String(i);
    html+=F("'><button class='red' style='width:100%' type='submit'>▤ Limpar</button></form></div>");
    html+=F("<form class='editor' action='/salvar' method='post' hidden><input type='hidden' name='csrf' value='");
    html+=csrfToken;
    html+=F("'><input type='hidden' name='index' value='");
    html+=String(i);
    html+=F("'><label>Medicamento</label><input name='remedio' maxlength='40' required value='");
    html+=escapeHtml(g.remedio);
    html+=F("'><div class='two'><div><label>Hora</label><input type='number' name='hora' min='0' max='23' required value='");
    if(cadastrada) html+=String(g.hora);
    html+=F("'></div><div><label>Minuto</label><input type='number' name='minuto' min='0' max='59' required value='");
    if(cadastrada) html+=String(g.minuto);
    html+=F("'></div></div><button class='blue' type='submit'>Salvar horário</button></form></section>");
  }
  html+=F(R"MBUI(</div><div class="bottom" id="horarios"><section class="card" id="alertas">
<h2>♧ Controle de LEDs</h2><p class="muted">Cada gaveta pisca durante 1 minuto no horário programado.</p>
<div class="buttons">
<form action="/silenciar" method="post"><input type="hidden" name="csrf" value=")MBUI");
  html+=csrfToken;
  html+=F(R"MBUI("><button class="red" type="submit" style="width:100%">Silenciar LEDs atuais</button></form>
<form action="/testar-leds" method="post"><input type="hidden" name="csrf" value=")MBUI");
  html+=csrfToken;
  html+=F(R"MBUI("><button class="outline" type="submit" style="width:100%">Testar LEDs</button></form>
</div><p class="hint">O teste pisca os três LEDs, um por vez, por 3 segundos cada. Sem som. O teste não confirma doses.</p>
<p class="hint" id="teste-estado" aria-live="polite"></p></section>
<section class="card" id="sobre"><h2>ⓘ Dica</h2>
<div class="tip">Os nomes e horários são guardados na placa. O MedBox utiliza somente sinalização luminosa por LEDs, sem alarme sonoro.</div>
<form action="/sair" method="post"><input type="hidden" name="csrf" value=")MBUI");
  html+=csrfToken;
  html+=F(R"MBUI("><button class="logout" type="submit">Sair da conta</button></form></section></div><section class="card" id="historico"><h2 style="font-size:16px;color:#1a548f;margin:0 0 10px">✓ Histórico de confirmações</h2>
<p class="muted">Registros manuais feitos por um responsável. Não comprovam ingestão.</p>)MBUI");
  if(totalConfirmacoes==0) {
    html+=F("<p class='muted'>Nenhuma dose confirmada ainda.</p>");
  } else {
    const int inicio=totalConfirmacoes>8 ? totalConfirmacoes-8 : 0;
    for(int i=totalConfirmacoes-1;i>=inicio;i--){
      const ConfirmacaoDose &reg=confirmacoes[i];
      time_t instante=(time_t)reg.instanteLocal;
      struct tm horaRegistro;
      gmtime_r(&instante,&horaRegistro); // instante ja deslocado para UTC-3
      char carimbo[22];
      snprintf(carimbo,sizeof(carimbo),"%02d/%02d/%04d %02d:%02d",
        horaRegistro.tm_mday,horaRegistro.tm_mon+1,horaRegistro.tm_year+1900,
        horaRegistro.tm_hour,horaRegistro.tm_min);
      html+=F("<div class='history-row'><div><b>✓ Gaveta ");
      html+=String(reg.gaveta+1);
      html+=F("</b><div>");
      html+=escapeHtml(reg.medicamento);
      html+=F("</div></div><div class='history-undo'><small>");
      html+=String(carimbo);
      html+=F("</small><form class='undoform' method='post' action='/desfazer-confirmacao'><input type='hidden' name='csrf' value='");
      html+=csrfToken;
      html+=F("'><input type='hidden' name='registro' value='");
      html+=String(i);
      html+=F("'><input type='hidden' name='instante' value='");
      html+=String(reg.instanteLocal);
      html+=F("'><input type='hidden' name='gaveta' value='");
      html+=String(reg.gaveta);
      html+=F("'><input type='hidden' name='horario' value='");
      html+=String(reg.minutoProgramado);
      html+=F("'><button class='undobtn' type='submit'>↶ Desfazer</button></form></div></div>");
    }
  }
  html+=F(R"MBUI(</section>
<footer>Protótipo educacional: mantenha também outro lembrete confiável para seus medicamentos.</footer></main></div>
<script>
(function(){
 var rel=document.getElementById('relogio'), data=document.getElementById('data');
 var inicio=Number(rel.getAttribute('data-local')), carga=Date.now();
 var nomes=['domingo','segunda-feira','terça-feira','quarta-feira','quinta-feira','sexta-feira','sábado'];
 var meses=['janeiro','fevereiro','março','abril','maio','junho','julho','agosto','setembro','outubro','novembro','dezembro'];
 var cards=Array.prototype.slice.call(document.querySelectorAll('.dose'));
 function dois(x){return String(x).padStart(2,'0')}
 function atualizar(){
  if(!inicio){rel.textContent='--:--:--';data.textContent='Horário ainda não sincronizado';return}
  // O NTPClient do firmware já aplica UTC-3: formatar sempre em UTC no navegador.
  var local=inicio+Math.floor((Date.now()-carga)/1000),d=new Date(local*1000);
  var h=d.getUTCHours(),m=d.getUTCMinutes(),s=d.getUTCSeconds();
  if(Math.floor(local/86400)!==Number(rel.dataset.dia)){location.reload();return}
  rel.textContent=dois(h)+':'+dois(m)+':'+dois(s);
  data.textContent=nomes[d.getUTCDay()]+', '+d.getUTCDate()+' de '+meses[d.getUTCMonth()]+' de '+d.getUTCFullYear();
  var agora=h*3600+m*60+s,proximo=null;
  cards.forEach(function(c){
   var minuto=Number(c.dataset.min);
   if(minuto<0)return;
   var delta=minuto*60-agora;
   if(delta<0 || c.dataset.confirmada==='1')delta+=86400;
   if(proximo===null||delta<proximo.delta)proximo={delta:delta,card:c,minuto:minuto};
  });
  cards.forEach(function(c){
   var status=c.querySelector('.confirmstatus'),btn=c.querySelector('.confirmbtn');
   if(!status||Number(c.dataset.min)<0||c.dataset.confirmada==='1')return;
   var vencida=agora>=Number(c.dataset.min)*60;
   status.textContent=vencida?'Horario passou: aguardando confirmacao':'Agendada: ainda nao confirmada';
   status.className='confirmstatus'+(vencida?' due':'');
   if(btn)btn.disabled=!vencida;
  });
  if(proximo){
   document.getElementById('prox-nome').textContent=proximo.card.dataset.nome;
   document.getElementById('prox-gaveta').textContent='Gaveta '+proximo.card.dataset.gaveta;
   document.getElementById('prox-hora').textContent='Às '+dois(Math.floor(proximo.minuto/60))+':'+dois(proximo.minuto%60);
   var v=proximo.delta;
   document.getElementById('contagem').textContent=dois(Math.floor(v/3600))+':'+dois(Math.floor(v%3600/60))+':'+dois(v%60);
  }else{
   document.getElementById('prox-nome').textContent='Nenhum medicamento';
   document.getElementById('prox-gaveta').textContent='Cadastre um horário';
   document.getElementById('prox-hora').textContent='';
   document.getElementById('contagem').textContent='--:--:--';
  }
 }
 atualizar();setInterval(atualizar,1000);
 document.querySelectorAll('.confirmform').forEach(function(form){
  form.addEventListener('submit',function(e){
   if(!window.confirm('Confirma que esta dose foi tomada? O registro sera manual.'))e.preventDefault();
  });
 });
 document.querySelectorAll('.undoform').forEach(function(form){
  form.addEventListener('submit',function(e){
   if(!window.confirm('Desfazer esta confirmacao? A dose voltara a ficar pendente. Os LEDs nao serao religados.'))e.preventDefault();
  });
 });
 document.querySelectorAll('.edit').forEach(function(b){
  b.addEventListener('click',function(){
   var form=b.closest('.drawer').querySelector('.editor');
   form.hidden=!form.hidden;if(!form.hidden)form.querySelector('input[name=remedio]').focus();
  });
 });
 if(new URLSearchParams(location.search).get('teste')==='1'){
  document.getElementById('teste-estado').textContent='Teste dos LEDs iniciado. Observe as três gavetas por 9 segundos.';
 }
})();
</script></body></html>)MBUI");
  cabecalhosSemCache();
  server.send(200,"text/html; charset=utf-8",html);
}

void salvar() {
  if (!autorizado() || !csrfValido()) return;
  String idx=server.arg("index"), h=server.arg("hora"), m=server.arg("minuto");
  String nome=server.arg("remedio");
  nome.trim();
  if(!inteiroValido(idx,0,2) || !inteiroValido(h,0,23) ||
     !inteiroValido(m,0,59) || nome.length()==0 || nome.length()>40 || nome.indexOf('|')>=0 ||
     nome.indexOf('\n')>=0 || nome.indexOf('\r')>=0) {
    server.send(400,"text/plain; charset=utf-8","Dados inválidos");
    return;
  }
  int i=idx.toInt();
  gavetas[i].remedio=nome;
  gavetas[i].hora=h.toInt();
  gavetas[i].minuto=m.toInt();
  gavetas[i].ultimoDia=-1;
  gavetas[i].alertaLedAtivo = false;
  salvarConfiguracao();
  atualizarOled(true); // Imediato, sem regravar o firmware ao editar pelo site.
  server.sendHeader("Location","/",true);
  server.send(303,"text/plain","");
}
void iniciarServicosWeb() {
  // Rotas registradas uma unica vez no setup; reabre o servidor ao reconectar.
  server.begin();
  mdnsAtivo = MDNS.begin("medbox");
  if (mdnsAtivo) {
    MDNS.addService("http", "tcp", 80);
    Serial.println(F("mDNS: http://medbox.local"));
  } else {
    Serial.println(F("mDNS indisponivel; utilize o IP local"));
  }
  Serial.print(F("MedBox acessivel em http://"));
  Serial.println(WiFi.localIP());
  tela("Sistema pronto", "Abra no navegador", WiFi.localIP().toString());
}

// Adaptador de rotas: evita conversoes automaticas de funcoes para
// std::function no compilador integrado ao ArduinoDroid.
// O ESP8266WebServer continua sendo usado para processar requisicoes e POSTs.
class MedBoxRouteHandler : public RequestHandler {
 public:
  bool canHandle(HTTPMethod method, const String &uri) override {
    (void)method;
    (void)uri;
    return true; // Abrange tambem 404 sem depender de onNotFound().
  }
  bool handle(ESP8266WebServer &http, HTTPMethod method,
              const String &uri) override {
    (void)http;
    if (method == HTTP_GET && uri == "/") raiz();
    else if (method == HTTP_GET && uri == "/login") mostrarLogin();
    else if (method == HTTP_POST && uri == "/login") enviarLogin();
    else if (method == HTTP_POST && uri == "/sair") sair();
    else if (method == HTTP_POST && uri == "/salvar") salvar();
    else if (method == HTTP_POST && uri == "/limpar") limparGaveta();
    else if (method == HTTP_POST && uri == "/silenciar") silenciar();
    else if (method == HTTP_POST && uri == "/testar-leds") testarLeds();
    else if (method == HTTP_POST && uri == "/confirmar-dose") confirmarDose();
    else if (method == HTTP_POST && uri == "/desfazer-confirmacao") desfazerConfirmacao();
    else {
      if (!autorizado()) return true;
      cabecalhosSemCache();
      server.send(404, "text/plain; charset=utf-8", "Pagina nao encontrada");
    }
    return true;
  }
};

void setup() {
  Serial.begin(115200);
  for (uint8_t pin : LED_PINS) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  armazenamentoDisponivel=LittleFS.begin();
  if(armazenamentoDisponivel){carregarConfiguracao();carregarConfirmacoes();}
  else Serial.println(F("Falha LittleFS: historico e configuracoes indisponiveis"));
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  Wire.beginTransmission(OLED_ENDERECO);
  oledDisponivel = Wire.endTransmission() == 0 &&
                   display.begin(SSD1306_SWITCHCAPVCC, OLED_ENDERECO, false, false);
  if (!oledDisponivel) {
    Serial.println(F("OLED indisponivel: confira SDA, SCL e endereco I2C"));
  }
  tela("Conectando Wi-Fi...","","");
  server.collectHeaders("Cookie");
  server.addHandler(new MedBoxRouteHandler());
  timeClient.begin();
  WiFi.mode(WIFI_STA);
  WiFi.begin(MEDBOX_WIFI_SSID,MEDBOX_WIFI_PASSWORD);
  unsigned long inicio=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-inicio<20000UL) {
    delay(250);
    yield();
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiEstavaConectado = true;
    iniciarServicosWeb();
    horaSincronizada = timeClient.forceUpdate();
    atualizarOled(true);
  } else {
    // Continua procurando a rede no loop e inicia o painel quando ela voltar.
    tela("Wi-Fi indisponivel", "Tentando reconectar", "rede 2.4 GHz");
    Serial.println(F("Wi-Fi indisponivel; aguardando reconexao"));
  }
}
void loop() {
  atualizarLeds();
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiEstavaConectado) {
      wifiEstavaConectado = false;
      mdnsAtivo = false;
      Serial.println(F("Wi-Fi caiu; tentando reconectar"));
    }
    if (millis() - ultimaTentativaWiFi > 10000UL) {
      ultimaTentativaWiFi = millis();
      WiFi.reconnect();
    }
  } else {
    if (!wifiEstavaConectado) {
      wifiEstavaConectado = true;
      iniciarServicosWeb();
      if (timeClient.forceUpdate()) horaSincronizada = true;
    }
    server.handleClient();
    if (mdnsAtivo) MDNS.update();
    if (timeClient.update()) horaSincronizada = true;
    if (!horaSincronizada && millis() - ultimaTentativaNTP > 15000UL) {
      ultimaTentativaNTP = millis();
      horaSincronizada = timeClient.forceUpdate();
    }
  }
  // Depois de sincronizado ao menos uma vez, o relogio segue por millis()
  // durante quedas temporarias de Wi-Fi. Nao substitui um RTC dedicado.
  if(horaSincronizada) {
    unsigned long segundos=timeClient.getEpochTime();
    long dia=(long)(segundos/86400UL);
    int h=timeClient.getHours(), m=timeClient.getMinutes();
    for(int i=0;i<3;i++) {
      if(h==gavetas[i].hora && m==gavetas[i].minuto &&
         gavetas[i].ultimoDia!=dia && !confirmadaHoje(i,(uint32_t)dia)) {
        gavetas[i].ultimoDia=dia;
        testeLedsAtivo=false; // Um lembrete verdadeiro tem prioridade sobre um teste.
        gavetas[i].inicioAlertaLed = millis();
        gavetas[i].alertaLedAtivo = true;
      }
    }
  }
  atualizarOled(false); // Normal, alerta ou retorno automatico apos 60s.
  delay(100);
}
