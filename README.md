# Antikit-8: simulador de gerenciamento de memória

Implementação em C11, com Aging global de 8 bits, preferência por páginas limpas,
controle térmico, paginação assimétrica, auditoria e mapa ASCII. Os dois arquivos
originais do professor em `hardware/` foram preservados byte a byte.

## Começar no Linux, WSL ou ambiente com GCC e Make

```sh
make
./antikit8 memory_trace.txt
make test
make results
```

`memory_trace.txt` é um exemplo criado para o trabalho, não um trace do professor.
O binário é compilado localmente. `make test` requer Python 3 (biblioteca padrão).
`make results` gera CSV, logs e, se matplotlib estiver instalado, três gráficos.
Para executar sem Python, basta `make` e `./antikit8 arquivo.txt`.
No Windows, abra a pasta pelo terminal do WSL com GCC e Make disponíveis.

```sh
./antikit8 traces/limites.txt
./antikit8 traces/termico_sujo.txt --csv eficiente.csv
./antikit8 traces/termico_sujo.txt --naive --csv baseline.csv
./antikit8 traces/termico_so_escritas.txt
./antikit8 traces/fading.txt
./antikit8 traces/fragmentacao_impar.txt --pause
```

`--naive`: Aging sem preferência por páginas limpas e sem espera térmica.
É uma comparação controlada, não uma política recomendada nem seleção aleatória.
`--pause`: aguarda Enter a cada mapa de dez acessos e no mapa final incompleto.

## Formato do trace

```text
# timestamp_ms PID endereço_virtual operação [bytes_úteis_da_página]
0 1 0 R
0 2 0x1000 W 1024
200 1 12287 R
```

- Timestamps inteiros, não negativos e em ordem não decrescente; PIDs uint32.
- Endereço decimal ou hexadecimal com `0x`, de 0 a 1.048.575.
- Operação `R` ou `W` em maiúsculas; espaços ou tabulações; comentários `#`.
- A quinta coluna é uma extensão explícita para medir fragmentação. Declara um
  prefixo útil da página (1 a 4.096 ou 8.192 bytes). O offset acessado deve estar
  dentro dele. Essa declaração persiste após evicção até outra declaração.
- Sem declaração, considera-se a página inteira útil. Contar endereços visitados
  não revela quantos bytes o processo alocou. Não se inventa uso de 1 KiB.
- Linhas em branco e comentários não contam como acessos. Erros de formato,
  endereço, ordem temporal ou uso declarado encerram a execução com erro.
- O parser admite até 256 processos distintos. Não é um limite do chip.
- Para outro formato de trace, adapte `src/parser.c`; o formato oficial não foi fornecido.

## Organização por integrante

| Responsável a preencher | Escopo | Arquivos principais |
|---|---|---|
| Integrante 1: nome/matrícula | Tradução e estruturas da RAM | `src/memory.c` |
| Integrante 2: nome/matrícula | Aging, vítimas e Swap térmico | `src/replacement.c` |
| Integrante 3: nome/matrícula | Parser, auditoria, mapa e métricas | `src/parser.c`, `src/audit.c`, `tests/run_experiments.py` |

`src/main.c` e `include/simulator.h` fazem a integração. `docs/relatorio.pdf` traz
resultados medidos; `docs/roteiro_video.md` distribui uma defesa de cinco minutos.
Cada estudante deve revisar, compreender e registrar suas próprias contribuições
reais. Não foi criado histórico Git nem foram atribuídos commits fictícios.
Faltam preencher identificação do grupo e gravar o vídeo dos três integrantes.

## Decisões importantes

Há 171 PTEs por processo, VPNs 0 a 170. Cada par ocupa 12 KiB.
RAM = 64 quadros de 4 KiB; páginas de 8 KiB ocupam dois quadros contíguos.
`frame_number` é o primeiro quadro. A substituição pode remover mais de uma
página para obter contiguidade; não há compactação automática.

Aging amostra R a cada cinco chamadas da MMU e depois o limpa. O histórico
`age` é legítimo estado do algoritmo; não é uma cópia usada para restaurar R.
O fading ocorre antes dessa amostragem, portanto seus efeitos influenciam Aging.

