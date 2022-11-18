#include "qemu/osdep.h"
#include "hw/i386/rdt.h"
#include "hw/qdev-properties.h"
#include "qom/object.h"
#include "target/i386/cpu.h"
#include "hw/isa/isa.h"

#define TYPE_RDT "rdt"
#define RDT_NUM_RMID_PROP "rmids"

#define QM_CTR_Error        (1ULL << 63)
#define QM_CTR_Unavailable  (1ULL << 62)

typedef struct RDTMonitor RDTMonitor;

struct RDTMonitor {
    uint64_t count;
};

struct RDTState {
    ISADevice parent;
    uint32_t rmids;
    GArray *monitors;

    PortioList test_io;
};

struct RDTStateClass { };

OBJECT_DECLARE_TYPE(RDTState, RDTStateClass, RDT);

uint32_t rdt_max_rmid(RDTState *rdt)
{
    return rdt->rmids - 1;
}

uint64_t rdt_read_event_count(RDTState *rdt, uint32_t rmid, uint32_t event_id)
{
    RDTMonitor *mon;

    if (!rdt) {
        return 0;
    }

    if (rmid >= rdt->monitors->len) {
        return QM_CTR_Error;
    }

    mon = &g_array_index(rdt->monitors, RDTMonitor, rmid);

    if (event_id != 2) {
        return QM_CTR_Error;
    }

    return mon->count == 0 ? QM_CTR_Unavailable : mon->count;
}

OBJECT_DEFINE_TYPE(RDTState, rdt, RDT, DEVICE);

static Property rdt_properties[] = {
    DEFINE_PROP_UINT32(RDT_NUM_RMID_PROP, RDTState, rmids, 256),
    DEFINE_PROP_END_OF_LIST(),
};

static void rdt_count_chunk(void *opaque, uint32_t addr, uint32_t val)
{
    RDTState *rdt = opaque;
    X86CPU *cpu = X86_CPU(current_cpu);
    uint32_t rmid = cpu->env.msr_ia32_pqr_assoc & 0xff;
    if (rmid < rdt->monitors->len) {
        RDTMonitor *mon = &g_array_index(rdt->monitors, RDTMonitor, rmid);
        mon->count++;
    }
}

static void rdt_init(Object *obj)
{
    RDTState *rdt = RDT(obj);
    rdt->monitors = g_array_new(false, true, sizeof(RDTMonitor));
}

static const MemoryRegionPortio rdt_test_port_list[] = {
    { 0xc, 4, 4, .write = rdt_count_chunk },
};

static void rdt_realize(DeviceState *d, Error **errp)
{
    RDTState *rdt = RDT(d);
    CPUState *cs;

    g_array_set_size(rdt->monitors, rdt->rmids);

    for (cs = first_cpu; cs; cs = CPU_NEXT(cs)) {
        X86CPU *cpu = X86_CPU(cs);
        cpu->rdt = rdt;
    }

    isa_register_portio_list(&rdt->parent, &rdt->test_io, 0xd00,
                             rdt_test_port_list, rdt, "one_chunk");
}

static void rdt_finalize(Object *obj)
{
    RDTState *rdt = RDT(obj);
    g_array_free(rdt->monitors, true);
}

static void rdt_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->hotpluggable = false;
    dc->desc = "RDT";
    dc->user_creatable = true;
    dc->realize = rdt_realize;

    device_class_set_props(dc, rdt_properties);
}
