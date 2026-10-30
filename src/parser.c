/* Integrante 3: parser estrito, enderecos decimais ou 0x..., comentarios #. */
#include "simulator.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static bool number(const char *str,uint32_t *value) {
    if (!str || !*str || *str=='-' || *str=='+') return false;
    errno=0; char *end;
    int base=str[0]=='0' && (str[1]=='x' || str[1]=='X') ? 16:10;
    unsigned long long n=strtoull(str,&end,base);
    if (errno || *end || n>UINT32_MAX) return false;
    *value=(uint32_t)n; return true;
}
int parse_trace(char *line,trace_t *t) {
    char *comment=strchr(line,'#'); if (comment) *comment='\0';
    char *tokens[6]; unsigned n=0;
    for (char *p=strtok(line," \t\r\n");p;p=strtok(NULL," \t\r\n")) {
        if (n==6) return -1;
        tokens[n++]=p;
    }
    if (!n) return 0;
    if (n!=4 && n!=5) return -1;
    *t=(trace_t){0};
    if (!number(tokens[0],&t->arrival) || !number(tokens[1],&t->pid) ||
        !number(tokens[2],&t->address)) return -1;
    if (strcmp(tokens[3],"R")==0) t->write=false;
    else if (strcmp(tokens[3],"W")==0) t->write=true;
    else return -1;
    uint32_t v,o;
    if (!translate(t->address,&v,&o)) return -1;
    t->has_useful=n==5;
    if (n==5 && (!number(tokens[4],&t->useful) || !t->useful ||
                 t->useful>page_size(v) || o>=t->useful)) return -1;
    return 1;
}
