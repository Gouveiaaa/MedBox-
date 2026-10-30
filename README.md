# 💊 MedBox Inteligente

Protótipo educacional de caixa de medicamentos com **3 gavetas manuais**, ESP8266 NodeMCU, OLED SSD1306, buzzer, LED e **painel web responsivo para Android, iOS, tablets e computadores**.

> ⚠️ Protótipo não certificado como dispositivo médico. Não dependa exclusivamente dele para lembrar medicamentos.

## Funcionalidades implementadas no firmware

- Cadastro do nome de medicamento, hora e minuto para cada uma das três gavetas.
- Painel web responsivo: cartões empilhados no celular e distribuídos conforme a largura em PC/tablet.
- Proteção do painel com **usuário e senha** via autenticação HTTP Basic.
- Persistência de nomes e horários no **LittleFS** do ESP8266.
- Botão **Silenciar alarmes de hoje**.
- Alerta no OLED, buzzer e LED quando o horário programado é atingido.
- Sincronização NTP com `a.st1.ntp.br`, fuso fixo UTC-3.
- Tentativas de reconexão Wi-Fi.
- Acesso pelo **IP local** e anúncio mDNS como **medbox.local** quando disponível.

## Hardware e conexões propostas

| Componente | Conexão prevista |
|---|---|
| ESP8266 NodeMCU com OLED integrado | Controlador |
| OLED SSD1306 128×64 | SDA GPIO4 (D2), SCL GPIO5 (D1), endereço 0x3C, **a confirmar no modelo real** |
| Buzzer | GPIO14 (D5), com acionamento elétrico apropriado |
| LED integrado | GPIO2 (D4), ativo em nível baixo |

Confirme o pinout do modelo real antes de conectar. GPIO2 participa da inicialização do ESP8266. Não aplique 5 V aos GPIOs.

## Instalação rápida

1. Instale o **Arduino IDE** e o pacote de placas ESP8266.
2. Instale as bibliotecas **Adafruit GFX**, **Adafruit SSD1306** e **NTPClient**.
3. Copie `firmware/secrets.example.h` para `firmware/secrets.h` e configure **SSID, senha do Wi-Fi, usuário e senha do painel**. O arquivo `secrets.h` não deve ser publicado.
4. Abra `firmware/MedBox.ino`, selecione a placa/porta e grave o firmware.
5. Ative o Wi-Fi 2,4 GHz do roteador ou hotspot do celular **antes** de ligar a MedBox.
6. Consulte o IP mostrado no OLED ou Monitor Serial a **115200 baud**.
7. Abra `http://medbox.local` se funcionar no seu dispositivo; caso contrário, use `http://IP_EXIBIDO`.

O hotspot pode impedir acesso pelo próprio celular ou descoberta mDNS. Nesses casos, use um segundo celular ou notebook conectado ao hotspot e acesse pelo IP.

## Usuários e senhas

O firmware **mantém** os campos `MEDBOX_ADMIN_USER` e `MEDBOX_ADMIN_PASSWORD` e os campos de Wi-Fi `MEDBOX_WIFI_SSID` e `MEDBOX_WIFI_PASSWORD`. Eles são definidos em `secrets.h` local. O arquivo `secrets.example.h` documenta todos os campos sem expor senhas reais.

**Importante:** o painel usa HTTP Basic sobre HTTP sem TLS. Utilize somente redes locais confiáveis e não abra a porta 80 na internet.

## Estado de validação e limitações

O código foi atualizado no repositório, mas **ainda não há confirmação de compilação nem testes no hardware real**.

- A conexão inicial Wi-Fi precisa ocorrer dentro da janela de 20 segundos para o servidor ser iniciado; uma reconexão posterior não corrige automaticamente esse caso.
- O mDNS é iniciado após conexão no `setup()`; após certas quedas de Wi-Fi, pode precisar de reinicialização.
- Sem sincronização NTP válida, não há alarme baseado em horário.
- Os horários persistem no LittleFS, mas o registro de disparos e o estado de silenciamento não são persistidos.
- O sistema não confirma abertura de gaveta nem ingestão do medicamento.
- Os nomes e horários padrão são exemplos, não orientações médicas.

## Documentação

- [Instalação, credenciais e rede](docs/instalacao.md)
- [Funcionamento, limitações e segurança](docs/funcionamento-e-seguranca.md)
- [Firmware](firmware/MedBox.ino)
- [Modelo de credenciais](firmware/secrets.example.h)

## Licença

Este projeto é distribuído sob a [Licença MIT](LICENSE). Copyright (c) 2026 Pedro Augusto Gouveia Rodrigues.
