#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
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
const uint8_t BUZZER_PIN = 14; // D5
const uint8_t LED_PIN = 2;     // D4, ativo em LOW
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ESP8266WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "a.st1.ntp.br", -10800, 60000);
bool horaSincronizada = false;
bool alarmeSilenciado = false;
unsigned long ultimaTentativaWiFi = 0;
unsigned long ultimaTentativaNTP = 0;
unsigned long ultimoDiaPersistido = 0;

struct Gaveta {
  uint8_t hora, minuto;
  String remedio;
  long ultimoDia; // evita repetição no mesmo dia durante execução
};
Gaveta gavetas[3] = {
  {8, 0, "Exemplo 1", -1},
  {12, 0, "Exemplo 2", -1},
  {20, 0, "Exemplo 3", -1}
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
void silenciar() {
  if (!autorizado()) return;
  alarmeSilenciado=true;
  noTone(BUZZER_PIN);
  digitalWrite(LED_PIN,HIGH);
  server.sendHeader("Location","/",true);
  server.send(303,"text/plain","");
}
void tela(const String &a, const String &b, const String &c) {
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
bool autorizado() {
  if (server.authenticate(MEDBOX_ADMIN_USER, MEDBOX_ADMIN_PASSWORD)) return true;
  server.requestAuthentication();
  return false;
}
void buzzer() {
  if (alarmeSilenciado) return;
  for(int i=0;i<3;i++) {
    tone(BUZZER_PIN,1000,200);
    digitalWrite(LED_PIN,LOW);
    delay(250);
    digitalWrite(LED_PIN,HIGH);
    delay(100);
    yield();
  }
  noTone(BUZZER_PIN);
}
bool inteiroValido(const String &s, int minV, int maxV) {
  if(s.length()==0 || s.length()>2) return false;
  for(unsigned int i=0;i<s.length();i++) if(s[i]<'0'||s[i]>'9') return false;
  int v=s.toInt();
  return v>=minV && v<=maxV;
}
void raiz() {
  if(!autorizado()) return;
  String html;
  html.reserve(5800);
  html+=F(R"MEDBOX(<!doctype html><html lang="pt-BR"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#1267c8"><title>MedBox Inteligente</title>
<style>
*{box-sizing:border-box}html{-webkit-text-size-adjust:100%}
body{margin:0;background:#f1f5f9;color:#152238;font:16px/1.5 system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}
header{background:#1267c8;color:white;padding:24px max(18px,env(safe-area-inset-right)) 24px max(18px,env(safe-area-inset-left))}
header .wrap{max-width:1000px;margin:auto}h1{font-size:clamp(1.5rem,5vw,2rem);margin:0}header p{margin:4px 0 0;opacity:.9}
main{width:100%;max-width:1000px;margin:0 auto;padding:18px max(14px,env(safe-area-inset-right)) calc(24px + env(safe-area-inset-bottom)) max(14px,env(safe-area-inset-left))}
.grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:16px}
section{min-width:0;background:#fff;border:1px solid #e2e8f0;border-radius:16px;padding:18px;box-shadow:0 3px 12px #0f172a0a}
h2{font-size:1.15rem;margin:0 0 16px;color:#1254a3}
label{display:block;font-weight:600;font-size:.93rem;margin:12px 0 5px}
input{display:block;width:100%;min-width:0;border:1px solid #b7c5d6;border-radius:10px;padding:12px;font:inherit;font-size:16px;background:#fff;color:#152238}
input:focus{outline:2px solid #2581eb;outline-offset:1px}
.time{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}
button{display:block;width:100%;min-height:48px;border:0;border-radius:10px;background:#1267c8;color:#fff;font:600 16px system-ui;cursor:pointer;margin-top:18px;padding:12px}
button:active{filter:brightness(.9)}.mute{background:#b42332}.actions{margin-top:16px}
footer{color:#526174;text-align:center;font-size:.87rem;margin:22px 0 0}
@media(max-width:740px){.grid{grid-template-columns:1fr}main{padding-top:14px}section{padding:16px}header{padding-top:20px;padding-bottom:20px}}
@media(min-width:741px) and (max-width:950px){.grid{grid-template-columns:repeat(2,minmax(0,1fr))}}
</style></head><body><header><div class="wrap"><h1>💊 MedBox Inteligente</h1><p>Configure seus medicamentos e horários</p></div></header>
<main><div class="grid">)MEDBOX");
  for(int i=0;i<3;i++) {
    html+=F("<section><h2>Gaveta "); html+=String(i+1);
    html+=F("</h2><form action='/salvar' method='post'><input type='hidden' name='index' value='");
    html+=String(i);
    html+=F("'><label for='rem");html+=String(i);html+=F("'>Medicamento</label><input id='rem");html+=String(i);
    html+=F("' name='remedio' maxlength='40' required autocomplete='off' value='");
    html+=escapeHtml(gavetas[i].remedio);
    html+=F("'><div class='time'><div><label>Hora<input type='number' inputmode='numeric' name='hora' min='0' max='23' required value='");
    html+=String(gavetas[i].hora);
    html+=F("'></label></div><div><label>Minuto<input type='number' inputmode='numeric' name='minuto' min='0' max='59' required value='");
    html+=String(gavetas[i].minuto);
    html+=F("'></label></div></div><button type='submit'>Salvar gaveta ");
    html+=String(i+1);html+=F("</button></form></section>");
  }
  html+=F(R"MEDBOX(</div><section class="actions"><h2>Controle de alarmes</h2><form action="/silenciar" method="post"><button class="mute" type="submit">Silenciar alarmes de hoje</button></form></section>
<footer>Protótipo educacional. Use também outro lembrete confiável.</footer></main></body></html>)MEDBOX");
  server.send(200,"text/html; charset=utf-8",html);
}

void salvar() {
  if(!autorizado()) return;
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
  salvarConfiguracao();
  tela("Gaveta "+String(i+1)+" salva",nome,h+":"+m);
  server.sendHeader("Location","/",true);
  server.send(303,"text/plain","");
}
void setup() {
  Serial.begin(115200);
  if (LittleFS.begin()) carregarConfiguracao();
  else Serial.println(F("Falha LittleFS: configuracoes nao serao persistidas"));
  pinMode(BUZZER_PIN,OUTPUT);
  pinMode(LED_PIN,OUTPUT);
  digitalWrite(LED_PIN,HIGH);
  Wire.begin(4,5); // CONFIRMAR pinos no modelo real de OLED integrado
  if(!display.begin(SSD1306_SWITCHCAPVCC,0x3C)) {
    Serial.println(F("OLED indisponivel"));
    // Permite continuar usando o painel mesmo sem OLED.
  }
  tela("Conectando Wi-Fi...","","");
  WiFi.mode(WIFI_STA);
  WiFi.begin(MEDBOX_WIFI_SSID,MEDBOX_WIFI_PASSWORD);
  unsigned long inicio=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-inicio<20000UL) {
    delay(250);
    yield();
  }
  if(WiFi.status()==WL_CONNECTED) {
    timeClient.begin();
    horaSincronizada=timeClient.forceUpdate();
    server.on("/",HTTP_GET,raiz);
    server.on("/salvar",HTTP_POST,salvar);
    server.on("/silenciar",HTTP_POST,silenciar);
    server.begin();
    tela("Sistema pronto","Abra no navegador",WiFi.localIP().toString());
    Serial.println(WiFi.localIP());
  } else {
    tela("Wi-Fi indisponivel","Reinicie e confira","rede 2.4 GHz");
  }
}
void loop() {
  if(WiFi.status()!=WL_CONNECTED) {
    if(millis()-ultimaTentativaWiFi>10000UL) {
      ultimaTentativaWiFi=millis();
      WiFi.reconnect();
    }
    delay(100);
    return;
  }

  server.handleClient();
  if(timeClient.update()) horaSincronizada=true;
  if(!horaSincronizada && millis()-ultimaTentativaNTP>15000UL) {
    ultimaTentativaNTP=millis();
    horaSincronizada=timeClient.forceUpdate();
  }
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
        buzzer();
      }
    }
  }
  delay(200);
}