Um page fault gera uma segunda chamada da MMU após o mapeamento. Assim,
`chamadas_mmu = acessos_trace + faults` para traces válidos, pois cada fault é
resolvido com um retry. O fading segue as chamadas, inclusive as que falham.

O hardware já aplica `hardware_cool_down` dentro do Page Out. Não resfriamos o
mesmo intervalo novamente. Quando necessário, a execução avança no tempo virtual,
sem alterar os timestamps de chegada. A espera fica registrada; não se inventa
uma conversão de milissegundos em ciclos. O motor não cobra Page In: os ciclos de
E/S informados são somente os cobrados pelos Page Outs reais.

Fading: na implementação fornecida, o sorteio ocorre entre todas as VPNs do
processo corrente, inclusive ausentes. O observador de `rand` no Makefile mantém
uma chamada à libc e o mesmo valor retornado, registrando o alvo real. O log e
o checksum são emitidos na volta da MMU; a API não oferece callback entre a
limpeza de R e o restante do acesso. Se o alvo for a própria página acessada,
R pode já estar 1 novamente ao retornar. Isso é comportamento do motor.

`map` cobre present/frame_number (e PID/ordem); `estado` inclui também R, M e
last_access_t; `acumulado` encadeia map e número do evento. FNV-1a de 64 bits é
checksum simples, não hash criptográfico, assinatura ou prova contra adulteração
intencional. A auditoria também verifica sobreposição, limites e correspondência
entre quadros e PTEs. Um checksum de present/frame_number não detecta R isoladamente.

Fragmentação da RAM = 100 × soma(tamanho da página - bytes úteis) / 262.144.
A média final dá peso igual a cada acesso do trace. Não é média por tempo.
O mapa também mostra desperdício / memória alocada. Memória livre não é
fragmentação interna. Cada caractere mostra 1 KiB; U parcial é arredondado
apenas no desenho, enquanto as métricas mantêm precisão de bytes.

## Testes e reprodução

`make test` verifica tradução exaustiva contra a MMU fornecida, casos de limite,
isolamento por PID, temperaturas, custo base, sequência do fading, fragmentação,
parser inválido, corrupção proposital de quadro e pressão aleatória em oito PIDs.
A corrupção proposital deve ser rejeitada; não modifica o código entregue.

```sh
make sanitize
# Se LeakSanitizer não puder acessar /proc no seu ambiente:
ASAN_OPTIONS=detect_leaks=0 make sanitize
# Voltar à compilação normal depois:
make clean
make
```

Nesta execução, ASan/UBSan passaram com detecção de vazamentos desabilitada.
LeakSanitizer falhou por restrição a /proc/ptrace do ambiente; não se declara um
resultado de ausência de vazamentos que ele não verificou. Na execução normal,
a única alocação dinâmica do simulador é liberada ao terminar. `die()` encerra o
processo imediatamente em erros fatais, delegando a liberação ao sistema.

A semente é `srand(1)`. Sequências de `rand()` podem variar entre bibliotecas C;
os resultados e logs incluídos correspondem ao ambiente de geração do pacote.
A baseline interrompe antes do deslocamento de int indefinido em temperaturas
extremas do motor original. Os dez Page Outs de teste não atingem esse limite.

## Artefatos

- `docs/relatorio.pdf`: relatório com arquitetura, métricas, gráficos e ressalvas.
- `docs/roteiro_video.md`: demonstração de cinco minutos, sem vídeo fictício.
- `resultados/`: dez logs/CSVs, resumo tabular e três PNGs.
- `tests/`: testes e reprodução das análises.
- `hardware/SHA256SUMS`: integridade dos arquivos fornecidos.

Referência conceitual indicada no enunciado: Andrew S. Tanenbaum,
*Sistemas Operacionais Modernos*, capítulo 3 (Memória). Edição e páginas não
foram informadas. O comportamento efetivamente implementado segue os dois
arquivos de hardware anexados e as premissas documentadas aqui.
