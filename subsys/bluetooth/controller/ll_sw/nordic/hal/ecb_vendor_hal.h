/*
 * Copyright (c) 2026 Kent Hauser
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* The nRF5 ECB AES block is private to the radio: nothing to arbitrate (hal/ecb.h). */
#include <zephyr/sys/util.h>	/* ARG_UNUSED */

static inline bool ecb_acquire(uint8_t caller_id)
{
	ARG_UNUSED(caller_id);

	return true;
}

static inline void ecb_release(void)
{
}
