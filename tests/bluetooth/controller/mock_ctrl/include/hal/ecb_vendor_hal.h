/*
 * Copyright (c) 2026 Kent Hauser
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* AES block arbitration for the controller unit tests (hal/ecb.h): weak defaults in
 * mock_ctrl/src/ecb.c (always ready), which a test can replace with fakes.
 */
#include <stdbool.h>
#include <stdint.h>

bool ecb_ready(uint8_t caller_id);
void ecb_encrypt_from(uint8_t caller_id, uint8_t const *const key_le,
		      uint8_t const *const clear_text_le, uint8_t *const cipher_text_le,
		      uint8_t *const cipher_text_be);
void ecb_encrypt_be_from(uint8_t caller_id, uint8_t const *const key_be,
			 uint8_t const *const clear_text_be, uint8_t *const cipher_text_be);
