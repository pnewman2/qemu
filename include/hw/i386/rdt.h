#ifndef HW_RDT_H
#define HW_RDT_H

#include "qemu/osdep.h"

typedef struct RDTState RDTState;

uint64_t rdt_read_event_count(RDTState *rdt, uint32_t rmid, uint32_t event_id);
uint32_t rdt_max_rmid(RDTState *rdt);

#endif
