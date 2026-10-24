/*
 * Copyright (c) 2020 Demant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/types.h>
#include <zephyr/ztest.h>

#include <stdio.h>
#include <stdlib.h>

#define CONFIG_BT_CTLR_FILTER_ACCEPT_LIST 1
#define CONFIG_BT_CTLR_PRIVACY 1
#define CONFIG_BT_CTLR_RL_SIZE 8
#define CONFIG_BT_CTLR_FAL_SIZE 8
#define CONFIG_BT_CTLR_RPA_CACHE_SIZE 4
#define CONFIG_BT_CTLR_TRPA_CACHE_SIZE 4
#define CONFIG_BT_CTLR_SW_DEFERRED_PRIVACY 1
#define CONFIG_BT_LOG_LEVEL 1
#define CONFIG_BT_CTLR_ECB_ARBITRATION 1

#include "ll_sw/ull_filter.c"

/*
 * Unit test of SW deferred privacy data structure and related methods
 * Tests the prpa and trpa cache functions (prpa_cache_add, prpa_cache_clear, prpa_cache_find
 * and trpa_cache_add, trpa_cache_clear, trpa_cache_find)
 */

#define BT_ADDR_INIT(P0, P1, P2, P3, P4, P5)                                   \
	(&(bt_addr_t){ { P0, P1, P2, P3, P4, P5 } })

void helper_privacy_clear(void)
{
	zassert_equal(newest_prpa, 0, "");
	for (uint8_t i = 0; i < CONFIG_BT_CTLR_RPA_CACHE_SIZE; i++) {
		zassert_equal(prpa_cache[i].taken, 0U, "");
	}
	zassert_equal(newest_trpa, 0, "");
	for (uint8_t i = 0; i < CONFIG_BT_CTLR_TRPA_CACHE_SIZE; i++) {
		zassert_equal(trpa_cache[i].rl_idx, FILTER_IDX_NONE, "");
	}
}

void helper_prpa_add(int skew)
{
	bt_addr_t a1, a2, a3, a4, a5;
	uint8_t pos, ex_pos;

	bt_addr_copy(&a1, BT_ADDR_INIT(0x12, 0x13, 0x14, 0x15, 0x16, 0x17));
	bt_addr_copy(&a2, BT_ADDR_INIT(0x22, 0x23, 0x24, 0x25, 0x26, 0x27));
	bt_addr_copy(&a3, BT_ADDR_INIT(0x32, 0x33, 0x34, 0x35, 0x36, 0x37));
	bt_addr_copy(&a4, BT_ADDR_INIT(0x42, 0x43, 0x44, 0x45, 0x46, 0x47));
	bt_addr_copy(&a5, BT_ADDR_INIT(0x52, 0x53, 0x54, 0x55, 0x56, 0x57));

	prpa_cache_add(&a1);
	pos = prpa_cache_find(&a1);
	ex_pos = (1 + skew) % CONFIG_BT_CTLR_RPA_CACHE_SIZE;
	zassert_equal(pos, ex_pos, "%d == %d", pos, ex_pos);

	prpa_cache_add(&a2);
	pos = prpa_cache_find(&a2);
	ex_pos = (2 + skew) % CONFIG_BT_CTLR_RPA_CACHE_SIZE;
	zassert_equal(pos, ex_pos, "");

	prpa_cache_add(&a3);
	pos = prpa_cache_find(&a3);
	ex_pos = (3 + skew) % CONFIG_BT_CTLR_RPA_CACHE_SIZE;
	zassert_equal(pos, ex_pos, "");

	/* Adding this should cause wrap around */
	prpa_cache_add(&a4);
	pos = prpa_cache_find(&a4);
	ex_pos = (4 + skew) % CONFIG_BT_CTLR_RPA_CACHE_SIZE;
	zassert_equal(pos, ex_pos, "");

	/* adding this should cause a1 to be dropped */
	prpa_cache_add(&a5);
	pos = prpa_cache_find(&a5);
	ex_pos = (1 + skew) % CONFIG_BT_CTLR_RPA_CACHE_SIZE;
	zassert_equal(pos, ex_pos, "");

	/* check that a1 can no longer be found */
	pos = prpa_cache_find(&a1);
	zassert_equal(pos, FILTER_IDX_NONE, "");
}

