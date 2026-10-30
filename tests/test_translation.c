#include "simulator.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
    if (argc>1 && !strcmp(argv[1],"corrupt")) {
        simulator_t *s=calloc(1,sizeof(*s)); assert(s);
        for (unsigned f=0;f<FRAME_COUNT;++f) s->frames[f].process=-1;
        process_t *p=get_process(s,1); map_page(s,p,0,0);
        s->frames[0].vpn=1;
        check_consistency(s); /* deve terminar em erro */
        free(s); return 0;
    }
    page_table_entry_t pt[PAGE_COUNT]={0}; hardware_state_t hw;
    hardware_init(&hw);
    for (unsigned i=0;i<PAGE_COUNT;++i) pt[i].present=true;
    uint32_t expected=0,start=0;
    for (uint32_t a=0;a<VIRTUAL_MEMORY_SIZE;++a) {
        while (a>=start+page_size(expected)) { start+=page_size(expected); ++expected; }
        uint32_t v,o;
        assert(translate(a,&v,&o)); assert(v==expected && o==a-start);
        assert(mmu_access_memory(&hw,pt,PAGE_COUNT,a,true,1));
        assert(pt[v].modified); pt[v].modified=false;
    }
    uint32_t v,o;
    assert(translate(12287,&v,&o) && v==1 && o==8191);
    assert(translate(12288,&v,&o) && v==2 && o==0);
    assert(translate(1048575,&v,&o) && v==170 && o==4095);
    assert(!translate(1048576,&v,&o));
    assert(!translate(UINT32_MAX,&v,&o));
    puts("Traducao exaustiva de 1.048.576 enderecos: OK");
    return 0;
}
