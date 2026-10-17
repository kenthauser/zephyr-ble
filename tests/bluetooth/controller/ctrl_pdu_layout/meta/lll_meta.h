/*
 * Copyright (c) 2026 Kent Hauser
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Test vendor PDU meta data: a stand-in for a radio's own buffer
 * descriptor, with the packet starting at an offset that is deliberately
 * not a multiple of four, so the tests show pdu[] follows the vendor's
 * offset rather than a word boundary. Used only by the *.meta*
 * scenarios; *.meta_default leaves the offsets to the controller's default.
 */

#ifndef TEST_LLL_META_H_
#define TEST_LLL_META_H_

#include <stdint.h>

typedef struct {
	void *next_entry;	/* descriptor link, as a radio queue entry has */
	uint8_t status;
	uint8_t config;
	uint16_t length;
	uint8_t length_prefix;	/* the packet follows this byte */
	uint8_t data[];
} lll_rx_pdu_meta_t;

#if !defined(CONFIG_TEST_PDU_META_DEFAULT_OFFSET)
#define LLL_RX_PDU_META_PDU_OFFSET (offsetof(lll_rx_pdu_meta_t, length_prefix) + 1U)
#endif

typedef struct {
	void *next_entry;
	uint8_t status;
	uint8_t config;
	uint8_t data[];
} lll_tx_pdu_meta_t;

#if !defined(CONFIG_TEST_PDU_META_DEFAULT_OFFSET)
#define LLL_TX_PDU_META_PDU_OFFSET (offsetof(lll_tx_pdu_meta_t, config) + 1U)
#endif

#endif /* TEST_LLL_META_H_ */
