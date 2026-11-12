#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <LittleFS.h>
#include "secrets.h"

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
// Apenas buzzer ATIVO de 3,3 V e corrente compativel; veja docs/instalacao.md.
// Deixe desabilitado enquanto nao houver um circuito eletrico validado.
constexpr bool HABILITAR_BUZZER_ATIVO = false;
constexpr uint8_t BUZZER_PIN = 16; // D0/GPIO16, liga/desliga sem PWM
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
bool alarmeSilenciado = false;
unsigned long ultimaTentativaWiFi = 0;
unsigned long ultimaTentativaNTP = 0;
unsigned long ultimoDiaPersistido = 0;

struct Gaveta {
  uint8_t hora, minuto;
  String remedio;
  long ultimoDia; // evita repetição no mesmo dia durante execução
  unsigned long inicioAlertaLed;
  bool alertaLedAtivo;
};
Gaveta gavetas[3] = {
  {8, 0, "Exemplo 1", -1, 0, false},
  {12, 0, "Exemplo 2", -1, 0, false},
  {20, 0, "Exemplo 3", -1, 0, false}
};

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
    if (h>=0 && h<24 && m>=0 && m<60 && nome.length()>0 && nome.length()<=40) {
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

void paginaLogin(bool invalido = false, bool bloqueado = false) {
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
  paginaLogin();
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
  bool algumAlarmeAtivo = false;
  for (int i = 0; i < 3; i++) {
    Gaveta &gaveta = gavetas[i];
    const unsigned long decorrido = agora - gaveta.inicioAlertaLed;
    if (alarmeSilenciado || decorrido >= DURACAO_ALERTA_LED) {
      gaveta.alertaLedAtivo = false;
    }
    const bool aceso = gaveta.alertaLedAtivo && (decorrido / INTERVALO_LED) % 2 == 0;
    digitalWrite(LED_PINS[i], aceso ? HIGH : LOW);
    if (gaveta.alertaLedAtivo) algumAlarmeAtivo = true;
  }
  if (HABILITAR_BUZZER_ATIVO) {
    // Buzzer ativo: pulsos simples de 500 ms, sem tone() nem PWM no GPIO16.
    const bool tocar = algumAlarmeAtivo && !alarmeSilenciado &&
                        ((agora / INTERVALO_LED) % 2 == 0);
    digitalWrite(BUZZER_PIN, tocar ? HIGH : LOW);
  }
}
void silenciar() {
  if (!autorizado() || !csrfValido()) return;
  alarmeSilenciado = true;
  atualizarLeds();
  server.sendHeader("Location","/",true);
  server.send(303,"text/plain","");
}
bool inteiroValido(const String &s, int minV, int maxV) {
  if(s.length()==0 || s.length()>2) return false;
  for(unsigned int i=0;i<s.length();i++) if(s[i]<'0'||s[i]>'9') return false;
  int v=s.toInt();
  return v>=minV && v<=maxV;
}
void raiz() {
  if (!autorizado()) return;
  String html;
  html.reserve(6200);
  html+=F(R"MEDBOX(<!doctype html><html lang="pt-BR"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#145bd7"><title>MedBox | Painel</title>
<style>
*{box-sizing:border-box}html{-webkit-text-size-adjust:100%}
body{margin:0;background:#f5f7fb;color:#17233b;font:15px/1.5 system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}
header{background:#fff;border-bottom:1px solid #e5eaf3}
.wrap{max-width:1020px;margin:auto;padding:0 20px}
.top{display:flex;align-items:center;justify-content:space-between;gap:16px;padding-top:19px;padding-bottom:19px}
.brand{display:flex;align-items:center;gap:12px}
.symbol{display:grid;place-items:center;flex:none;width:43px;height:43px;border-radius:13px;background:#145bd7;color:#fff;font-size:27px;font-weight:800}
.brand-name{font-weight:800;font-size:21px;letter-spacing:-.6px;line-height:1.15}
.brand-caption{font-size:12px;color:#6b7890;margin-top:3px}
.secure{font-size:12px;color:#395579;background:#edf4ff;border:1px solid #dbe8ff;padding:7px 11px;border-radius:99px;white-space:nowrap}
main{max-width:1020px;margin:0 auto;padding:32px 20px 22px}
.lead{margin-bottom:20px}
h1{font-size:clamp(21px,3.5vw,26px);letter-spacing:-.7px;margin:0;color:#142643}
.lead p{color:#64748b;margin:5px 0 0;font-size:14px}
.grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:14px}
section.card{min-width:0;background:#fff;border:1px solid #e3e8f1;border-radius:15px;padding:19px;box-shadow:0 5px 18px #17294a08}
.card-title{display:flex;align-items:center;gap:9px;margin-bottom:17px}
.dot{display:inline-block;width:10px;height:10px;border-radius:50%;background:#1768e6}
.gaveta-2 .dot{background:#e44756}.gaveta-3 .dot{background:#24a477}
h2{font-size:16px;font-weight:750;margin:0;color:#162743}
.card-title small{margin-left:auto;color:#76849a;font-size:12px;font-weight:500}
label{display:block;font-size:13px;font-weight:650;margin:11px 0 5px;color:#394a62}
input{display:block;width:100%;min-width:0;background:#fff;border:1px solid #ccd5e3;border-radius:9px;padding:10px 11px;font:inherit;font-size:16px;color:#192940}
input:focus{outline:2px solid #a4c6ff;outline-offset:1px;border-color:#3c7cf5}
.time{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}
button{display:block;width:100%;min-height:45px;border:0;border-radius:9px;background:#145bd7;color:#fff;font:650 14px system-ui;cursor:pointer;margin-top:17px;padding:11px}
button:focus-visible{outline:3px solid #9bc4ff;outline-offset:2px}
button:active{filter:brightness(.92)}
.actions{margin-top:16px;display:flex;align-items:center;justify-content:space-between;gap:20px}
.actions h2{font-size:15px}
.actions p{font-size:13px;color:#687991;margin:5px 0 0}
.actions form{width:230px;flex:none}
.actions button{margin:0;background:#fff;color:#a52e40;border:1px solid #e9bfc8}
.actions button:hover{background:#fff2f4}
.logout{max-width:180px;margin:18px 0 0 auto}
.logout button{background:transparent;border:1px solid #d9e1ee;color:#455875;margin:0;font-size:13px;min-height:39px}
footer{font-size:12px;color:#75839a;text-align:center;margin:23px 0 0}
@media(max-width:800px){.grid{grid-template-columns:1fr}.actions{align-items:stretch;flex-direction:column}.actions form{width:100%}main{padding:24px 16px 20px}.wrap{padding:0 16px}}
@media(max-width:380px){.secure{font-size:0;padding:7px 10px}.secure:before{content:"🔒";font-size:15px}}
</style></head><body>
<header><div class="wrap top"><div class="brand"><span class="symbol" aria-hidden="true">+</span><div><div class="brand-name">MedBox</div><div class="brand-caption">Painel de medicamentos</div></div></div><div class="secure">🔒 Acesso protegido</div></div></header>
<main><div class="lead"><h1>Suas gavetas</h1><p>Defina o medicamento e o horário de cada gaveta.</p></div>
<div class="grid">)MEDBOX");
  for (int i = 0; i < 3; i++) {
    html+=F("<section class='card gaveta-"); html+=String(i+1);
    html+=F("'><div class='card-title'><span class='dot' aria-hidden='true'></span><h2>Gaveta ");
    html+=String(i+1);
    html+=F("</h2><small>Alerta 1 min</small></div><form action='/salvar' method='post'><input type='hidden' name='csrf' value='");
    html+=csrfToken;
    html+=F("'><input type='hidden' name='index' value='");
    html+=String(i);
    html+=F("'><label for='rem");html+=String(i);html+=F("'>Medicamento</label><input id='rem");html+=String(i);
    html+=F("' name='remedio' maxlength='40' required autocomplete='off' value='");
    html+=escapeHtml(gavetas[i].remedio);
    html+=F("'><div class='time'><div><label for='hora");html+=String(i);
    html+=F("'>Hora</label><input id='hora");html+=String(i);
    html+=F("' type='number' inputmode='numeric' name='hora' min='0' max='23' required value='");
    html+=String(gavetas[i].hora);
    html+=F("'></div><div><label for='minuto");html+=String(i);
    html+=F("'>Minuto</label><input id='minuto");html+=String(i);
    html+=F("' type='number' inputmode='numeric' name='minuto' min='0' max='59' required value='");
    html+=String(gavetas[i].minuto);
    html+=F("'></div></div><button type='submit'>Salvar horário</button></form></section>");
  }
  html+=F(R"MEDBOX(</div><section class="card actions"><div><h2>Controle de alarmes</h2><p>O LED de cada gaveta pisca durante 1 minuto no horário definido.</p></div>
<form action="/silenciar" method="post"><input type="hidden" name="csrf" value=")MEDBOX");
  html+=csrfToken;
  html+=F(R"MEDBOX("><button type="submit">Silenciar alarmes de hoje</button></form></section>
<form class="logout" action="/sair" method="post"><input type="hidden" name="csrf" value=")MEDBOX");
  html+=csrfToken;
  html+=F(R"MEDBOX("><button type="submit">Sair da conta</button></form>
<footer>Protótipo educacional. Mantenha também outro lembrete de medicamentos.</footer></main></body></html>)MEDBOX");
  cabecalhosSemCache();
  server.send(200,"text/html; charset=utf-8",html);
}

void salvar() {
  if (!autorizado() || !csrfValido()) return;
  String idx=server.arg("index"), h=server.arg("hora"), m=server.arg("minuto");
  String nome=server.arg("remedio");
  nome.trim();
  if(!inteiroValido(idx,0,2) || !inteiroValido(h,0,23) ||
     !inteiroValido(m,0,59) || nome.length()==0 || nome.length()>40) {
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
  tela("Gaveta "+String(i+1)+" salva",nome,h+":"+m);
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

void setup() {
  Serial.begin(115200);
  for (uint8_t pin : LED_PINS) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  if (HABILITAR_BUZZER_ATIVO) {
    digitalWrite(BUZZER_PIN, LOW);
    pinMode(BUZZER_PIN, OUTPUT);
  }
  if (LittleFS.begin()) carregarConfiguracao();
  else Serial.println(F("Falha LittleFS: configuracoes nao serao persistidas"));
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  Wire.beginTransmission(OLED_ENDERECO);
  oledDisponivel = Wire.endTransmission() == 0 &&
                   display.begin(SSD1306_SWITCHCAPVCC, OLED_ENDERECO, false, false);
  if (!oledDisponivel) {
    Serial.println(F("OLED indisponivel: confira SDA, SCL e endereco I2C"));
  }
  tela("Conectando Wi-Fi...","","");
  server.collectHeaders("Cookie");
  server.on("/", HTTP_GET, raiz);
  server.on("/login", HTTP_GET, mostrarLogin);
  server.on("/login", HTTP_POST, enviarLogin);
  server.on("/sair", HTTP_POST, sair);
  server.on("/salvar", HTTP_POST, salvar);
  server.on("/silenciar", HTTP_POST, silenciar);
  server.onNotFound([]() {
    if (!autorizado()) return;
    cabecalhosSemCache();
    server.send(404, "text/plain; charset=utf-8", "Pagina nao encontrada");
  });
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
    if ((unsigned long)dia!=ultimoDiaPersistido) {
      ultimoDiaPersistido=(unsigned long)dia;
      alarmeSilenciado=false;
    }
    int h=timeClient.getHours(), m=timeClient.getMinutes();
    for(int i=0;i<3;i++) {
      if(h==gavetas[i].hora && m==gavetas[i].minuto && gavetas[i].ultimoDia!=dia) {
        gavetas[i].ultimoDia=dia;
        tela("HORA DO REMEDIO!","Gaveta "+String(i+1),gavetas[i].remedio);
        if (!alarmeSilenciado) {
          gavetas[i].inicioAlertaLed = millis();
          gavetas[i].alertaLedAtivo = true;
        }
      }
    }
  }
  delay(100);
}
