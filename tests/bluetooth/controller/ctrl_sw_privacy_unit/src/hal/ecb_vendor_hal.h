/*
 * Copyright (c) 2026 Kent Hauser
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* AES block arbitration for this test (hal/ecb.h): fakes in main.c, so a test can make the block
 * busy and check the caller's id.
 */
#include <stdbool.h>
#include <stdint.h>

bool ecb_ready(uint8_t caller_id);
void ecb_encrypt_from(uint8_t caller_id, uint8_t const *const key_le,
		      uint8_t const *const clear_text_le, uint8_t *const cipher_text_le,
		      uint8_t *const cipher_text_be);
void ecb_encrypt_be_from(uint8_t caller_id, uint8_t const *const key_be,
			 uint8_t const *const clear_text_be, uint8_t *const cipher_text_be);
