/*
 * Copyright (c) 2021 Demant
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/types.h>
#include <zephyr/ztest.h>
#include "hal/ecb.h"

__attribute__((weak)) void ecb_encrypt(uint8_t const *const key_le,
				       uint8_t const *const clear_text_le,
				       uint8_t *const cipher_text_le, uint8_t *const cipher_text_be)
{
}

__attribute__((weak)) bool ecb_ready(uint8_t caller_id)
{
	ARG_UNUSED(caller_id);

	return true;
}

__attribute__((weak)) void ecb_encrypt_from(uint8_t caller_id, uint8_t const *const key_le,
					    uint8_t const *const clear_text_le,
					    uint8_t *const cipher_text_le,
					    uint8_t *const cipher_text_be)
{
	ARG_UNUSED(caller_id);

	ecb_encrypt(key_le, clear_text_le, cipher_text_le, cipher_text_be);
}

__attribute__((weak)) void ecb_encrypt_be_from(uint8_t caller_id, uint8_t const *const key_be,
					       uint8_t const *const clear_text_be,
					       uint8_t *const cipher_text_be)
{
}
