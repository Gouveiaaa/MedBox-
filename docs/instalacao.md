# Instalação e pinout

## Bibliotecas
- ESP8266WiFi (core ESP8266)
- ESP8266WebServer (core ESP8266)
- Wire (I2C)
- WiFiUDP (core ESP8266)
- Adafruit GFX Library
- Adafruit SSD1306
- NTPClient

## Ligações propostas
| Sinal | NodeMCU | GPIO |
|---|---|---|
| SDA OLED | D2 | 4 |
| SCL OLED | D1 | 5 |
| Buzzer (sinal) | D5 | 14 |
| LED integrado | D4 | 2 |

**Verifique o modelo exato do ESP8266 com OLED integrado.** O barramento I2C e o endereço 0x3C não são universais. Confira tensão de operação e corrente máxima do buzzer. Use transistor/driver e proteção conforme o componente. Não aplique 5 V diretamente aos GPIOs do ESP8266.

## Configuração
1. Crie `firmware/secrets.h` a partir de `firmware/secrets.example.h`.
2. Substitua todos os valores de exemplo por credenciais novas e fortes.
3. Compile e carregue `firmware/MedBox.ino`.
4. Conecte o telefone/computador à mesma rede Wi-Fi do dispositivo.
5. Abra o endereço IP exibido no OLED.

A rede precisa operar em 2,4 GHz, compatível com ESP8266.

## Diagnóstico
- Sem imagem no OLED: confirme pinagem I2C, endereço e alimentação.
- Sem Wi-Fi: confira SSID, senha, alcance e frequência 2,4 GHz.
- Sem horário: confira acesso UDP/NTP e conectividade.
- Sem som: confira se o buzzer é ativo/passivo e se o driver elétrico é apropriado.
