# Funcionamento, rede, segurança e limitações da MedBox

## Fluxo operacional atual

1. O ESP8266 inicializa periféricos, monta LittleFS e tenta carregar as configurações das três gavetas.
2. Tenta conectar ao Wi-Fi 2,4 GHz configurado no `secrets.h`.
3. Quando conecta durante a inicialização, sincroniza horário com `a.st1.ntp.br` (UTC-3), inicia servidor HTTP e anuncia `medbox.local` via mDNS.
4. Exibe o IP no OLED e imprime no Monitor Serial a 115200 baud.
5. O usuário acessa pelo IP ou por `http://medbox.local` quando a rede suportar mDNS.
6. A primeira tela é **Entrar no MedBox**. Só depois de autenticar com `MEDBOX_ADMIN_USER` e `MEDBOX_ADMIN_PASSWORD` o painel e os formulários das gavetas ficam acessíveis. O usuário pode encerrar a sessão com **Sair da conta**.
7. Cada gaveta tem nome, hora e minuto; alterações são salvas no LittleFS.
8. Quando o horário coincide e o NTP foi sincronizado ao menos uma vez, o OLED mostra o aviso e o LED da gaveta pisca por um minuto. Um buzzer ativo opcional pode alternar junto aos LEDs após configuração elétrica segura. Se mais de uma gaveta tiver o mesmo horário, os LEDs correspondentes piscam e o OLED mostra a última gaveta processada.
9. O botão **Silenciar alarmes de hoje** apaga os LEDs e desativa os avisos luminosos restantes daquele dia durante a sessão atual.

## Layout do painel

O HTML/CSS é servido diretamente pelo ESP8266, sem hospedagem externa. O painel se ajusta a Android, iOS, tablets e PCs: uma coluna em telas pequenas e múltiplas colunas em telas maiores. Não há aplicativo nativo.

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

- **A compilação para ESP8266 e o teste na placa estão pendentes.**
- O firmware agora tenta iniciar o servidor quando a conexão Wi-Fi aparecer mesmo após falhar no boot; a correção ainda depende de validação na HW-364A.
- A recuperação de mDNS nas reconexões ainda depende do hotspot e precisa de teste físico.
- Sem NTP válido após reinicialização, os alertas por horário não funcionam. Após uma sincronização válida, o horário segue estimado por `millis()` durante interrupções temporárias da rede, sujeito a deriva.
- O fuso UTC-3 é fixo, sem horário de verão automático.
- Configurações padrão são apenas exemplos ilustrativos.

## Próximos testes recomendados

Compilação, testes em ESP8266 com OLED e LEDs, persistência após desligamento, acesso em Android e iPhone via hotspot, comportamento do mDNS, reinício sem Wi-Fi e alertas na virada do dia.

> ⚠️ Protótipo educacional, não é dispositivo médico certificado e não deve ser o único sistema de lembrete.
