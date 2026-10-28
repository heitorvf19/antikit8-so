#include "simulator.h"
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
int main(int argc,char **argv) {
    if (argc<2) {
        fprintf(stderr,"Uso: %s TRACE [--naive] [--pause] [--csv ARQUIVO]\n",argv[0]); return 1;
    }
    simulator_t *s=calloc(1,sizeof(*s)); if (!s) die("sem memoria para simulador");
    for (unsigned f=0;f<FRAME_COUNT;++f) s->frames[f].process=-1;
    hardware_init(&s->hw); s->max_temp=BASE_SWAP_TEMP;
    s->chain=UINT64_C(14695981039346656037); srand(1);
    for (int i=2;i<argc;++i) {
        if (!strcmp(argv[i],"--naive")) s->naive=true;
        else if (!strcmp(argv[i],"--pause")) s->pause=true;
        else if (!strcmp(argv[i],"--csv") && i+1<argc) {
            if (s->csv) die("--csv repetido");
            s->csv=fopen(argv[++i],"w"); if (!s->csv) die("nao foi possivel criar CSV");
            fputs("acesso,chegada_ms,execucao_ms,temp_c,temp_max_c,faults,ciclos_io,uteis,alocados,frag_ram_pct\n",s->csv);
        } else die("opcao invalida");
    }
    FILE *in=fopen(argv[1],"r"); if (!in) die("nao foi possivel abrir trace");
    char line[1024]; uint64_t line_no=0; uint32_t last_arrival=0;
    while (fgets(line,sizeof(line),in)) {
        ++line_no;
        if (!strchr(line,'\n') && !feof(in)) die("linha longa demais (max 1022 caracteres)");
        trace_t t; int status=parse_trace(line,&t);
        if (!status) continue;
        if (status<0 || (s->accesses && t.arrival<last_arrival)) {
            fprintf(stderr,"Trace invalido na linha %" PRIu64 "\n",line_no);
            fclose(in); if (s->csv) fclose(s->csv); free(s); return 1;
        }
        last_arrival=t.arrival;
        if (t.arrival>s->now) s->now=t.arrival;
        process_t *p=get_process(s,t.pid);
        uint32_t vpn,offset;
        if (!translate(t.address,&vpn,&offset)) die("endereco fora de 1MiB");
        if (t.has_useful) p->useful[vpn]=t.useful;
        else if (offset>=p->useful[vpn]) die("acesso fora dos bytes uteis declarados anteriormente");
        ++s->accesses; ++p->accesses;
        printf("ACCESS n=%" PRIu64 " chegada=%u execucao=%u pid=%u va=%u VPN=%u Offset=%u op=%c\n",
               s->accesses,t.arrival,s->now,t.pid,t.address,vpn,offset,t.write?'W':'R');
        bool hit=observed_access(s,p,&t);
        if (s->hw.global_access_count%FADING_THRESHOLD==0) aging_tick(s);
        if (!hit) {
            ++s->faults; ++p->faults;
            printf("PAGE_FAULT pid=%u vpn=%u\n",p->pid,vpn);
            make_space(s,vpn);
            int f=find_space(s,vpn); if (f<0) die("alocacao falhou apos substituicao");
            map_page(s,p,vpn,(uint32_t)f);
            if (!observed_access(s,p,&t)) die("retry falhou");
            if (s->hw.global_access_count%FADING_THRESHOLD==0) aging_tick(s);
        }
        printf("PHYSICAL pid=%u vpn=%u pa=%u\n",p->pid,vpn,
               p->pt[vpn].frame_number*PAGE_SIZE_EVEN+offset);
        check_consistency(s); sample_metrics(s,t.arrival);
        if (s->accesses%10==0) draw_ram(s);
    }
    if (ferror(in)) die("falha na leitura do trace");
    fclose(in);
    if (s->accesses%10) draw_ram(s);
    /* Materializa uma unica vez o resfriamento pendente ao encerrar. */
    hardware_cool_down(&s->hw,s->now-s->hw.last_swap_ms);
    summary(s);
    if (s->csv && fclose(s->csv)) die("falha ao gravar CSV");
    free(s); return 0;
}
