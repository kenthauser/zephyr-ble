/*
 * Copyright (c) 2026 Kent Hauser
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include <zephyr/types.h>
#include <zephyr/ztest.h>

#include "hal/cpu.h"
#include "hal/ccm.h"
#include "util/util.h"
#include "util/memq.h"
#include "pdu_df.h"
#include "lll/pdu_vendor.h"
#include "pdu.h"
#include "lll.h"
#include "lll_conn.h"

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

/* struct node_tx exactly as v4.5.0 lays it out: the reference the default layout must reproduce. */
struct node_tx_v4_5 {
	union {
		void        *next;
		memq_link_t *link;
	};

	uint8_t pdu[];
};

#if !defined(CONFIG_BT_CTLR_TX_PDU_META)
ZTEST(pdu_layout, test_tx_layout)
{
	zassert_equal(offsetof(struct node_tx, pdu), offsetof(struct node_tx_v4_5, pdu));
	zassert_equal(offsetof(struct node_tx, pdu) % 4U, 0U);
	zassert_equal(sizeof(struct node_tx), sizeof(struct node_tx_v4_5));
}
#endif /* !CONFIG_BT_CTLR_TX_PDU_META */

#if defined(CONFIG_BT_CTLR_RX_PDU_META)
ZTEST(pdu_layout, test_rx_meta_overlay)
{
	size_t meta = offsetof(struct node_rx_pdu, rx_pdu_meta);
	size_t got = offsetof(struct node_rx_pdu, pdu);

	/* The vendor meta data takes the reference's pdu[] position, on a word boundary... */
	zassert_equal(meta, offsetof(struct node_rx_pdu_v4_5, pdu), "meta data at %zu", meta);
	zassert_equal(meta % 4U, 0U, "meta data at %zu", meta);

	/* ...and pdu[] sits at the vendor's offset inside it, which need not be a word boundary. */
	zassert_equal(got, meta + LLL_RX_PDU_META_PDU_OFFSET,
		      "pdu[] at %zu, meta data at %zu, offset %zu", got, meta,
		      (size_t)LLL_RX_PDU_META_PDU_OFFSET);
#if defined(CONFIG_TEST_PDU_META_DEFAULT_OFFSET)
	/* No vendor offset: pdu[] follows the meta data. */
	zassert_equal(LLL_RX_PDU_META_PDU_OFFSET, sizeof(lll_rx_pdu_meta_t));
#else
	zassert_not_equal(LLL_RX_PDU_META_PDU_OFFSET % 4U, 0U,
			  "test offset should not be word aligned");
#endif

	/* The whole descriptor fits in the node. */
	zassert_true(sizeof(struct node_rx_pdu) >= meta + sizeof(lll_rx_pdu_meta_t));
}
#endif /* CONFIG_BT_CTLR_RX_PDU_META */

#if defined(CONFIG_BT_CTLR_TX_PDU_META)
ZTEST(pdu_layout, test_tx_meta_overlay)
{
	size_t meta = offsetof(struct node_tx, tx_pdu_meta);
	size_t got = offsetof(struct node_tx, pdu);

	zassert_equal(meta, offsetof(struct node_tx_v4_5, pdu), "meta data at %zu", meta);
	zassert_equal(meta % 4U, 0U, "meta data at %zu", meta);
	zassert_equal(got, meta + LLL_TX_PDU_META_PDU_OFFSET,
		      "pdu[] at %zu, meta data at %zu, offset %zu", got, meta,
		      (size_t)LLL_TX_PDU_META_PDU_OFFSET);
#if defined(CONFIG_TEST_PDU_META_DEFAULT_OFFSET)
	zassert_equal(LLL_TX_PDU_META_PDU_OFFSET, sizeof(lll_tx_pdu_meta_t));
#else
	zassert_not_equal(LLL_TX_PDU_META_PDU_OFFSET % 4U, 0U,
			  "test offset should not be word aligned");
#endif
	zassert_true(sizeof(struct node_tx) >= meta + sizeof(lll_tx_pdu_meta_t));
}
#endif /* CONFIG_BT_CTLR_TX_PDU_META */

ZTEST_SUITE(pdu_layout, NULL, NULL, NULL, NULL, NULL);
