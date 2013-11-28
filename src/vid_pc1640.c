/*PC1640 video emulation.
  Mostly standard EGA, but with CGA & Hercules emulation*/
#include <stdlib.h>
#include "ibm.h"
#include "device.h"
#include "io.h"
#include "mem.h"
#include "timer.h"
#include "video.h"
#include "vid_cga.h"
#include "vid_ega.h"
#include "vid_pc1640.h"

typedef struct pc1640_t
{
        mem_mapping_t cga_mapping;
        mem_mapping_t ega_mapping;

        cga_t cga;
        ega_t ega;
        
        int cga_enabled;
        int dispontime, dispofftime, vidtime;
} pc1640_t;

void pc1640_out(uint16_t addr, uint8_t val, void *p)
{
        pc1640_t *pc1640 = (pc1640_t *)p;
        
        switch (addr)
        {
                case 0x3db:
                pc1640->cga_enabled = val & 0x40;
                if (pc1640->cga_enabled)
                {
                        mem_mapping_enable(&pc1640->cga_mapping);
                        mem_mapping_disable(&pc1640->ega_mapping);
                }
                else
                {
                        mem_mapping_disable(&pc1640->cga_mapping);
                        switch (pc1640->ega.gdcreg[6] & 0xc)
                        {
                                case 0x0: /*128k at A0000*/
                                mem_mapping_set_addr(&pc1640->ega_mapping, 0xa0000, 0x20000);
                                break;
                                case 0x4: /*64k at A0000*/
                                mem_mapping_set_addr(&pc1640->ega_mapping, 0xa0000, 0x10000);
                                break;
                                case 0x8: /*32k at B0000*/
                                mem_mapping_set_addr(&pc1640->ega_mapping, 0xb0000, 0x08000);
                                break;
                                case 0xC: /*32k at B8000*/
                                mem_mapping_set_addr(&pc1640->ega_mapping, 0xb8000, 0x08000);
                                break;
                        }
                }                
                pclog("3DB write %02X\n", val);
                return;
        }
        if (pc1640->cga_enabled) cga_out(addr, val, &pc1640->cga);
        else                     ega_out(addr, val, &pc1640->ega);
}

uint8_t pc1640_in(uint16_t addr, void *p)
{
        pc1640_t *pc1640 = (pc1640_t *)p;
        
        switch (addr)
        {
        }
        
        if (pc1640->cga_enabled) return cga_in(addr, &pc1640->cga);
        else                     return ega_in(addr, &pc1640->ega);
}

void pc1640_recalctimings(pc1640_t *pc1640)
{
        if (pc1640->cga_enabled) 
        {
                cga_recalctimings(&pc1640->cga);
                pc1640->dispontime  = pc1640->cga.dispontime;
                pc1640->dispofftime = pc1640->cga.dispofftime;
        }
        else            
        {
                ega_recalctimings(&pc1640->ega);
                pc1640->dispontime  = pc1640->ega.dispontime;
                pc1640->dispofftime = pc1640->ega.dispofftime;
        }
}

void pc1640_poll(void *p)
{
        pc1640_t *pc1640 = (pc1640_t *)p;
        if (pc1640->cga_enabled) 
        {
                pc1640->cga.vidtime = pc1640->vidtime;
                cga_poll(&pc1640->cga);
                pc1640->vidtime = pc1640->cga.vidtime;
        }
        else                     
        {
                pc1640->ega.vidtime = pc1640->vidtime;
                ega_poll(&pc1640->ega);
                pc1640->vidtime = pc1640->ega.vidtime;
        }
}

void *pc1640_init()
{
        pc1640_t *pc1640 = malloc(sizeof(pc1640_t));
        cga_t *cga = &pc1640->cga;
        ega_t *ega = &pc1640->ega;
        memset(pc1640, 0, sizeof(pc1640_t));

        ega_init(&pc1640->ega);
        pc1640->cga.vram = pc1640->ega.vram;
        pc1640->cga_enabled = 1;
        cga_init(&pc1640->cga);
                        
        timer_add(pc1640_poll, &pc1640->vidtime, TIMER_ALWAYS_ENABLED, pc1640);
        mem_mapping_add(&pc1640->cga_mapping, 0xb8000, 0x08000, cga_read, NULL, NULL, cga_write, NULL, NULL,  cga);
        mem_mapping_add(&pc1640->ega_mapping, 0,       0,       ega_read, NULL, NULL, ega_write, NULL, NULL,  ega);
        io_sethandler(0x03a0, 0x0040, pc1640_in, NULL, NULL, pc1640_out, NULL, NULL, pc1640);
        return cga;
}

void pc1640_close(void *p)
{
        pc1640_t *pc1640 = (pc1640_t *)p;

        free(pc1640->ega.vram);
        free(pc1640);
}

void pc1640_speed_changed(void *p)
{
        pc1640_t *pc1640 = (pc1640_t *)p;
        
        pc1640_recalctimings(pc1640);
}

device_t pc1640_device =
{
        "Amstrad PC1640 (video)",
        0,
        pc1640_init,
        pc1640_close,
        NULL,
        pc1640_speed_changed,
        NULL,
        NULL
};
