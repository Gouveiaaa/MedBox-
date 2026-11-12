# Funcionamento, rede, segurança e limitações da MedBox

## Fluxo operacional atual

1. O ESP8266 inicializa periféricos, monta LittleFS e tenta carregar as configurações das três gavetas.
2. Tenta conectar ao Wi-Fi 2,4 GHz configurado no `secrets.h`.
3. Quando conecta durante a inicialização, sincroniza horário com `a.st1.ntp.br` (UTC-3), inicia servidor HTTP e anuncia `medbox.local` via mDNS.
4. Mostra o IP na inicialização e na tela principal do OLED, e imprime no Monitor Serial a 115200 baud. O OLED mostra a hora atual e o próximo medicamento após sincronização NTP.
5. O usuário acessa pelo IP ou por `http://medbox.local` quando a rede suportar mDNS.
6. A primeira tela é **Entrar no MedBox**. Só depois de autenticar com `MEDBOX_ADMIN_USER` e `MEDBOX_ADMIN_PASSWORD` o painel e os formulários das gavetas ficam acessíveis. O usuário pode encerrar a sessão com **Sair da conta**.
7. Cada gaveta tem nome, hora e minuto; alterações são salvas no LittleFS.
8. Quando o horário coincide e o NTP foi sincronizado ao menos uma vez, o OLED mostra a gaveta e o medicamento enquanto o LED pisca por um minuto, **sem som**. Depois retorna automaticamente ao relógio e próximo medicamento. Se mais de uma gaveta tiver o mesmo horário, ambos LEDs piscam (o OLED mostra a primeira gaveta com aviso ativo).
9. **Silenciar LEDs atuais** interrompe os LEDs em andamento sem cancelar próximos horários. **Testar LEDs** aciona sequencialmente os três LEDs por 3 segundos cada e não altera horários.
10. As gavetas começam desativadas (sem medicamentos fictícios); o botão **Limpar** desativa individualmente e persiste a alteração no LittleFS. A interface não confirma tomada do medicamento.

## OLED integrado: atualização automática

A tela SSD1306 128×64 exibe continuamente **hora atual**, **próxima gaveta**, **hora programada**, **tempo restante aproximado**, **nome do medicamento** e **IP**, quando disponíveis. Atualiza aproximadamente uma vez por segundo, sem necessidade de enviar o firmware novamente após cada alteração do site. Ao salvar ou limpar um cadastro, recalcula imediatamente. Durante os 60 segundos do aviso luminoso, o OLED prioriza **HORA DO REMEDIO**, o número da gaveta e o medicamento; ao terminar ou silenciar, volta à tela principal.

Os textos são reduzidos a 21 caracteres por linha e os acentos portugueses são transliterados para a fonte ASCII do display. Sem NTP válido, exibe `Aguardando horario NTP` em vez de inventar a hora. O dispositivo não confirma ingestão de medicamentos nem possui RTC dedicado; em quedas prolongadas de conexão após sincronização, o relógio estimado pode derivar. Uma atualização de hora pelo NTP pode corrigir essa deriva.

## Layout do painel

O HTML/CSS e um script JavaScript curto são servidos diretamente pelo ESP8266, sem hospedagem externa. O layout mantém o cabeçalho azul, menu lateral, relógio sincronizado, próxima dose e três cartões com cores vermelho/azul/verde. Os cartões se reorganizam em tablets e celulares. O JavaScript usa o horário NTP enviado pela placa como referência; não usa relógio local do celular como fonte da hora da medicação. Não há aplicativo nativo.

## Usuários, senhas e segurança

**Preservar no firmware os quatro campos:**

- `MEDBOX_WIFI_SSID`: nome do roteador/hotspot.
- `MEDBOX_WIFI_PASSWORD`: senha do Wi-Fi.
- `MEDBOX_ADMIN_USER`: usuário do painel.
- `MEDBOX_ADMIN_PASSWORD`: senha do painel.

Esses valores ficam no arquivo **local** `firmware/secrets.h`, criado a partir de `firmware/secrets.example.h`. O Git ignora `secrets.h`, mas não protege senhas que sejam copiadas para outros arquivos ou commits.

A autenticação agora é feita por página própria (usuário e senha), seguida de uma **sessão temporária no servidor**. O cookie usa `HttpOnly` e `SameSite=Strict`; formulários de salvar horários, silenciar alarmes e sair exigem um token anti-CSRF. A sessão vence após 30 minutos de inatividade, há uma sessão ativa por vez e o login bloqueia novas tentativas por um minuto depois de cinco falhas consecutivas.

**Limitação essencial:** não há TLS no servidor do ESP8266. Usuário, senha e cookie trafegam por HTTP e podem ser interceptados por quem controla a rede; uma tela de login personalizada não equivale a comunicação criptografada. Mantenha o acesso somente em rede local confiável, não encaminhe a porta 80 para a internet, use credenciais exclusivas e não dependa desta proteção em ambientes hostis.

## Hotspot e medbox.local

- ESP8266 suporta apenas Wi-Fi 2,4 GHz.
- `medbox.local` é nome mDNS anunciado pelo ESP8266, **não um domínio cadastrado na internet**.
- Suporte mDNS varia entre Android, iOS, Windows e roteadores/hotspots; IP numérico é a alternativa.
- Alguns celulares não conseguem acessar clientes do próprio hotspot. Um segundo dispositivo conectado à rede costuma ser a opção mais previsível.
- O hotspot precisa fornecer internet para sincronização NTP.

## Persistência e alertas

Os nomes e horários são gravados no LittleFS e podem sobreviver a reinícios. O estado `ultimoDia` de cada gaveta e o silenciamento do dia ficam apenas na RAM, portanto reiniciar pode permitir novo alerta no mesmo dia. O sistema não detecta abertura das gavetas nem ingestão.

## Limitações e pontos a validar

- O firmware é validado por compilação automatizada ESP8266; a exibição contínua do OLED precisa de validação na placa real.
- O firmware agora tenta iniciar o servidor quando a conexão Wi-Fi aparecer mesmo após falhar no boot; a correção ainda depende de validação na HW-364A.
- A recuperação de mDNS nas reconexões ainda depende do hotspot e precisa de teste físico.
- Sem NTP válido após reinicialização, os alertas por horário não funcionam. Após uma sincronização válida, o horário segue estimado por `millis()` durante interrupções temporárias da rede, sujeito a deriva.
- O fuso UTC-3 é fixo, sem horário de verão automático.
- Configurações padrão são apenas exemplos ilustrativos.

## Próximos testes recomendados

Compilação, testes em ESP8266 com OLED e LEDs, persistência após desligamento, acesso em Android e iPhone via hotspot, comportamento do mDNS, reinício sem Wi-Fi e alertas na virada do dia.

> ⚠️ Protótipo educacional, não é dispositivo médico certificado e não deve ser o único sistema de lembrete.
