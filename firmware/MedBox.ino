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
  String html=F("<!doctype html><html lang='pt-BR'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>MedBox</title><style>body{font:16px system-ui;background:#f1f5f9;color:#0f172a;max-width:450px;margin:25px auto;padding:12px}section{background:white;padding:18px;margin:12px 0;border-radius:12px}input,button{display:block;width:100%;box-sizing:border-box;padding:10px;margin:8px 0}button{background:#0284c7;color:white;border:0;border-radius:6px}</style></head><body><h1>💊 MedBox Inteligente</h1>");
  for(int i=0;i<3;i++) {
    html+=F("<section><h2>Gaveta "); html+=String(i+1);
    html+=F("</h2><form action='/salvar' method='post'><input type='hidden' name='index' value='");
    html+=String(i);
    html+=F("'><label>Medicamento</label><input name='remedio' maxlength='40' required value='");
    html+=escapeHtml(gavetas[i].remedio);
    html+=F("'><label>Hora</label><input type='number' name='hora' min='0' max='23' value='");
    html+=String(gavetas[i].hora);
    html+=F("'><label>Minuto</label><input type='number' name='minuto' min='0' max='59' value='");
    html+=String(gavetas[i].minuto);
    html+=F("'><button>Salvar</button></form></section>");
  }
  html+=F("<section><form action='/silenciar' method='post'><button>Silenciar alarmes de hoje</button></form></section>");
  html+=F("<p>Protótipo educacional. Use também outro lembrete confiável.</p></body></html>");
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
