# Verificação executada em 28/10/2026

- Compilação C11 com -Wall -Wextra -Wpedantic -Werror: aprovada.
- make test: aprovado; inclui todos os 1.048.576 endereços válidos e comparação com a MMU.
- ASAN_OPTIONS=detect_leaks=0 make sanitize: aprovado.
- LeakSanitizer padrão: não executado com sucesso; restrição /proc/ptrace do ambiente.
- Hardware fornecido: cópias byte a byte conferidas com os anexos originais.
- Estresse térmico: CSV confirma 262.144 bytes alocados após o acesso 32.
- Dez experimentos executados; métricas efetivas em resumo.csv e resumo.json.
- Relatório: 6 páginas, renderizadas e inspecionadas visualmente.

A detecção de vazamentos permanece pendente de execução em ambiente compatível.
Os resultados térmicos e as sequências de fading dependem do motor e da libc usados.
