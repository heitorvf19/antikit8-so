#!/usr/bin/env python3
import csv, json, re, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'resultados'; out.mkdir(exist_ok=True)
experiments=[('sujo_eficiente','termico_sujo',False),('sujo_baseline','termico_sujo',True),
             ('misto_eficiente','termico_misto',False),('misto_baseline','termico_misto',True),
             ('escritas_eficiente','termico_so_escritas',False),('escritas_baseline','termico_so_escritas',True),
             ('fragmentacao_par','fragmentacao_par',False),('fragmentacao_impar','fragmentacao_impar',False),
             ('limites','limites',False),('fading','fading',False)]
summary=[]
for label,trace,naive in experiments:
    cmd=[str(ROOT/'antikit8'),str(ROOT/'traces'/f'{trace}.txt'),'--csv',str(out/f'{label}.csv')]
    if naive: cmd+=['--naive']
    with (out/f'{label}.log').open('w') as log:
        subprocess.run(cmd,stdout=log,check=True)
    log=(out/f'{label}.log').read_text()
    row={'experimento':label}
    for key in ['Acessos do trace','Page Faults globais','Temperatura maxima Swap','Ciclos E/S','Page Outs','Espera termica','Fragmentacao media / RAM']:
        row[key]=float(re.search(re.escape(key)+r': ([0-9.]+)',log)[1])
    summary.append(row)
(out/'resumo.json').write_text(json.dumps(summary,indent=2,ensure_ascii=False)+'\n')
with (out/'resumo.csv').open('w') as f:
    w=csv.DictWriter(f,fieldnames=summary[0].keys()); w.writeheader(); w.writerows(summary)
try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
except ImportError:
    print('CSV/logs prontos. Para graficos: instalar matplotlib e repetir make results.'); raise SystemExit(0)
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':10,'axes.spines.top':False,'axes.spines.right':False})
fig,axes=plt.subplots(1,3,figsize=(14,4.5))
for label,color,title in [('sujo_baseline','#dc6143','Aging sem controle'),('sujo_eficiente','#167d9a','Aging + limpas + espera')]:
    data=list(csv.DictReader((out/f'{label}.csv').open()))
    data=data[31:]
    x=[int(d['acesso']) for d in data]
    axes[0].plot(x,[float(d['temp_c']) for d in data],'-o',color=color,label=title,markersize=4)
    axes[1].plot(x,[float(d['ciclos_io']) for d in data],'-o',color=color,markersize=4)
    axes[2].plot(x,[float(d['execucao_ms']) for d in data],'-o',color=color,markersize=4)
axes[0].axhline(65,color='gray',linestyle='--',label='Limite de 65 °C')
axes[0].set_ylabel('Temperatura do Swap (°C)'); axes[0].legend(fontsize=8)
axes[1].set_ylabel('Ciclos de E/S acumulados'); axes[1].set_yscale('symlog',linthresh=1000)
axes[2].set_ylabel('Tempo de execução (ms)')
for a in axes: a.set_xlabel('Acesso do trace'); a.grid(alpha=.15)
fig.suptitle('RAM inicialmente suja • 10 novas leituras com chegada em 200 ms')
fig.tight_layout(); fig.savefig(out/'comparacao_termica.png',dpi=160); plt.close(fig)
fig,axes=plt.subplots(1,2,figsize=(10,4.5))
for label,color in [('escritas_baseline','#dc6143'),('escritas_eficiente','#167d9a')]:
    d=list(csv.DictReader((out/f'{label}.csv').open()))[32:]
    x=[int(r['acesso']) for r in d]
    axes[0].plot(x,[float(r['temp_c']) for r in d],'-o',color=color,label=label)
    axes[1].plot(x,[float(r['execucao_ms']) for r in d],'-o',color=color)
axes[0].axhline(65,color='gray',linestyle='--'); axes[0].legend(fontsize=8)
axes[0].set_ylabel('Temperatura (°C)'); axes[1].set_ylabel('Tempo de execução (ms)')
for a in axes: a.set_xlabel('Acesso do trace'); a.grid(alpha=.15)
fig.suptitle('Todas as páginas sujas • espera térmica custa tempo virtual')
fig.tight_layout(); fig.savefig(out/'espera_termica.png',dpi=160); plt.close(fig)
fig,axes=plt.subplots(1,2,figsize=(10,4.5))
axes[0].bar(['Par (4 KiB)','Ímpar (8 KiB)'],[75,87.5],color=['#167d9a','#dc6143'])
axes[0].set_ylabel('Desperdício / página alocada (%)'); axes[0].set_ylim(0,100)
axes[1].bar(['Par (4 KiB)','Ímpar (8 KiB)'],[1.171875,2.734375],color=['#167d9a','#dc6143'])
axes[1].set_ylabel('Desperdício / RAM de 256 KiB (%)')
for a,vs in zip(axes,[[75,87.5],[1.171875,2.734375]]):
    for i,v in enumerate(vs): a.annotate(f'{v:g}%',(i,v),xytext=(0,5),textcoords='offset points',ha='center')
    a.grid(axis='y',alpha=.15)
fig.suptitle('1 KiB útil declarado • 10 acessos à mesma página em cada cenário')
fig.tight_layout(); fig.savefig(out/'fragmentacao.png',dpi=160); plt.close(fig)
print('Experimentos, logs, CSVs e tres graficos gerados em resultados/.')
