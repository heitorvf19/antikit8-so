/* Integrante 3: checksum, observacao do fading, mapas e metricas. */
#include "simulator.h"
#include <stdlib.h>
#include <inttypes.h>

static uint64_t mix(uint64_t h, uint32_t x) {
    for (unsigned b=0;b<4;++b) { h^=(x>>(8*b))&255u; h*=UINT64_C(1099511628211); }
    return h;
}
void audit(simulator_t *s,const char *reason) {
    check_consistency(s);
    uint64_t map=UINT64_C(14695981039346656037), state=map;
    for (size_t i=0;i<s->process_count;++i) {
        process_t *p=&s->processes[i];
        map=mix(map,p->pid); state=mix(state,p->pid);
        for (uint32_t v=0;v<PAGE_COUNT;++v) {
            page_table_entry_t *e=&p->pt[v];
            map=mix(mix(map,e->present),e->frame_number);
            state=mix(mix(mix(mix(mix(state,e->present),e->frame_number),
                                     e->referenced),e->modified),e->last_access_t);
        }
    }
    s->chain=mix(mix(mix(s->chain,(uint32_t)map),(uint32_t)(map>>32)),(uint32_t)++s->audit_seq);
    printf("AUDIT n=%" PRIu64 " motivo=%s map=%016" PRIx64 " estado=%016" PRIx64
           " acumulado=%016" PRIx64 " OK\n",s->audit_seq,reason,map,state,s->chain);
}
/* So hardware_specs.o e compilado com -Drand=antikit_observed_rand.
   Delega exatamente UMA chamada ao rand real e devolve o mesmo resultado.
   Nao altera a sequencia, a tabela ou os arquivos fornecidos. */
static int draw_value;
static bool drew;
int antikit_observed_rand(void) { draw_value=rand(); drew=true; return draw_value; }
bool observed_access(simulator_t *s,process_t *p,trace_t *t) {
    /* Snapshot apenas para diagnostico; nunca alimenta Aging ou substituicao. */
    bool before_r[PAGE_COUNT];
    for (uint32_t v=0;v<PAGE_COUNT;++v) before_r[v]=p->pt[v].referenced;
    drew=false;
    bool hit=mmu_access_memory(&s->hw,p->pt,PAGE_COUNT,t->address,t->write,s->now);
    if (drew) {
        uint32_t v=(uint32_t)draw_value%PAGE_COUNT;
        printf("FADING mmu=%" PRIu64 " pid=%u vpn=%u presente=%d R_antes=%d R_retorno=%d efeito=%s\n",
               s->hw.global_access_count,p->pid,v,p->pt[v].present,before_r[v],p->pt[v].referenced,
               !p->pt[v].present ? "sorteio_ausente" : before_r[v] ? "zerado_pelo_hardware" : "ja_era_zero");
        audit(s,"APOS_MMU_COM_FADING");
    } else audit(s,"APOS_MMU");
    return hit;
}
void fragmentation(simulator_t *s,uint64_t *used,uint64_t *wasted,uint64_t *allocated) {
    *used=0; *allocated=0;
    for (size_t i=0;i<s->process_count;++i) for (uint32_t v=0;v<PAGE_COUNT;++v)
        if (s->processes[i].pt[v].present) {
            *used+=s->processes[i].useful[v]; *allocated+=page_size(v);
        }
    *wasted=*allocated-*used;
}
void draw_ram(simulator_t *s) {
    printf("RAM t=%u; cada caractere=1KiB: U=util F=fragmento .=livre\n",s->now);
    for (uint32_t f=0;f<FRAME_COUNT;++f) {
        if (f%8==0) printf("%06u [",f*PAGE_SIZE_EVEN);
        for (uint32_t k=0;k<4;++k) {
            if (s->frames[f].process<0) { putchar('.'); continue; }
            process_t *p=&s->processes[s->frames[f].process]; uint32_t v=s->frames[f].vpn;
            uint32_t start=(f-p->pt[v].frame_number)*PAGE_SIZE_EVEN+k*1024;
            putchar(start<p->useful[v] ? 'U':'F');
        }
        if (f%8==7) puts("]");
    }
    uint64_t u,w,a; fragmentation(s,&u,&w,&a);
    printf("FRAG uteis=%" PRIu64 " desperdicio=%" PRIu64 " alocados=%" PRIu64
           " RAM=%.4f%% alocada=%.4f%% (U parcial arredondado somente no desenho)\n",
           u,w,a,100.0*(double)w/PHYSICAL_RAM_SIZE,a?100.0*(double)w/(double)a:0.0);
    if (s->pause) { printf("Enter para continuar...\n"); (void)getchar(); }
}
void sample_metrics(simulator_t *s,uint32_t arrival) {
    uint64_t u,w,a; fragmentation(s,&u,&w,&a);
    double frag=100.0*(double)w/PHYSICAL_RAM_SIZE;
    s->frag_sum+=frag;
    /* Temperatura passiva projetada: motor atualiza registrador no proximo out. */
    double temp=s->hw.swap_temperature-0.5*(s->now-s->hw.last_swap_ms);
    if (temp<BASE_SWAP_TEMP) temp=BASE_SWAP_TEMP;
    if (s->csv) fprintf(s->csv,"%" PRIu64 ",%u,%u,%.1f,%.1f,%" PRIu64 ",%" PRIu64
                       ",%" PRIu64 ",%" PRIu64 ",%.8f\n",s->accesses,arrival,s->now,temp,s->max_temp,
                       s->faults,s->io_cycles,u,a,frag);
}
void summary(simulator_t *s) {
    printf("\nRESUMO politica=%s\n",s->naive?"aging_baseline_sem_controle_termico":"aging_limpa_primeiro_termico");
    printf("Acessos do trace: %" PRIu64 "\nChamadas MMU (inclui retries): %" PRIu64 "\n",
           s->accesses,s->hw.global_access_count);
    printf("Page Faults globais: %" PRIu64 "\n",s->faults);
    for (size_t i=0;i<s->process_count;++i) printf("PID %u: acessos=%" PRIu64 " faults=%" PRIu64 "\n",
                   s->processes[i].pid,s->processes[i].accesses,s->processes[i].faults);
    printf("Temperatura maxima Swap: %.1f C\nCiclos E/S: %" PRIu64
           "\nCiclos CPU totais: %" PRIu64 "\nPage Outs: %" PRIu64
           "\nEspera termica: %" PRIu64 " ms\nFragmentacao media / RAM: %.6f%%\n",
           s->max_temp,s->io_cycles,s->hw.total_cpu_cycles,s->page_outs,s->waits,
           s->accesses?s->frag_sum/(double)s->accesses:0.0);
}
