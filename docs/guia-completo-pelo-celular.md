# 📱 MedBox: fazer tudo pelo celular Android

Este guia ensina a **preparar o código, tentar compilar/gravar o ESP8266 e utilizar o painel pelo celular**, sem exigir computador no fluxo principal. **A gravação totalmente pelo celular depende da compatibilidade do Android, do adaptador USB OTG e do aplicativo de programação. Não foi validada no hardware deste projeto.**

> **iPhone/iPad:** é possível acessar e configurar o painel web, mas gravar firmware ESP8266 diretamente por iOS não é um procedimento suportado neste guia. Para a primeira gravação, utilize um Android compatível ou computador.

## 1. O que separar

- Celular **Android** com suporte **USB OTG/USB host**.
- Cabo USB **de dados** compatível com o ESP8266 e, se necessário, adaptador **USB-C OTG**.
- Placa **ESP8266 NodeMCU com OLED integrado**, LEDs azul, vermelho e verde, três resistores e jumpers. Veja a [pinagem da montagem](instalacao.md#2-ligações-propostas).
- Acesso à internet para baixar ferramentas e bibliotecas.
- Idealmente, **um segundo celular**: o primeiro fornece o hotspot; o segundo abre o painel. Também é possível usar um roteador Wi-Fi 2,4 GHz existente.

**Antes de começar:** confirme se seu modelo de placa pode ser programado pela porta USB e qual chip USB-serial usa (por exemplo, CH340 ou CP210x). Nem todo aplicativo Android oferece driver para todos os chips.

## 2. Obter o código no celular

1. Abra o navegador no Android e acesse o [repositório MedBox](https://github.com/Gouveiaaa/MedBox-).
2. Toque em **Code → Download ZIP** (ou obtenha os arquivos `firmware/MedBox.ino` e `firmware/secrets.example.h`).
3. Extraia o ZIP com o gerenciador de arquivos do Android.
4. Localize a pasta `firmware`.

**Importante:** o código Arduino inclui `"secrets.h"`. Esse arquivo deve existir na mesma pasta do `MedBox.ino` no ambiente de compilação. Se o aplicativo exigir que o sketch e a pasta tenham o mesmo nome, reorganize localmente sem mudar o conteúdo do código.

## 3. Instalar uma ferramenta de programação Android

Você precisará de um aplicativo/ambiente Android que **suporte explicitamente compilar para ESP8266 e enviar firmware via USB OTG**, com as bibliotecas usadas pelo projeto. Um editor de texto sozinho **não grava** a placa.

O **ArduinoDroid** é uma alternativa que pode ser investigada, mas **a compatibilidade com ESP8266, core, bibliotecas e upload USB depende da versão**. Não considere o suporte garantido. Confira a documentação e as opções de placa antes de prosseguir.

Se o aplicativo não disponibilizar ESP8266, Adafruit SSD1306, Adafruit GFX, NTPClient ou upload por USB, **pare nessa etapa**: o procedimento não está validado para esse aplicativo. A alternativa mais previsível é compilar/gravar uma vez pelo Arduino IDE em um PC; **depois disso, toda a configuração de medicamentos e o uso diário são feitos pelo celular**.

## 4. Configurar Wi-Fi, usuário e senha sem publicá-los

1. Na pasta local do firmware, copie `secrets.example.h` e renomeie a cópia para **`secrets.h`**.
2. Abra `secrets.h` em um editor de código do Android.
3. Ajuste estes campos:

```cpp
#pragma once
#define MEDBOX_WIFI_SSID "NOME_DO_WIFI_2G4"
#define MEDBOX_WIFI_PASSWORD "SENHA_DO_WIFI"
#define MEDBOX_ADMIN_USER "admin"
#define MEDBOX_ADMIN_PASSWORD "SENHA_FORTE_DO_PAINEL"
```

4. Salve o arquivo ao lado de `MedBox.ino`.
5. **Não envie `secrets.h` ao GitHub**. O repositório público preserva o modelo `secrets.example.h`, sem senhas reais.

O **usuário e a senha do painel são diferentes** da senha do Wi-Fi. O firmware não possui tela para alterar essas credenciais: modificá-las exige editar `secrets.h` e gravar o firmware novamente.

## 5. Configurar o hotspot do celular

1. Em **Configurações → Roteador Wi-Fi/Hotspot**, ative o compartilhamento.
2. Se existir a opção, escolha **banda de 2,4 GHz**. O ESP8266 não conecta em 5 GHz.
3. Defina o nome e a senha **exatamente iguais** aos valores em `secrets.h`.
4. Mantenha o hotspot ativo e com acesso à internet para o NTP.

**Usando um único Android:** ele pode servir como hotspot e, dependendo do modelo, acessar o próprio ESP8266, mas isso **não é garantido**. A alternativa mais confiável é usar **celular A** para o hotspot e **celular B** conectado à rede para abrir o painel.

## 6. Compilar e gravar pelo Android (se o ambiente for compatível)

1. Abra `MedBox.ino` no aplicativo de programação.
2. Selecione a **placa ESP8266/NodeMCU correta**.
3. Instale ou disponibilize as bibliotecas **Adafruit GFX**, **Adafruit SSD1306** e **NTPClient**, além do core ESP8266 que fornece as demais bibliotecas.
4. Conecte o ESP8266 ao Android com cabo **de dados** e adaptador **USB OTG**, se necessário.
5. Autorize o acesso USB quando o Android solicitar.
6. Use a opção **Compilar/Verificar**. Corrija eventuais erros de dependências ou placa.
7. Se compilar, use **Enviar/Upload** e aguarde a conclusão antes de desconectar.
8. Se houver monitor serial no aplicativo, ajuste para **115200 baud** para observar o IP e eventuais mensagens.

**Não há confirmação de que este firmware compila no ArduinoDroid ou foi gravado com sucesso por Android.** Não prometa que o aplicativo fará upload sem verificar o suporte à placa e ao chip USB-serial.

## 7. Ligar a MedBox e descobrir o endereço

1. Deixe o hotspot/roteador 2,4 GHz ativo **antes** de ligar a placa.
2. Ligue ou reinicie o ESP8266.
3. Aguarde a conexão. O código tenta conectar durante aproximadamente **20 segundos** no início.
4. Consulte o **IP no OLED** ou no **Monitor Serial a 115200 baud**. Exemplo ilustrativo: `192.168.43.125` (o IP real será diferente).
5. Se o Wi-Fi não conectar na primeira tentativa, confira as credenciais e mantenha a rede disponível. O firmware agora tenta iniciar o painel quando reconectar, mas essa melhoria ainda precisa de teste físico.

## 8. Abrir e usar o painel no celular

1. Conecte o aparelho que acessará o painel **à mesma rede** da MedBox.
2. Abra **Chrome** no Android ou **Safari** no iPhone.
3. Tente digitar **`http://medbox.local`** na barra de endereço.
4. Se não funcionar, digite **`http://` seguido do IP mostrado no OLED**, por exemplo `http://192.168.43.125`.
5. Quando o navegador pedir autenticação, use **`MEDBOX_ADMIN_USER`** e **`MEDBOX_ADMIN_PASSWORD`** definidos no seu `secrets.h`.
6. Na página, preencha **medicamento, hora e minuto** de cada uma das **3 gavetas**.
7. Toque em **Salvar** na gaveta correspondente. Os dados são armazenados no **LittleFS** e devem permanecer após reiniciar.
8. Use **Silenciar alarmes de hoje** para apagar os LEDs e suspender os próximos avisos luminosos do dia enquanto o ESP8266 estiver ligado.

O painel se adapta à largura da tela: cartões em coluna no celular e em grade em telas maiores. **Não é preciso instalar um aplicativo nativo** para usá-lo depois que o firmware estiver gravado.

## 9. Criar atalho na tela inicial (opcional)

- **Android/Chrome:** com o painel aberto, abra o menu **⋮** e procure **Adicionar à tela inicial**. A opção exata varia conforme a versão.
- **iPhone/Safari:** abra o menu de compartilhamento e selecione **Adicionar à Tela de Início**.

Isso cria um **atalho para o endereço local**, não transforma a MedBox em serviço na nuvem. Se o IP mudar e `medbox.local` não funcionar, será necessário atualizar o atalho.

## 10. Problemas comuns

| Sintoma | O que verificar |
|---|---|
| Android não reconhece o ESP8266 | USB OTG, cabo de dados, alimentação e driver USB-serial do app |
| Não aparece ESP8266 no aplicativo | O ambiente de programação talvez não suporte o core/placa |
| Erro ao compilar | Core ESP8266, `secrets.h`, bibliotecas e seleção da placa |
| Sem conexão Wi-Fi | SSID, senha, hotspot ativo, banda 2,4 GHz, alcance |
| `medbox.local` não abre | mDNS pode não funcionar no Android/hotspot; tente IP |
| IP também não abre | Dispositivos na mesma rede? Hotspot isola clientes? Servidor iniciou? |
| Não aparece IP no OLED | Verifique I²C, alimentação e Monitor Serial |
| LED não pisca no horário | Verifique sincronização NTP, fuso, silenciamento e ligações do LED |
| Não salva medicamentos | Verifique LittleFS e mensagens do firmware |

## 11. Segurança e limites

- **HTTP Basic em HTTP não criptografa o usuário nem a senha**. Utilize rede local confiável, não exponha a porta 80 na internet e não reutilize senhas importantes.
- O projeto **não confirma a ingestão** de medicamentos.
- O horário depende de NTP e conectividade na sincronização; não há relógio RTC dedicado.
- A gravação e os testes físicos **ainda não foram validados**.
- **Não use como único lembrete de medicamentos.**

## 12. Fluxo resumido sem PC

**Android compatível + USB OTG → baixar arquivos → criar `secrets.h` → compilar → gravar ESP8266 → ativar hotspot 2,4 GHz → reiniciar MedBox → acessar `medbox.local` ou IP → autenticar → cadastrar 3 gavetas.**

Se a etapa de compilação/gravação pelo Android não for compatível, use um PC **somente para a primeira gravação**; todo o restante continua acessível pelo celular.
