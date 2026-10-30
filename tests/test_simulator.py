#!/usr/bin/env python3
"""Testes de propriedades observaveis; nao exige bibliotecas Python externas."""
import os, random, re, shlex, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
os.chdir(ROOT)
def run(path,*args,ok=True):
    r=subprocess.run(['./antikit8',str(path),*args],text=True,capture_output=True)
    assert (r.returncode==0)==ok, (str(path),r.stderr,r.stdout[-1200:])
    assert 'runtime error:' not in r.stderr and 'AddressSanitizer' not in r.stderr, r.stderr
    return r.stdout

def metric(out,name):
    return float(re.search(re.escape(name)+r': ([0-9.]+)',out)[1])

with tempfile.TemporaryDirectory() as folder:
    folder=Path(folder)
    flags=shlex.split(os.environ.get('CFLAGS','-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror'))
    exe=folder/'translation'
    subprocess.run([os.environ.get('CC','cc'),*flags,'-Iinclude','-Ihardware',
                    'tests/test_translation.c','src/memory.o','src/audit.o','hardware/hardware_specs.o','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    r=subprocess.run([str(exe),'corrupt'],capture_output=True,text=True)
    assert r.returncode!=0 and 'RAM e PTE divergentes' in r.stderr
    out=run('traces/limites.txt')
    for text in ('VPN=1 Offset=8191','VPN=2 Offset=0','VPN=170 Offset=4095'):
        assert text in out
    assert metric(out,'Page Faults globais')==3
    assert metric(out,'Chamadas MMU (inclui retries)')==6
    for name in ['termico_sujo','termico_misto','termico_so_escritas']:
        out=run('traces/'+name+'.txt')
        assert metric(out,'Temperatura maxima Swap')<=65
        assert metric(out,'Ciclos E/S')==1000*metric(out,'Page Outs')
        assert metric(out,'Page Faults globais')==42
        assert metric(out,'Ciclos CPU totais')==metric(out,'Ciclos E/S')+84
        if name=='termico_so_escritas': assert metric(out,'Espera termica')>0
    bad=run('traces/termico_sujo.txt','--naive')
    assert metric(bad,'Temperatura maxima Swap')>65
    assert metric(bad,'Ciclos E/S')>16000
    even=run('traces/fragmentacao_par.txt'); odd=run('traces/fragmentacao_impar.txt')
    assert '[UFFF' in even and '[UFFFFFFF' in odd
    assert metric(even,'Fragmentacao media / RAM')==1.171875
    assert metric(odd,'Fragmentacao media / RAM')==2.734375
    fade=run('traces/fading.txt')
    events=re.findall(r'FADING mmu=(\d+)',fade)
    count=int(metric(fade,'Chamadas MMU (inclui retries)'))
    assert list(map(int,events))==list(range(5,count+1,5))
    assert 'efeito=zerado_pelo_hardware' in fade and 'efeito=sorteio_ausente' in fade
    assert len(re.findall(r'FADING[^\n]*\nAUDIT[^\n]*APOS_MMU_COM_FADING',fade))==len(events)
    # Parser: limites, tokens extras, bytes uteis e ordem temporal.
    invalid=['0 1 1048576 R\n','-1 1 0 R\n','0 1 0 X\n','0 1 0 R 0\n',
             '0 1 0 R 4097\n','0 1 1024 R 1024\n','0 1 0 R extra extra\n',
             '2 1 0 R\n1 1 0 R\n','0 1 0 R\n'+'x'*1200+'\n',
             '0 1 0 R 1024\n1 1 1024 R\n','4294967296 1 0 R\n']
    for i,data in enumerate(invalid):
        file=folder/f'invalid{i}.txt'; file.write_text(data); run(file,ok=False)
    empty=folder/'empty.txt'; empty.write_text('# comentario\n\n')
    assert metric(run(empty),'Acessos do trace')==0
    # Mesmo VA em PIDs diferentes deve ocupar quadros distintos.
    file=folder/'pids.txt'; file.write_text('0 7 0 W\n0 999 0 R\n0 7 0 R\n0 999 0 R\n')
    out=run(file); assert metric(out,'Page Faults globais')==2
    assert 'PID 7: acessos=2 faults=1' in out and 'PID 999: acessos=2 faults=1' in out
    # Mistura 4/8KiB com pressao de memoria, escritas e multiplos PIDs.
    rng=random.Random(2026); rows=[]
    for i in range(600):
        v=rng.randrange(171); a=v//2*12288+(4096 if v%2 else 0)
        rows.append(f'{i//50} {rng.randrange(8)} {a} {rng.choice(["R","W"])}')
    file=folder/'random.txt'; file.write_text('\n'.join(rows)+'\n')
    out=run(file)
    assert metric(out,'Acessos do trace')==600 and metric(out,'Temperatura maxima Swap')<=65
print('OK: limites, traducao exaustiva/MMU, corrupcao detectada, termico, fading, fragmentacao, parser e multiprocessos.')
