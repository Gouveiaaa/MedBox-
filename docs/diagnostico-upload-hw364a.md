# Diagnóstico de upload: HW-364A / ESP8266

Este documento aborda o erro real encontrado no MedBox:
`esptool.py v3.0 ... Failed to connect to ESP8266: Timed out waiting for packet header`.
Com a placa **sem fios externos**, o Monitor Serial a 74880 baud apresentou
`boot mode:(7,2)`, `(7,1)`, `(7,7)` e `waiting for host`. A COM4 também apresentou
`PermissionError(13, 'Acesso negado')` em uma tentativa. Não há confirmação de defeito físico;
os testes abaixo visam distinguir porta ocupada, seleção de boot e hardware.

## 1. Três problemas diferentes

| Sintoma | Interpretação | Próximo passo |
|---|---|---|
| Código compila mas fica em `Connecting...` | ESP não respondeu ao esptool | Confira o modo de boot |
| `PermissionError(13)` em COM4 | Outro processo ou driver impede abrir a porta | Feche Monitor Serial, Serial Plotter e outros programas; reconecte e confira a porta |
| `boot mode:(7,x)` + `waiting for host` | Boot SDIO, **não** serial nem execução de firmware | Verifique GPIO15 baixo durante reset |
| `boot mode:(1,x)` | ROM bootloader UART | Feche o Monitor Serial e tente upload |
| `boot mode:(3,x)` | Inicialização a partir da flash | Verifique GPIO0/FLASH para modo 1 |

O segundo número depois da vírgula **não** é o modo de upload. O primeiro é o determinante.

## 2. Configuração do software

1. Deixe a HW-364A somente com o cabo USB de **dados**, sem LED, buzzer, jumper e resistor externo.
2. No Arduino IDE, use o pacote ESP8266 by ESP8266 Community.
3. Selecione **Generic ESP8266 Module** (ponto de partida para HW-364A), a **porta COM que aparece agora** e `Upload Speed = 115200`. O modo DIO pode ser testado para compatibilidade.
4. Feche o **Monitor Serial e Plotter** antes de carregar. Se o Windows negar a porta, feche outras janelas do IDE e outros programas seriais; reinicie o IDE e reavalie a porta no Gerenciador de Dispositivos.
5. Para o bootloader manual: **segure FLASH**, pressione e solte **RSET** mantendo FLASH pressionado, depois solte FLASH. Confira no Monitor Serial a 74880 baud o primeiro número do `boot mode`. Feche o Monitor Serial antes de enviar o sketch.

### Atenção: `boot mode 7` não é causado pelo código Arduino

O boot ROM lê pinos elétricos **antes** de começar a executar qualquer sketch:

- GPIO15 / D8: nível **baixo**.
- GPIO2 / D4: nível **alto**.
- GPIO0 / D3: nível **baixo** para gravar (`boot mode 1`), **alto** para executar (`boot mode 3`).

Se o valor permanece `(7,x)` com a placa nua, **trocar Arduino IDE por PlatformIO, esptool ou Flash Download Tool não remove a exigência elétrica**.

## 3. Verificação física sem improvisar ligações

- Com multímetro, meça **tensão DC** entre D8 (GPIO15) e GND **durante o reset**; use pontas finas e não faça curto entre pinos. É esperado nível baixo no instante de amostragem. Uma leitura estática pode não capturar pulsos: se houver dúvida, peça ajuda a alguém com experiência em eletrônica.
- A placa normalmente deve possuir o circuito de pull-down de GPIO15. Se o sinal estiver alto apesar de estar totalmente desconectada de periféricos, suspeite de ligação errada, componente ou trilha defeituosa. Não há confirmação conclusiva sem medir.
- Caso não tenha equipamento, teste outro cabo de dados e outra porta/computador. Persistindo o modo 7, por ser uma placa nova e nunca gravada, solicite troca/garantia em vez de soldar componentes às cegas.
- **Não** ligue pinos diretamente a 5 V; **não** faça pontes ou soldas com USB conectado.

## 4. Teste mínimo depois que a gravação funcionar

Use `diagnosticos/Diagnostico_HW364A/Diagnostico_HW364A.ino` primeiro. Ele só usa `Wire`, imprime no Serial a 115200 baud e procura `0x3C` com SDA GPIO14 e SCL GPIO12. Se encontrar o OLED, grave então o firmware completo do MedBox com `secrets.h` local.

Se encontrar o dispositivo, mas a tela não mostrar conteúdo, teste separadamente o SSD1306 (Adafruit GFX + SSD1306) com os mesmos pinos. Caso `0x3C` não responda, revise a revisão de hardware e a pinagem.

Fontes técnicas:
- [Espressif: boot mode selection](https://docs.espressif.com/projects/esptool/en/latest/esp8266/advanced-topics/boot-mode-selection.html)
- [Espressif: entering the bootloader](https://docs.espressif.com/projects/esptool/en/latest/esp8266/esptool/entering-bootloader.html)
- [Pinagem documentada da HW-364A](https://github.com/peff74/esp8266_OLED_HW-364A)

> Este é um protótipo educacional. Nunca dependa exclusivamente da MedBox para tomar medicamentos.
