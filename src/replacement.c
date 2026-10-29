
/* Integrante 2: Aging, selecao de vitimas e controle termico do Swap. */

#include "simulator.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Atualiza o historico Aging de todas as paginas residentes.
 * O bit R ocupa o bit mais significativo do historico.
 */
void aging_tick(simulator_t *s) {
    if (s == NULL) {
        return;
    }

    for (size_t i = 0; i < s->process_count; ++i) {
        process_t *p = &s->processes[i];

        for (uint32_t vpn = 0; vpn < PAGE_COUNT; ++vpn) {
            page_table_entry_t *entry = &p->pt[vpn];

            if (!entry->present) {
                continue;
            }

            p->age[vpn] >>= 1;

            if (entry->referenced) {
                p->age[vpn] |= UINT8_C(0x80);
            }

            entry->referenced = false;
        }
    }
}

/*
 * Aguarda o resfriamento necessario antes de um Page Out.
 * O hardware realiza o resfriamento quando o timestamp muda.
 */
static void thermal_wait(simulator_t *s) {
    const double target_temp = (double)MAX_SAFE_TEMP - 12.0;
    double projected_temp = s->hw.swap_temperature;
    uint32_t elapsed = s->now - s->hw.last_swap_ms;

    if (elapsed > 0) {
        projected_temp -= 0.5 * (double)elapsed;

        if (projected_temp < (double)BASE_SWAP_TEMP) {
            projected_temp = (double)BASE_SWAP_TEMP;
        }
    }

    if (projected_temp > target_temp) {
        uint32_t wait_ms =
            (uint32_t)ceil((projected_temp - target_temp) * 2.0);

        if (wait_ms == 0) {
            wait_ms = 1;
        }

        s->now += wait_ms;
        s->waits += wait_ms;
    } else if (s->now == s->hw.last_swap_ms) {
        ++s->now;
        ++s->waits;
    }
}

/*
 * Executa Page Out de uma pagina modificada antes de remove-la.
 */
static void page_out(simulator_t *s, process_t *p, uint32_t vpn) {
    page_table_entry_t *entry = &p->pt[vpn];

    if (!entry->present || !entry->modified) {
        return;
    }

    if (!s->naive) {
        thermal_wait(s);
    }

    uint64_t cycles_before = s->hw.total_cpu_cycles;

    hardware_trigger_page_out(&s->hw, s->now);

    s->io_cycles += s->hw.total_cpu_cycles - cycles_before;
    ++s->page_outs;

    if (s->hw.swap_temperature > s->max_temp) {
        s->max_temp = s->hw.swap_temperature;
    }

    printf("PAGE_OUT pid=%u vpn=%u temp=%.1f\n",
           p->pid, vpn, s->hw.swap_temperature);

    entry->modified = false;
    audit(s, "PAGE_OUT");
}

/*
 * Escolhe a pagina com menor historico Aging.
 * Empates sao resolvidos pelo acesso mais antigo.
 * No modo normal, paginas limpas tem prioridade.
 */
static process_t *select_victim(simulator_t *s, uint32_t *victim_vpn) {
    process_t *best_process = NULL;
    uint32_t best_vpn = 0;
    uint8_t best_age = UINT8_MAX;
    uint32_t best_access = UINT32_MAX;
    bool best_dirty = true;

    for (size_t i = 0; i < s->process_count; ++i) {
        process_t *p = &s->processes[i];

        for (uint32_t vpn = 0; vpn < PAGE_COUNT; ++vpn) {
            page_table_entry_t *entry = &p->pt[vpn];

            if (!entry->present) {
                continue;
            }

            bool dirty = entry->modified;

            if (!s->naive && best_process != NULL) {
                if (best_dirty == false && dirty) {
                    continue;
                }

                if (best_dirty == true && dirty == false) {
                    best_process = NULL;
                    best_age = UINT8_MAX;
                    best_access = UINT32_MAX;
                }
            }

            if (best_process == NULL ||
                p->age[vpn] < best_age ||
                (p->age[vpn] == best_age &&
                 entry->last_access_t < best_access)) {
                best_process = p;
                best_vpn = vpn;
                best_age = p->age[vpn];
                best_access = entry->last_access_t;
                best_dirty = dirty;
            }
        }
    }

    if (best_process != NULL) {
        *victim_vpn = best_vpn;
    }

    return best_process;
}

/*
 * Libera quadros ate encontrar espaco suficiente e contiguo.
 * Paginas de 8 KB precisam de dois quadros contiguos.
 */
void make_space(simulator_t *s, uint32_t vpn) {
    if (s == NULL || vpn >= PAGE_COUNT) {
        die("argumento invalido em make_space");
    }

    while (find_space(s, vpn) < 0) {
        uint32_t victim_vpn = 0;
        process_t *victim = select_victim(s, &victim_vpn);

        if (victim == NULL) {
            die("nao foi encontrada pagina residente para substituicao");
        }

        page_out(s, victim, victim_vpn);
        unmap_page(s, victim, victim_vpn);
    }
}
