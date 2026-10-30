# 💊 MedBox Inteligente

Protótipo educacional de caixa inteligente para lembretes de medicamentos, com **ESP8266**, **OLED SSD1306**, buzzer, LED e painel web local.

> ⚠️ **Não é um dispositivo médico certificado.** Não use como único lembrete de medicamentos. Quedas de energia, falhas de Wi-Fi, relógio ou hardware podem impedir alertas.

## Recursos
- Configuração de **3 gavetas** (nome do medicamento e horário).
- Painel web local protegido por autenticação HTTP Basic.
- Horário via NTP `a.st1.ntp.br`, fuso UTC-3 (sem ajuste automático de horário de verão).
- Aviso no OLED, LED e buzzer.
- Proteção contra repetição do alarme no mesmo dia enquanto o dispositivo permanece ligado.

## Hardware
| Componente | Conexão prevista |
|---|---|
| NodeMCU ESP8266 com OLED integrado | Microcontrolador |
| OLED SSD1306 128×64 | SDA GPIO4 (D2), SCL GPIO5 (D1), endereço 0x3C **a confirmar na placa** |
| Buzzer | GPIO14 (D5), com circuito de acionamento adequado |
| LED integrado (ativo em LOW) | GPIO2 (D4) |

**Atenção:** placas com OLED integrado podem ter pinagem diferente. Confirme o esquema elétrico antes de energizar. GPIO2 é pino de boot: não conecte cargas que alterem seu estado na inicialização. Não ligue buzzer de corrente elevada diretamente ao GPIO.

## Instalação
1. Instale o core **ESP8266 para Arduino** e as bibliotecas **Adafruit GFX**, **Adafruit SSD1306** e **NTPClient**. `ESP8266WiFi`, `ESP8266WebServer`, `Wire` e `WiFiUDP` vêm com o ambiente correspondente.
2. Copie `firmware/secrets.example.h` para `firmware/secrets.h` e configure **suas próprias** credenciais. Esse arquivo é ignorado pelo Git.
3. Abra `firmware/MedBox.ino` no ambiente Arduino compatível, selecione a placa e compile.
4. Ligue o ESP8266 à rede Wi-Fi 2,4 GHz. Veja o IP no OLED ou no monitor serial.
5. No mesmo Wi-Fi, abra o IP exibido no navegador e autentique-se.

O ArduinoDroid pode ter limitações de compatibilidade com o core ESP8266 e bibliotecas. Caso não compile, use Arduino IDE em um computador.

## Limitações da versão inicial
- Configurações das gavetas ficam **somente na RAM** e se perdem ao reiniciar.
- Exige Wi-Fi e sincronização NTP para alarmes confiáveis; após reinício sem rede não há horário validado.
- O painel usa HTTP sem criptografia: **somente em rede local confiável**, sem redirecionar a porta 80 para a internet.
- O buzzer bloqueia brevemente a execução durante o toque; não há sensor de abertura das gavetas nem confirmação de dose.
- Horários e medicamentos iniciais são **exemplos ilustrativos**, não recomendações médicas.

## Documentação
- [Instalação e pinout](docs/instalacao.md)
- [Funcionamento e segurança](docs/funcionamento-e-seguranca.md)
- [Firmware](firmware/MedBox.ino)

## Licença
Nenhuma licença de reutilização foi concedida por enquanto. Consulte o proprietário antes de redistribuir ou modificar.
