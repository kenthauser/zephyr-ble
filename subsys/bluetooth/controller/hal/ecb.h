/*
 * Copyright (c) 2016-2024 Nordic Semiconductor ASA
 * Copyright (c) 2016 Vinayak Kariappa Chettimada
 *
 * SPDX-License-Identifier: Apache-2.0
 */

typedef void (*ecb_fp) (uint32_t status, uint8_t *cipher_be, void *context);

struct ecb {
	uint8_t in_key_be[16];
	uint8_t in_clear_text_be[16];
	uint8_t out_cipher_text_be[16];

	/* if not null reverse copy into in_key_be */
	uint8_t *in_key_le;
	/* if not null reverse copy into in_clear_text_be */
	uint8_t *in_clear_text_le;

	ecb_fp  fp_ecb;
	void    *context;
};

void ecb_encrypt_be(uint8_t const *const key_be, uint8_t const *const clear_text_be,
		    uint8_t * const cipher_text_be);
void ecb_encrypt(uint8_t const *const key_le, uint8_t const *const clear_text_le,
		 uint8_t * const cipher_text_le, uint8_t * const cipher_text_be);

void ecb_encrypt_nonblocking(struct ecb *e);

int ecb_ut(void);

/* The AES block by calling context. caller_id is the caller's TICKER_USER_ID_*, known at each call
 * site. A vendor whose block is shared (CONFIG_BT_CTLR_ECB_ARBITRATION) arbitrates it in
 * hal/ecb_vendor_hal.h, which declares the three below or defines them static inline; without it
 * they are ecb_encrypt(), ecb_encrypt_be() and true. The context is passed, not detected:
 * k_is_in_isr() cannot tell the LLL from a thread where the controller's levels run as meta-IRQ
 * threads.
 *
 *   bool ecb_ready(uint8_t caller_id);
 *
 * Whether ecb_encrypt_from() can run now. Always true for a thread (TICKER_USER_ID_THREAD); a
 * controller context gets false while another user holds the block, and retries later
 * (connection: next event; LLCP: re-run; RPA resolution: unresolved this time).
 *
 *   void ecb_encrypt_from(uint8_t caller_id, ...);
 *   void ecb_encrypt_be_from(uint8_t caller_id, ...);
 *
 * ecb_encrypt() and ecb_encrypt_be() from caller_id; they always complete.
 */

#if defined(CONFIG_BT_CTLR_ECB_ARBITRATION)
#include "hal/ecb_vendor_hal.h"
#else /* !CONFIG_BT_CTLR_ECB_ARBITRATION */
#include <zephyr/sys/util.h>	/* ARG_UNUSED */

static inline bool ecb_ready(uint8_t caller_id)
{
	ARG_UNUSED(caller_id);
	return true;
}

static inline void ecb_encrypt_from(uint8_t caller_id, uint8_t const *const key_le,
				    uint8_t const *const clear_text_le,
				    uint8_t * const cipher_text_le, uint8_t * const cipher_text_be)
{
	ARG_UNUSED(caller_id);
	ecb_encrypt(key_le, clear_text_le, cipher_text_le, cipher_text_be);
}

static inline void ecb_encrypt_be_from(uint8_t caller_id, uint8_t const *const key_be,
				       uint8_t const *const clear_text_be,
				       uint8_t * const cipher_text_be)
{
	ARG_UNUSED(caller_id);
	ecb_encrypt_be(key_be, clear_text_be, cipher_text_be);
}
#endif /* !CONFIG_BT_CTLR_ECB_ARBITRATION */
