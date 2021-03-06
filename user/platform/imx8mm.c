/*
 * Copyright 2017 NXP
 * Copyright 2020 Marco Giorgi <giorgi.marco.96@disroot.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "../../include/m4ctrl.h"

static uint32_t *m4rcr;
static uint8_t memory_idx = TCML_IDX;

/* This variable is initialised in m4ctrl.c */
extern int core_id;
extern MEMORY_LOCATION_t target_memory_location;

m4_data m4[M4_CORES_NUM] = {
    { .areas =
	{
		{SRC_ADDR, 0, SRC_MAP_SIZE},
		{OCRAM_ADDR, 0, OCRAM_MAP_SIZE},
		{TCML_ADDR, 0, TCML_MAP_SIZE},
		{DDR_ADDR, 0, DDR_MAP_SIZE}
	}
    },
};

void platform_setup()
{
	alignment_check(&m4[core_id]);
	map_memory(&m4[core_id]);
	m4rcr = m4[core_id].areas[SRC_IDX].vaddr + M4RCR_OFFS / sizeof(*m4rcr);
}

void cleanup()
{
	unmap_memory(&m4[core_id]);
}

void m4_start()
{
	*m4rcr &= (~(1 << M4CR_NON_SCLR_BIT));
}
void m4_stop()
{
	*m4rcr |= (1 << M4CR_NON_SCLR_BIT);
}

static void m4_platform_reset(void)
{
	*m4rcr |= 1 << M4PR_BIT;
}

void m4_reset(void)
{
	*m4rcr |= 1 << M4CR_BIT;
}

static void m4_image_transfer(uint32_t *dst, const char * binary_name)
{
	int fd;
	uint32_t v;
	uint32_t *p;

	if ((fd = open(binary_name, O_RDONLY)) == -1)
	{
		perror("failed to open firmware image");
		exit(EXIT_FAILURE);
	}

	sync();

	p = dst;
	while (read(fd, &v, sizeof(v)) == sizeof(v))
	{
		*p++ = v;
	}


	/* copy initial stack pointer into TCML */
	m4[core_id].areas[TCML_IDX].vaddr[0] =  m4[core_id].areas[memory_idx].vaddr[0];

	/* copy reset vector */
	m4[core_id].areas[TCML_IDX].vaddr[1] =  m4[core_id].areas[memory_idx].vaddr[1];

	sync();

	close(fd);
}

void m4_deploy(char *filename)
{
	m4_platform_reset();
	switch(target_memory_location) {
		case MEM_TCM:
			memory_idx = TCML_IDX;
			break;
		case MEM_DDR:
			memory_idx = DDR_IDX;
			break;
		default:
			memory_idx = TCML_IDX;
			break;
	}
	printf("Deploying '%s' to 0x%x\n",filename, m4[core_id].areas[memory_idx].paddr);
	m4_image_transfer(m4[core_id].areas[memory_idx].vaddr, filename);
	m4_reset();
}
