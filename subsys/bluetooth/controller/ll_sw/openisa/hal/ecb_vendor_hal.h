/*
 * Copyright (c) 2026 Kent Hauser
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* The RV32M1 CAU3 is driven without synchronization today; these no-ops
 * keep that behavior (hal/ecb.h).
 */
#include <zephyr/sys/util.h>	/* ARG_UNUSED */

static inline bool ecb_acquire(uint8_t caller_id)
{
	ARG_UNUSED(caller_id);

	return true;
}

static inline void ecb_release(void)
{
}