void helper_trpa_add(int skew)
{
	bt_addr_t a1, a2, a3, a4, a5;
	uint8_t pos, ex_pos;

	bt_addr_copy(&a1, BT_ADDR_INIT(0x12, 0x13, 0x14, 0x15, 0x16, 0x17));
	bt_addr_copy(&a2, BT_ADDR_INIT(0x22, 0x23, 0x24, 0x25, 0x26, 0x27));
	bt_addr_copy(&a3, BT_ADDR_INIT(0x32, 0x33, 0x34, 0x35, 0x36, 0x37));
	bt_addr_copy(&a4, BT_ADDR_INIT(0x42, 0x43, 0x44, 0x45, 0x46, 0x47));
	bt_addr_copy(&a5, BT_ADDR_INIT(0x52, 0x53, 0x54, 0x55, 0x56, 0x57));

	trpa_cache_add(&a1, 0);
	pos = trpa_cache_find(&a1, 0);
	ex_pos = (1 + skew) % CONFIG_BT_CTLR_TRPA_CACHE_SIZE;
	zassert_equal(pos, ex_pos, "%d == %d", pos, ex_pos);

	trpa_cache_add(&a2, 1);
	pos = trpa_cache_find(&a2, 1);
	ex_pos = (2 + skew) % CONFIG_BT_CTLR_TRPA_CACHE_SIZE;
	zassert_equal(pos, ex_pos, "");

	trpa_cache_add(&a3, 2);
	pos = trpa_cache_find(&a3, 2);
	ex_pos = (3 + skew) % CONFIG_BT_CTLR_TRPA_CACHE_SIZE;
	zassert_equal(pos, ex_pos, "");

	/* Adding this should cause wrap around */
	trpa_cache_add(&a4, 3);
	pos = trpa_cache_find(&a4, 3);
	ex_pos = (4 + skew) % CONFIG_BT_CTLR_TRPA_CACHE_SIZE;
	zassert_equal(pos, ex_pos, "");

	/* adding this should cause a1 to be dropped */
	trpa_cache_add(&a5, 4);
	pos = trpa_cache_find(&a5, 4);
	ex_pos = (1 + skew) % CONFIG_BT_CTLR_TRPA_CACHE_SIZE;
	zassert_equal(pos, ex_pos, "");

	/* check that a1 can no longer be found */
	pos = trpa_cache_find(&a1, 0);
	zassert_equal(pos, FILTER_IDX_NONE, "");
}

/* ah() sample data, Core Vol 3 Part H D.7, little-endian */
static const uint8_t ah_irk[IRK_SIZE] = { 0x9b, 0x7d, 0x39, 0x0a, 0xa6, 0x10, 0x10, 0x34,
					  0x05, 0xad, 0xc8, 0x57, 0xa3, 0x34, 0x02, 0xec };
static const uint8_t ah_m[16] = { 0x94, 0x81, 0x70 };
static const uint8_t ah_aes[16] = { 0xaa, 0xfb, 0x0d, 0xc4, 0xdc, 0x1b, 0x8c, 0xa4,
				    0x11, 0x23, 0xbe, 0x2e, 0xb7, 0x5f, 0x9d, 0x15 };
/* The RPA: hash = ah = 0x0dfbaa, prand = 0x708194 */
static const uint8_t ah_rpa[BDADDR_SIZE] = { 0xaa, 0xfb, 0x0d, 0x94, 0x81, 0x70 };

static struct {
	bool ready;
	uint8_t ready_calls;
	uint8_t ready_caller;
	uint8_t encrypt_calls;
	uint8_t encrypt_caller;
} ecb_test;

bool ecb_ready(uint8_t caller_id)
{
	ecb_test.ready_calls++;
	ecb_test.ready_caller = caller_id;

	return ecb_test.ready;
}

/* The D.7 block for its key and plaintext; any other input gives a block that matches nothing */
void ecb_encrypt_from(uint8_t caller_id, uint8_t const *const key_le,
		      uint8_t const *const clear_text_le, uint8_t *const cipher_text_le,
		      uint8_t *const cipher_text_be)
{
	ARG_UNUSED(cipher_text_be);

	ecb_test.encrypt_calls++;
	ecb_test.encrypt_caller = caller_id;

	if (!memcmp(key_le, ah_irk, sizeof(ah_irk)) && !memcmp(clear_text_le, ah_m, sizeof(ah_m))) {
		memcpy(cipher_text_le, ah_aes, sizeof(ah_aes));
	} else {
		memset(cipher_text_le, 0xff, sizeof(ah_aes));
	}
}

