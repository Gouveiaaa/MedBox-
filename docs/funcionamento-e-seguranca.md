# Funcionamento, rede, segurança e limitações da MedBox

## Fluxo operacional atual

1. O ESP8266 inicializa periféricos, monta LittleFS e tenta carregar as configurações das três gavetas.
2. Tenta conectar ao Wi-Fi 2,4 GHz configurado no `secrets.h`.
3. Quando conecta durante a inicialização, sincroniza horário com `a.st1.ntp.br` (UTC-3), inicia servidor HTTP e anuncia `medbox.local` via mDNS.
4. Exibe o IP no OLED e imprime no Monitor Serial a 115200 baud.
5. O usuário acessa pelo IP ou por `http://medbox.local` quando a rede suportar mDNS.
6. O painel solicita **usuário e senha**, definidos em `MEDBOX_ADMIN_USER` e `MEDBOX_ADMIN_PASSWORD`.
7. Cada gaveta tem nome, hora e minuto; alterações são salvas no LittleFS.
8. Quando o horário coincide e o NTP está válido, o OLED mostra o aviso e o LED da gaveta pisca por um minuto. Se mais de uma gaveta tiver o mesmo horário, os LEDs correspondentes piscam e o OLED mostra a última gaveta processada.
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

A autenticação HTTP Basic **não criptografa** credenciais, pois o servidor usa HTTP sem TLS. Use somente rede local confiável, não encaminhe a porta 80 para a internet e não use senhas reutilizadas de serviços importantes.

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
- Se o Wi-Fi não conectar nos primeiros 20 segundos de inicialização, o servidor web não é iniciado nessa execução; é preciso reiniciar após disponibilizar a rede.
- O firmware tenta reconectar o Wi-Fi, mas a recuperação completa do mDNS após desconexão ainda não foi validada.
- Sem NTP válido após reinicialização, os alertas por horário não funcionam.
- O fuso UTC-3 é fixo, sem horário de verão automático.
- Configurações padrão são apenas exemplos ilustrativos.

## Próximos testes recomendados

Compilação, testes em ESP8266 com OLED e LEDs, persistência após desligamento, acesso em Android e iPhone via hotspot, comportamento do mDNS, reinício sem Wi-Fi e alertas na virada do dia.

> ⚠️ Protótipo educacional, não é dispositivo médico certificado e não deve ser o único sistema de lembrete.
