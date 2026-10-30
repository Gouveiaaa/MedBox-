# Instalação da MedBox: Arduino, Wi-Fi, usuários e acesso web

## 1. Pré-requisitos

- ESP8266 NodeMCU com OLED SSD1306 integrado (pinagem real a confirmar).
- Cabo USB de dados e alimentação adequada.
- Arduino IDE com pacote de placas **ESP8266 by ESP8266 Community**.
- Bibliotecas adicionais: **Adafruit GFX Library**, **Adafruit SSD1306**, **NTPClient**.
- Bibliotecas do core: `ESP8266WiFi`, `ESP8266WebServer`, `ESP8266mDNS`, `LittleFS`, `WiFiUDP` e `Wire`.

No Arduino IDE, adicione o índice de placas `https://arduino.esp8266.com/stable/package_esp8266com_index.json` em Preferências, instale o pacote ESP8266 pelo Gerenciador de Placas e selecione a placa correta.

## 2. Ligações propostas

| Sinal | NodeMCU | GPIO |
|---|---|---|
| OLED SDA | D2 | 4 |
| OLED SCL | D1 | 5 |
| Buzzer (sinal) | D5 | 14 |
| LED integrado | D4 | 2 |

**Atenção:** em placas com OLED integrado, os pinos I²C e o endereço podem ser diferentes. Verifique o modelo exato. Use driver/transistor apropriado para buzzer de maior corrente. Não aplique 5 V aos GPIOs.

## 3. Manter usuário e senha no projeto

Copie `firmware/secrets.example.h` para `firmware/secrets.h` e edite o arquivo local:

```cpp
#pragma once
#define MEDBOX_WIFI_SSID "NOME_DA_REDE_2G4"
#define MEDBOX_WIFI_PASSWORD "SENHA_DO_WIFI"
#define MEDBOX_ADMIN_USER "admin"
#define MEDBOX_ADMIN_PASSWORD "SENHA_DO_PAINEL"
```

**Não exclua esses campos**: o código depende deles. Use suas próprias credenciais. O arquivo real `secrets.h` é ignorado pelo Git para não expor senhas no repositório público. Não confunda a senha do hotspot com a senha do painel.

## 4. Gravar o firmware

Abra `firmware/MedBox.ino` no Arduino IDE, selecione placa e porta, compile e envie. Abra o Monitor Serial em **115200 baud**. O firmware ainda precisa ser validado por compilação e teste físico.

## 5. Roteador ou hotspot do celular

Ative o hotspot do Android ou iPhone e escolha **2,4 GHz** quando houver essa opção. Coloque seu SSID e senha no `secrets.h`. Ligue o hotspot **antes** da MedBox.

O ESP8266 tentará conectar durante até 20 segundos. Após conexão bem-sucedida, inicia o servidor web na porta 80 e imprime o IP no Monitor Serial, além de tentar mostrá-lo no OLED.

Se o ESP8266 não conectar na inicialização, **reinicie a placa após corrigir a rede**: o código atual ainda não inicia o servidor quando a primeira conexão só acontece depois desse prazo.

## 6. Abrir o painel em Android, iOS e PC

Conecte o aparelho à mesma rede local do ESP8266 e abra:

- `http://medbox.local`, caso o mDNS seja suportado pelo hotspot e navegador;
- `http://IP_DO_ESP8266`, usando o IP visto no OLED ou Monitor Serial.

O painel usa layout responsivo: no celular os cartões ficam em coluna; em PC, podem aparecer lado a lado. Ao entrar, informe o usuário e a senha definidos no `secrets.h`.

**Hotspot:** alguns celulares não permitem que o aparelho que fornece a rede acesse seus próprios clientes, e alguns bloqueiam mDNS. Se necessário, use um segundo celular ou notebook conectado ao hotspot e o IP numérico.

## 7. Configurar os medicamentos

No painel, preencha nome, hora e minuto em cada uma das três gavetas e toque em **Salvar gaveta**. As configurações são gravadas em LittleFS. O botão **Silenciar alarmes de hoje** suprime os toques do restante do dia enquanto a placa permanecer ligada.

## 8. Diagnóstico

- **Não abre `medbox.local`:** tente o IP numérico; mDNS não é universal.
- **Sem IP:** verifique SSID, senha, 2,4 GHz, alcance e reinicie a placa.
- **OLED apagado:** confira pinagem, endereço I²C e alimentação.
- **Sem alertas:** confira NTP/internet, relógio, horário e estado de silenciamento.
- **Sem buzzer:** verifique tipo de buzzer e circuito de acionamento.
- **Horários não persistem:** confira a inicialização e funcionamento do LittleFS.

**Segurança:** HTTP Basic não criptografa credenciais. Use rede confiável e nunca publique `secrets.h` ou exponha a porta 80 à internet.
