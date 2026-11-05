# 💊 MedBox Inteligente

Protótipo educacional de caixa de medicamentos com **3 gavetas manuais**, ESP8266 NodeMCU com OLED integrado de 0,96″, três LEDs externos e **painel web responsivo para Android, iOS, tablets e computadores**.

> ⚠️ Protótipo não certificado como dispositivo médico. Não dependa exclusivamente dele para lembrar medicamentos.

## Funcionalidades implementadas no firmware

- Cadastro do nome de medicamento, hora e minuto para cada uma das três gavetas.
- Painel web responsivo: cartões empilhados no celular e distribuídos conforme a largura em PC/tablet.
- Proteção do painel com **usuário e senha** via autenticação HTTP Basic.
- Persistência de nomes e horários no **LittleFS** do ESP8266.
- Botão **Silenciar alarmes de hoje**.
- Aviso no OLED e LED da gaveta piscando por um minuto no horário programado.
- Sincronização NTP com `a.st1.ntp.br`, fuso fixo UTC-3.
- Tentativas de reconexão Wi-Fi.
- Acesso pelo **IP local** e anúncio mDNS como **medbox.local** quando disponível.

## Hardware e conexões propostas

| Componente | Conexão na configuração padrão |
|---|---|
| NodeMCU ESP8266 com OLED SSD1306 integrado, 128×64 | SDA D2 (GPIO4), SCL D1 (GPIO5), endereço `0x3C` |
| LED azul, gaveta 1 | D5 (GPIO14) |
| LED vermelho, gaveta 2 | D6 (GPIO12) |
| LED verde, gaveta 3 | D7 (GPIO13) |
| 3 resistores de 220 Ω e jumpers | Um resistor em série por LED, com o cátodo de cada LED ligado ao GND |

Confira os pinos do OLED da sua placa antes de ligar os LEDs. Veja a [montagem e a pinagem alternativa](docs/instalacao.md#2-ligações-propostas) para placas com OLED em D6/D5.

## Fazer tudo pelo celular 📱

Veja o [guia completo para Android, USB OTG, hotspot e painel web](docs/guia-completo-pelo-celular.md). A gravação via Android depende de suporte do aplicativo à placa ESP8266; ainda não foi validada no hardware.

## Instalação rápida

1. Instale o **Arduino IDE** e o pacote de placas ESP8266.
2. Instale as bibliotecas **Adafruit GFX**, **Adafruit SSD1306** e **NTPClient**.
3. Copie `firmware/secrets.example.h` para `firmware/secrets.h` e configure **SSID, senha do Wi-Fi, usuário e senha do painel**. O arquivo `secrets.h` não deve ser publicado.
4. Coloque `MedBox.ino` e `secrets.h` na mesma pasta, chamada `MedBox`. Abra o sketch, selecione a placa e a porta e grave o firmware.
5. Ative o Wi-Fi 2,4 GHz do roteador ou hotspot do celular **antes** de ligar a MedBox.
6. Consulte o IP mostrado no OLED ou Monitor Serial a **115200 baud**.
7. Abra `http://medbox.local` se funcionar no seu dispositivo; caso contrário, use `http://IP_EXIBIDO`.

O hotspot pode impedir acesso pelo próprio celular ou descoberta mDNS. Nesses casos, use um segundo celular ou notebook conectado ao hotspot e acesse pelo IP.

## Usuários e senhas

O firmware **mantém** os campos `MEDBOX_ADMIN_USER` e `MEDBOX_ADMIN_PASSWORD` e os campos de Wi-Fi `MEDBOX_WIFI_SSID` e `MEDBOX_WIFI_PASSWORD`. Eles são definidos em `secrets.h` local. O arquivo `secrets.example.h` documenta todos os campos sem expor senhas reais.

**Importante:** o painel usa HTTP Basic sobre HTTP sem TLS. Utilize somente redes locais confiáveis e não abra a porta 80 na internet.

## Estado de validação e limitações

O comportamento dos LEDs e do OLED foi verificado com periféricos simulados em C++. **A compilação para ESP8266 e o teste na placa estão pendentes.**

- A conexão inicial Wi-Fi precisa ocorrer dentro da janela de 20 segundos para o servidor ser iniciado; uma reconexão posterior não corrige automaticamente esse caso.
- O mDNS é iniciado após conexão no `setup()`; após certas quedas de Wi-Fi, pode precisar de reinicialização.
- Sem sincronização NTP válida, não há alarme baseado em horário.
- Os horários persistem no LittleFS, mas o registro de disparos e o estado de silenciamento não são persistidos.
- O sistema não confirma abertura de gaveta nem ingestão do medicamento.
- Os nomes e horários padrão são exemplos, não orientações médicas.

## Documentação

- [Instalação, credenciais e rede](docs/instalacao.md)
- [Passo a passo completo pelo celular](docs/guia-completo-pelo-celular.md)
- [Funcionamento, limitações e segurança](docs/funcionamento-e-seguranca.md)
- [Firmware](firmware/MedBox.ino)
- [Modelo de credenciais](firmware/secrets.example.h)

## Licença

Este projeto é distribuído sob a [Licença MIT](LICENSE). Copyright (c) 2026 Pedro Augusto Gouveia Rodrigues.
