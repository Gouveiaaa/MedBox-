# Instalação da MedBox: Arduino, Wi-Fi, usuários e acesso web

📱 **Quer fazer o procedimento pelo Android?** Siga o [guia completo pelo celular](guia-completo-pelo-celular.md), incluindo USB OTG, hotspot e uso do painel. A compilação e gravação por Android dependem de compatibilidade ainda não validada.

## 1. Pré-requisitos

- ESP8266 NodeMCU com OLED SSD1306 integrado de 0,96″ e resolução de 128×64.
- LEDs azul, vermelho e verde, três resistores de 220 Ω e jumpers.
- Cabo USB de dados e alimentação adequada.
- Arduino IDE com pacote de placas **ESP8266 by ESP8266 Community**.
- Bibliotecas adicionais: **Adafruit GFX Library**, **Adafruit SSD1306**, **NTPClient**.
- Bibliotecas do core: `ESP8266WiFi`, `ESP8266WebServer`, `ESP8266mDNS`, `LittleFS`, `WiFiUDP` e `Wire`.

No Arduino IDE, adicione o índice de placas `https://arduino.esp8266.com/stable/package_esp8266com_index.json` em Preferências, instale o pacote ESP8266 pelo Gerenciador de Placas e selecione a placa correta.

## 2. Ligações propostas

| Sinal | HW-364A (padrão atual) | Outra placa com OLED em D2/D1 |
|---|---|---|
| OLED SDA | D6 (GPIO14) | D2 (GPIO4) |
| OLED SCL | D5 (GPIO12) | D1 (GPIO5) |
| LED azul, gaveta 1 | D1 (GPIO5) | D1 (GPIO5), **conflito: escolha outro pino** |
| LED vermelho, gaveta 2 | D2 (GPIO4) | D2 (GPIO4), **conflito: escolha outro pino** |
| LED verde, gaveta 3 | D7 (GPIO13) | D7 (GPIO13) |
| Buzzer ativo opcional | D0 (GPIO16), desativado no firmware | Validar circuito |

Com a placa desligada, ligue cada saída ao ânodo (perna longa) do LED por um resistor de 220 Ω. Ligue o cátodo (perna curta) ao GND. Use os jumpers para fazer as conexões e mantenha os GPIOs em 3,3 V.

O OLED já vem ligado na HW-364A. O firmware usa `OLED_HW364A = true`, com SDA GPIO14 e SCL GPIO12, endereço `0x3C`. Em uma placa diferente, alterar a pinagem do OLED exige também **revisar os pinos dos LEDs** para evitar conflitos.

O buzzer é **opcional**: use somente módulo **ativo**, compatível com 3,3 V e corrente segura para o GPIO, ou circuito com transistor e alimentação adequada. **Não** conecte um buzzer de 5 V diretamente ao GPIO. GPIO16 (D0) apenas liga/desliga esse buzzer, sem PWM; configure `HABILITAR_BUZZER_ATIVO = true` somente após validar o circuito. Não conecte nada em GPIO0/D3, GPIO2/D4 ou GPIO15/D8 durante o diagnóstico de boot.

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

Coloque `MedBox.ino` e o seu `secrets.h` na mesma pasta, chamada `MedBox`. Abra `MedBox.ino` no Arduino IDE, selecione placa e porta, compile e envie. Abra o Monitor Serial em **115200 baud**. O firmware ainda precisa ser validado por compilação para ESP8266 e teste físico.

## 5. Roteador ou hotspot do celular

Ative o hotspot do Android ou iPhone e escolha **2,4 GHz** quando houver essa opção. Coloque seu SSID e senha no `secrets.h`. Ligue o hotspot **antes** da MedBox.

O ESP8266 tentará conectar durante até 20 segundos. Após conexão bem-sucedida, inicia o servidor web na porta 80 e imprime o IP no Monitor Serial, além de tentar mostrá-lo no OLED.

Se o ESP8266 não conectar na inicialização, **reinicie a placa após corrigir a rede**: o código atual ainda não inicia o servidor quando a primeira conexão só acontece depois desse prazo.

## 6. Abrir o painel em Android, iOS e PC

Conecte o aparelho à mesma rede local do ESP8266 e abra:

- `http://medbox.local`, caso o mDNS seja suportado pelo hotspot e navegador;
- `http://IP_DO_ESP8266`, usando o IP visto no OLED ou Monitor Serial.

Ao abrir o endereço, **a primeira página é a tela `Entrar no MedBox`**, antes de qualquer horário ou medicamento. Informe o usuário e a senha de `secrets.h`. Após entrar, surge o painel responsivo das três gavetas. Use **Sair da conta** para encerrar a sessão; a sessão também vence após 30 minutos sem acessar o painel. Não há cadastro público de usuários nem senha padrão funcional: configure a senha em `secrets.h` antes de gravar o ESP8266.

**Hotspot:** alguns celulares não permitem que o aparelho que fornece a rede acesse seus próprios clientes, e alguns bloqueiam mDNS. Se necessário, use um segundo celular ou notebook conectado ao hotspot e o IP numérico.

## 7. Configurar os medicamentos

No painel, preencha nome, hora e minuto em cada uma das três gavetas e toque em **Salvar gaveta**. As configurações são gravadas em LittleFS. No horário marcado, o LED correspondente pisca por um minuto. O botão **Silenciar alarmes de hoje** apaga os LEDs e suspende os próximos avisos luminosos do dia enquanto a placa permanecer ligada.

## 8. Diagnóstico

Se a gravação falhar com `Connecting...` / `Timed out waiting for packet header`, ou houver `PermissionError(13, 'Acesso negado')`, use o [guia de upload e modos de boot da HW-364A](diagnostico-upload-hw364a.md) antes de mudar o firmware.

### Outras falhas

- **Não abre `medbox.local`:** tente o IP numérico; mDNS não é universal.
- **Sem IP:** verifique SSID, senha, 2,4 GHz, alcance e reinicie a placa.
- **OLED apagado:** confira pinagem, endereço I²C e alimentação.
- **Sem alertas:** confira NTP/internet, relógio, horário e estado de silenciamento.
- **LED apagado durante o aviso:** confira a polaridade, o resistor, o jumper, o GND e a coluna de pinagem selecionada.
- **Horários não persistem:** confira a inicialização e funcionamento do LittleFS.

**Segurança:** o formulário de login e o cookie de sessão trafegam por **HTTP sem TLS**. O cookie possui atributos `HttpOnly` e `SameSite=Strict`, mas eles não impedem interceptação em rede local insegura. Use rede Wi-Fi confiável e senha exclusiva, não publique `secrets.h` e nunca exponha a porta 80 à internet. O servidor permite uma sessão ativa; outro login encerra a anterior. Depois de cinco tentativas erradas, o login fica bloqueado por um minuto.
