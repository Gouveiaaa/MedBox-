# Funcionamento e segurança

## Fluxo
1. O ESP8266 conecta ao Wi-Fi configurado.
2. Obtém horário pelo servidor NTP `a.st1.ntp.br` (UTC-3).
3. Exibe IP no OLED e disponibiliza painel web na rede local.
4. O administrador define nome e horário de cada uma das três gavetas.
5. Quando o horário coincide, o OLED mostra a gaveta e o medicamento, e o buzzer e o LED sinalizam.

## Segurança
- Nunca publique senhas reais em código, README, imagens ou logs.
- Configure credenciais no arquivo `secrets.h`, excluído pelo `.gitignore`.
- **HTTP Basic não criptografa senha nem tráfego.** Use apenas Wi-Fi confiável, sem exposição pública ou port forwarding.
- Troque as credenciais anteriormente compartilhadas antes de usar o equipamento.
- Valide entrada do usuário e escape conteúdo HTML para reduzir riscos de injeção.
- O painel web não deve ser usado em redes compartilhadas ou hostis.

## Limitações operacionais
- O sistema não detecta se o medicamento foi retirado ou ingerido.
- Um horário perdido durante falta de energia ou rede não é recuperado automaticamente.
- Alterações não sobrevivem a reinício nesta versão.
- O fuso UTC-3 é fixo.
- Alertas dependem do funcionamento do microcontrolador e dos periféricos.

## Próximas melhorias
Persistência em LittleFS, modo de configuração inicial, watchdog operacional, sincronização e recuperação de horários, autenticação mais robusta, testes de hardware e confirmação física de gaveta.
