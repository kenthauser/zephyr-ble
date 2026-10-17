/*
 * Copyright (c) 2026 Kent Hauser
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* AES block arbitration for the controller unit tests (hal/ecb.h): plain
 * functions, so a test can fake a busy block. The mocks always succeed.
 */
#include <stdbool.h>
#include <stdint.h>

bool ecb_acquire(uint8_t caller_id);
void ecb_release(void);
