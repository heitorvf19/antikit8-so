#ifndef SIMULATOR_H
#define SIMULATOR_H
#include "hardware_specs.h"
#include <stdio.h>
#include <stddef.h>
#define PAGE_COUNT 171u
#define FRAME_COUNT (PHYSICAL_RAM_SIZE / PAGE_SIZE_EVEN)
#define NO_FRAME UINT32_MAX
/* Limite de processos do simulador, nao do hardware; PIDs podem ser esparsos. */
#define MAX_PROCESSES 256u
typedef struct {
    uint32_t pid;
    page_table_entry_t pt[PAGE_COUNT]; /* unica tabela autoritativa */
    uint8_t age[PAGE_COUNT]; /* historico do Aging, nao copia de bits R/M */
    uint32_t useful[PAGE_COUNT]; /* declaracao do trace, nao inferida de acessos */
    uint64_t faults, accesses;
} process_t;
typedef struct { int process; uint32_t vpn; } frame_t;
typedef struct {
    uint32_t arrival, pid, address, useful;
    bool write, has_useful;
} trace_t;
typedef struct {
    hardware_state_t hw;
    process_t processes[MAX_PROCESSES];
    size_t process_count;
    frame_t frames[FRAME_COUNT];
    uint32_t now;
    uint64_t accesses, faults, io_cycles, page_outs, waits, audit_seq, chain;
    double frag_sum, max_temp;
    bool naive, pause;
    FILE *csv;
} simulator_t;
void die(const char *message);
bool translate(uint32_t address, uint32_t *vpn, uint32_t *offset);
uint32_t page_size(uint32_t vpn);
process_t *get_process(simulator_t *s, uint32_t pid);
int find_space(simulator_t *s, uint32_t vpn);
void map_page(simulator_t *s, process_t *p, uint32_t vpn, uint32_t frame);
void unmap_page(simulator_t *s, process_t *p, uint32_t vpn);
void check_consistency(simulator_t *s);
void aging_tick(simulator_t *s);
void make_space(simulator_t *s, uint32_t vpn);
int parse_trace(char *line, trace_t *t);
void audit(simulator_t *s, const char *reason);
void fragmentation(simulator_t *s, uint64_t *used, uint64_t *wasted, uint64_t *allocated);
void draw_ram(simulator_t *s);
void sample_metrics(simulator_t *s, uint32_t arrival);
void summary(simulator_t *s);
bool observed_access(simulator_t *s, process_t *p, trace_t *t);
int antikit_observed_rand(void);
#endif
