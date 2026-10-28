/* Integrante 1: traducao, processos e alocacao fisica. */
#include "simulator.h"
#include <stdlib.h>
#include <string.h>

void die(const char *message) { fprintf(stderr, "ERRO: %s\n", message); exit(EXIT_FAILURE); }
uint32_t page_size(uint32_t vpn) { return vpn % 2 ? PAGE_SIZE_ODD : PAGE_SIZE_EVEN; }
bool translate(uint32_t a, uint32_t *vpn, uint32_t *offset) {
    if (a >= VIRTUAL_MEMORY_SIZE) return false;
    uint32_t pair = a / (PAGE_SIZE_EVEN + PAGE_SIZE_ODD);
    uint32_t rest = a % (PAGE_SIZE_EVEN + PAGE_SIZE_ODD);
    *vpn = 2 * pair + (rest >= PAGE_SIZE_EVEN);
    *offset = rest >= PAGE_SIZE_EVEN ? rest - PAGE_SIZE_EVEN : rest;
    return *vpn < PAGE_COUNT;
}
process_t *get_process(simulator_t *s, uint32_t pid) {
    for (size_t i=0; i<s->process_count; ++i)
        if (s->processes[i].pid == pid) return &s->processes[i];
    if (s->process_count == MAX_PROCESSES) die("limite de 256 processos excedido");
    process_t *p = &s->processes[s->process_count++];
    p->pid = pid;
    for (uint32_t v=0; v<PAGE_COUNT; ++v) {
        p->pt[v].frame_number = NO_FRAME;
        p->useful[v] = page_size(v);
    }
    audit(s, "CRIAR_TABELA");
    return p;
}
int find_space(simulator_t *s, uint32_t vpn) {
    uint32_t n=page_size(vpn)/PAGE_SIZE_EVEN;
    for (uint32_t f=0; f+n<=FRAME_COUNT; ++f) {
        bool free=true;
        for (uint32_t k=0; k<n; ++k) if (s->frames[f+k].process != -1) free=false;
        if (free) return (int)f;
    }
    return -1;
}
void map_page(simulator_t *s, process_t *p, uint32_t v, uint32_t f) {
    size_t pi=(size_t)(p-s->processes);
    uint32_t n=page_size(v)/PAGE_SIZE_EVEN;
    if (p->pt[v].present || f+n>FRAME_COUNT) die("mapeamento invalido");
    for (uint32_t k=0; k<n; ++k) {
        if (s->frames[f+k].process != -1) die("sobreposicao de quadros");
        s->frames[f+k] = (frame_t){(int)pi,v};
    }
    p->pt[v] = (page_table_entry_t){.frame_number=f,.present=true};
    p->age[v]=0;
    printf("MAP pid=%u vpn=%u frame=%u quadros=%u\n",p->pid,v,f,n);
    audit(s,"MAPEAR");
}
void unmap_page(simulator_t *s, process_t *p, uint32_t v) {
    page_table_entry_t *e=&p->pt[v];
    if (!e->present || e->modified) die("tentativa de remover pagina ausente/suja");
    uint32_t n=page_size(v)/PAGE_SIZE_EVEN;
    for (uint32_t k=0; k<n; ++k) s->frames[e->frame_number+k].process=-1;
    *e=(page_table_entry_t){.frame_number=NO_FRAME};
    p->age[v]=0;
    audit(s,"DESMAPEAR");
}
void check_consistency(simulator_t *s) {
    unsigned seen[FRAME_COUNT]={0};
    for (size_t i=0;i<s->process_count;++i) for (uint32_t v=0;v<PAGE_COUNT;++v) {
        page_table_entry_t *e=&s->processes[i].pt[v];
        if (!e->present) {
            if (e->frame_number != NO_FRAME) die("auditoria: frame em pagina ausente");
            continue;
        }
        uint32_t n=page_size(v)/PAGE_SIZE_EVEN;
        if (e->frame_number>FRAME_COUNT-n) die("auditoria: frame fora da RAM");
        for (uint32_t k=0;k<n;++k) {
            uint32_t f=e->frame_number+k;
            if (++seen[f]!=1 || s->frames[f].process!=(int)i || s->frames[f].vpn!=v)
                die("auditoria: RAM e PTE divergentes");
        }
    }
    for (uint32_t f=0;f<FRAME_COUNT;++f)
        if ((s->frames[f].process!=-1) != (seen[f]==1)) die("auditoria: quadro orfao");
}
