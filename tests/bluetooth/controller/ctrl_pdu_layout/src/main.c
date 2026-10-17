/*
 * Copyright (c) 2026 Kent Hauser
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include <zephyr/types.h>
#include <zephyr/ztest.h>

#include "hal/cpu.h"
#include "util/util.h"
#include "util/memq.h"
#include "pdu_df.h"
#include "lll/pdu_vendor.h"
#include "pdu.h"
#include "lll.h"

/* struct node_rx_pdu exactly as v4.5.0 lays it out without vendor meta data: the reference the
 * default layout must reproduce.
 */
struct node_rx_pdu_v4_5 {
	struct node_rx_hdr hdr;
	union {
		struct node_rx_ftr rx_ftr;
#if defined(CONFIG_BT_CTLR_SYNC_ISO) || defined(CONFIG_BT_CTLR_CONN_ISO)
		struct node_rx_iso_meta rx_iso_meta;
#endif
	};
	union {
		uint8_t pdu[0] __aligned(4);
	};
};

#if !defined(CONFIG_BT_CTLR_RX_PDU_META)
ZTEST(pdu_layout, test_rx_layout)
{
	/* Without vendor meta data the node is exactly the reference. */
	zassert_equal(offsetof(struct node_rx_pdu, pdu), offsetof(struct node_rx_pdu_v4_5, pdu));
	zassert_equal(offsetof(struct node_rx_pdu, pdu) % 4U, 0U);
	zassert_equal(sizeof(struct node_rx_pdu), sizeof(struct node_rx_pdu_v4_5));
}
#endif /* !CONFIG_BT_CTLR_RX_PDU_META */

ZTEST_SUITE(pdu_layout, NULL, NULL, NULL, NULL, NULL);