/* Resolving list entry 0 with local IRK irk; address resolution enabled */
static void helper_rl_setup(const uint8_t irk[IRK_SIZE])
{
	memset(rl, 0, sizeof(rl));
	rl[0].taken = 1U;
	rl[0].lirk = 1U;
	memcpy(rl[0].local_irk, irk, IRK_SIZE);
	rl_enable = 1U;
}

static void before(void *data)
{
	ARG_UNUSED(data);

	/* Run before each test - clear the cache so we start fresh each time. */
	prpa_cache_clear();
	trpa_cache_clear();

	memset(&ecb_test, 0, sizeof(ecb_test));
}

ZTEST_SUITE(test_ctrl_sw_privacy_unit, NULL, NULL, before, NULL, NULL);

ZTEST(test_ctrl_sw_privacy_unit, test_privacy_clear)
{
	helper_privacy_clear();
}

ZTEST(test_ctrl_sw_privacy_unit, test_privacy_add)
{
	helper_prpa_add(0);
	helper_trpa_add(0);
}

ZTEST(test_ctrl_sw_privacy_unit, test_privacy_add_stress)
{
	bt_addr_t ar;

	for (uint8_t skew = 0; skew < CONFIG_BT_CTLR_RPA_CACHE_SIZE; skew++) {
		for (uint8_t i = 0; i < skew; i++) {
			bt_addr_copy(&ar,
				     BT_ADDR_INIT(0xde, 0xad, 0xbe,
						  0xef, 0xaa, 0xff));
			prpa_cache_add(&ar);
		}
		helper_prpa_add(skew);
		prpa_cache_clear();
	}

	for (uint8_t skew = 0; skew < CONFIG_BT_CTLR_TRPA_CACHE_SIZE; skew++) {
		for (uint8_t i = 0; i < skew; i++) {
			bt_addr_copy(&ar,
				     BT_ADDR_INIT(0xde, 0xad, 0xbe,
						  0xef, 0xaa, 0xff));
			trpa_cache_add(&ar, 0);
		}
		helper_trpa_add(skew);
		trpa_cache_clear();
	}
}

/* The LLL's TargetA check (ull_filter_lll_rl_addr_resolve()) with the AES block busy: the address
 * is left unresolved, without encrypting, and the peer sends again.
 */
ZTEST(test_ctrl_sw_privacy_unit, test_rl_addr_resolve_aes_busy)
{
	helper_rl_setup(ah_irk);
	ecb_test.ready = false;

	zassert_false(ull_filter_lll_rl_addr_resolve(BT_ADDR_LE_RANDOM, ah_rpa, 0U));
	zassert_equal(ecb_test.ready_calls, 1U);
	zassert_equal(ecb_test.ready_caller, TICKER_USER_ID_LLL);
	zassert_equal(ecb_test.encrypt_calls, 0U);
}

/* The same address with the block free: resolved, the block used once as the LLL */
ZTEST(test_ctrl_sw_privacy_unit, test_rl_addr_resolve)
{
	helper_rl_setup(ah_irk);
	ecb_test.ready = true;

	zassert_true(ull_filter_lll_rl_addr_resolve(BT_ADDR_LE_RANDOM, ah_rpa, 0U));
	zassert_equal(ecb_test.ready_calls, 1U);
	zassert_equal(ecb_test.ready_caller, TICKER_USER_ID_LLL);
	zassert_equal(ecb_test.encrypt_calls, 1U);
	zassert_equal(ecb_test.encrypt_caller, TICKER_USER_ID_LLL);
}

/* Another device's IRK does not resolve the address */
ZTEST(test_ctrl_sw_privacy_unit, test_rl_addr_resolve_other_irk)
{
	uint8_t irk[IRK_SIZE];

	memcpy(irk, ah_irk, sizeof(irk));
	irk[0] ^= 0x01U;
	helper_rl_setup(irk);
	ecb_test.ready = true;

	zassert_false(ull_filter_lll_rl_addr_resolve(BT_ADDR_LE_RANDOM, ah_rpa, 0U));
	zassert_equal(ecb_test.encrypt_calls, 1U);
}
