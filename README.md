# 💊 MedBox Inteligente

Protótipo educacional de caixa de medicamentos com **3 gavetas manuais**, ESP8266 NodeMCU com OLED integrado de 0,96″, três LEDs externos e **painel web responsivo para Android, iOS, tablets e computadores**.

> ⚠️ Protótipo não certificado como dispositivo médico. Não dependa exclusivamente dele para lembrar medicamentos.

## Funcionalidades implementadas no firmware

- Cadastro do nome de medicamento, hora e minuto para cada uma das três gavetas. Novas instalações começam sem horários fictícios cadastrados.
- Painel web responsivo: cartões empilhados no celular e distribuídos conforme a largura em PC/tablet.
- **Tela de login própria antes do painel**, com usuário e senha; sessão temporária (30 minutos de inatividade), botão Sair, proteção de formulários (CSRF) e bloqueio temporário após cinco falhas.
- Persistência de nomes e horários no **LittleFS** do ESP8266.
- **Confirmação manual de cada dose pelo site:** somente após o horário agendado, o responsável pode registrar a dose, encerrar apenas o LED daquela gaveta e consultar histórico local de até 24 confirmações. Essa ação não comprova que o remédio foi ingerido.
- Situações da dose: agendada, horário passou (aguardando confirmação), ou confirmada manualmente no dia. O OLED mostra a quantidade confirmada e as gavetas ativas.
- **Desfazer confirmação** na própria gaveta e no histórico (inclusive registro de outro dia, se ainda constar entre os 24 recentes). Reverte apenas a marcação escolhida, atualiza o histórico e o OLED, sem religar automaticamente o LED.
- Botão **Silenciar LEDs atuais** (não cancela lembretes futuros), **Testar LEDs** e **Limpar** cada gaveta.
- **OLED automático:** mostra continuamente o horário local (após NTP), a próxima gaveta/medicamento, horário e tempo restante; nomes e horários alterados no site aparecem sem recompilar. Durante o lembrete, mostra a gaveta e o medicamento e depois retorna à tela principal.
- Aviso no OLED e LED da gaveta piscando por um minuto no horário programado.
- Sincronização NTP com `a.st1.ntp.br`, fuso fixo UTC-3.
- Reconexão Wi-Fi e inicialização do painel web após a rede voltar (a validar na placa).
- **Sem som nem buzzer:** alertas exclusivamente luminosos. Teste integrado dos LEDs por gaveta (3 segundos cada), e botão para interromper LEDs atualmente ativos.
- Painel com relógio NTP (quando sincronizado), próximo medicamento, cronômetro e cartões para editar/limpar os horários. Sem alegar automaticamente que o medicamento foi ingerido.
- Acesso pelo **IP local** e anúncio mDNS como **medbox.local** quando disponível.

## Hardware e conexões propostas

| Componente | Conexão na configuração padrão |
|---|---|
| HW-364A com OLED SSD1306 integrado, 128×64 | SDA D6 (GPIO14), SCL D5 (GPIO12), endereço `0x3C` |
| LED vermelho, gaveta 1 | D1 (GPIO5) |
| LED azul, gaveta 2 | D2 (GPIO4) |
| LED verde, gaveta 3 | D7 (GPIO13) |
| 3 resistores de 220 Ω e jumpers | Um resistor em série por LED, com o cátodo de cada LED ligado ao GND |

O firmware agora usa **por padrão a pinagem HW-364A**. Confirme no hardware antes de conectar LEDs ou buzzer, pois existem revisões e placas com pinagem diferente. Veja [ligações e cuidados](docs/instalacao.md#2-ligações-propostas).

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

**Importante:** o login usa um formulário próprio e uma sessão por cookie `HttpOnly; SameSite=Strict`, mas o ESP8266 serve **HTTP sem TLS**. Isso **não criptografa** as credenciais nem o cookie na rede. Utilize somente redes locais confiáveis, senhas exclusivas e não exponha a porta 80 na internet. O dispositivo armazena uma sessão por vez e a sessão expira após 30 minutos de inatividade.

## Estado de validação e limitações

O firmware é compilado no GitHub Actions para ESP8266 e já compilou no ArduinoDroid em versão anterior. **O OLED automático desta atualização ainda precisa ser testado fisicamente na HW-364A.**

- O servidor web agora tenta iniciar ao voltar a conexão mesmo após falha de Wi-Fi no boot; comportamento ainda não validado em hardware.
- O mDNS é reinicializado nas reconexões, mas pode não funcionar em todos os hotspots ou revisões.
- Sem sincronização NTP válida, não há alarme baseado em horário.
- Os horários persistem no LittleFS, mas o registro de disparos e o estado de silenciamento não são persistidos.
- O sistema não confirma abertura de gaveta nem ingestão do medicamento.
- Os nomes e horários padrão são exemplos, não orientações médicas.

## Problemas de upload na HW-364A

Se aparecer `Timed out waiting for packet header` com `boot mode:(7,x)`, **não é erro de compilação ou de pinagem do OLED**: o ESP8266 nem chegou a iniciar o programa. Consulte o [diagnóstico de gravação COM4, FLASH/RSET e GPIO15](docs/diagnostico-upload-hw364a.md). O `boot mode:(1,x)` é o carregador serial; `(3,x)` executa o firmware. Não faça curtos ou novas ligações sem confirmar a pinagem com instrumento.

Um teste mínimo do barramento OLED, **independente de Wi-Fi e bibliotecas Adafruit**, está em [diagnosticos/Diagnostico_HW364A](diagnosticos/Diagnostico_HW364A). Ele também exige que a placa aceite gravação primeiro.

## Documentação

- [Instalação, credenciais e rede](docs/instalacao.md)
- [Diagnóstico de upload da HW-364A](docs/diagnostico-upload-hw364a.md)
- [Passo a passo completo pelo celular](docs/guia-completo-pelo-celular.md)
- [Funcionamento, limitações e segurança](docs/funcionamento-e-seguranca.md)
- [Firmware](firmware/MedBox.ino)
- [Modelo de credenciais](firmware/secrets.example.h)

## Licença

Este projeto é distribuído sob a [Licença MIT](LICENSE). Copyright (c) 2026 Pedro Augusto Gouveia Rodrigues.
