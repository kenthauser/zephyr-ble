/*
 * Copyright (c) 2026 Kent Hauser
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * aesx: application AES-CCM, to test contention for an AES engine shared between the
 * application and the controller.
 *
 * The sender CCM-encrypts each GATT write's payload before handing it to the host; the receiver
 * decrypts and verifies it. Where the controller uses the same AES engine as the application
 * (one engine on the SoC, the controller encrypting the link on it), the two contend for it at
 * the transfer rate; where the controller has its own (e.g. the nRF5 radio's ECB/CCM), they do
 * not. A background load (aesx bg on) runs a CCM op then sleeps 300 us, at the lowest application
 * priority; on a shared engine, turned on before the link is encrypted, it makes the controller's
 * session-key derivation find the engine busy. The receiver counts writes that arrived intact,
 * failed their tag, or are missing from the sequence.
 *
 * Write payload (197 bytes): seq (4, LE) | ciphertext (189) | tag (4). Nonce: seq LE in the
 * first 4 bytes, the rest a constant. No AAD.
 *
 * AES goes through the Zephyr crypto API: crypto_cc23x0 (the SoC's AES engine) where present,
 * else the Mbed TLS shim (software).
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#define AESX_LEN 197U
#define AESX_SEQ 4U
#define AESX_TAG 4U
#define AESX_CT  (AESX_LEN - AESX_SEQ - AESX_TAG)

static const uint8_t aesx_key[16] __aligned(4) = {
	0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
	0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
};

static struct {
	uint32_t tx;		/* writes sent */
	uint32_t enc_fail;	/* sender's encrypt failed */
	uint32_t enc_bad;	/* sender's own ciphertext failed to decrypt */
	uint32_t rx;		/* writes received */
	uint32_t rx_ok;		/* tag verified, plaintext as sent */
	uint32_t rx_tag;	/* tag failed */
	uint32_t rx_data;	/* tag verified, plaintext wrong */
	uint32_t rx_gap;	/* writes missing from the sequence */
	uint32_t rx_len;	/* wrong length */
	uint32_t next_seq;
	int64_t rx_first_ms;
	int64_t rx_last_ms;
	int64_t tx_ms;		/* duration of the last send */
} st;

/* LLCP session-key derivations the controller deferred because the AES engine was busy, where the
 * controller counts them (ull_llcp_enc.c, test instrumentation). Zero otherwise.
 */
__weak uint32_t llcp_enc_retry;

static void nonce_make(uint8_t nonce[16], uint32_t seq)
{
	memset(nonce, 0xA5, 16);
	sys_put_le32(seq, nonce);
}

/* The plaintext of write seq: a pattern that depends on the sequence number. */
static void plain_make(uint8_t *p, uint32_t seq)
{
	for (uint32_t i = 0U; i < AESX_CT; i++) {
		p[i] = (uint8_t)((seq * 7U) + i);
	}
}

#include <zephyr/crypto/crypto.h>

#if DT_HAS_COMPAT_STATUS_OKAY(ti_cc23x0_aes)
#define AESX_CRYPTO_DEV() DEVICE_DT_GET_ONE(ti_cc23x0_aes)
#else
#define AESX_CRYPTO_DEV() device_get_binding(CONFIG_CRYPTO_MBEDTLS_SHIM_DRV_NAME)
#endif

static struct cipher_ctx ctx_enc;
static struct cipher_ctx ctx_dec;
static bool sessions;

static int session(const struct device *dev, struct cipher_ctx *ctx, enum cipher_op op)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->keylen = sizeof(aesx_key);
	ctx->key.bit_stream = aesx_key;
	ctx->flags = CAP_RAW_KEY | CAP_SYNC_OPS | CAP_SEPARATE_IO_BUFS | CAP_NO_IV_PREFIX;
	ctx->mode_params.ccm_info.nonce_len = 13U;
	ctx->mode_params.ccm_info.tag_len = AESX_TAG;

	return cipher_begin_session(dev, ctx, CRYPTO_CIPHER_ALGO_AES, CRYPTO_CIPHER_MODE_CCM, op);
}

static int backend_init(void)
{
	const struct device *dev = AESX_CRYPTO_DEV();
	int err;

	if (sessions) {
		return 0;
	}
	if ((dev == NULL) || !device_is_ready(dev)) {
		return -ENODEV;
	}
	err = session(dev, &ctx_enc, CRYPTO_CIPHER_OP_ENCRYPT);
	if (err == 0) {
		err = session(dev, &ctx_dec, CRYPTO_CIPHER_OP_DECRYPT);
	}
	sessions = (err == 0);

	return err;
}

static int backend_ccm(bool enc, const uint8_t *nonce, const uint8_t *in, uint8_t *out,
		       uint8_t *tag)
{
	/* Whole AES blocks in and out (crypto_cc23x0 reads and writes them): bounce through
	 * block-rounded buffers, one user at a time.
	 */
	static uint8_t z_in[ROUND_UP(AESX_CT, 16U)] __aligned(4);
	static uint8_t z_out[ROUND_UP(AESX_CT, 16U)] __aligned(4);
	static K_MUTEX_DEFINE(z_lock);
	struct cipher_pkt pkt = {
		.in_buf = z_in,
		.in_len = AESX_CT,
		.out_buf = z_out,
		.out_buf_max = sizeof(z_out),
	};
	struct cipher_aead_pkt ap = {
		.ad = NULL,
		.ad_len = 0U,
		.pkt = &pkt,
		.tag = tag,
	};
	int err;

	(void)k_mutex_lock(&z_lock, K_FOREVER);
	memcpy(z_in, in, AESX_CT);
	err = cipher_ccm_op(enc ? &ctx_enc : &ctx_dec, &ap, (uint8_t *)nonce);
	memcpy(out, z_out, AESX_CT);
	(void)k_mutex_unlock(&z_lock);

	if (err == 0) {
		return 0;
	}

	return enc ? -EIO : -EBADMSG;
}

/* Background load: AES not tied to the link's timing (bulk encryption, an image decrypt), on its
 * own session at the lowest application priority, one CCM op then a 300 us sleep.
 */
static volatile bool bg_on;
static uint32_t bg_ops;

static void bg_fn(void *a, void *b, void *c)
{
	static uint8_t bg_in[ROUND_UP(CONFIG_APP_AESX_BG_LEN, 16U)] __aligned(4);
	static uint8_t bg_out[ROUND_UP(CONFIG_APP_AESX_BG_LEN, 16U)] __aligned(4);
	static uint8_t bg_tag[AESX_TAG] __aligned(4);
	static struct cipher_ctx ctx_bg;
	const struct device *dev = AESX_CRYPTO_DEV();
	uint8_t nonce[16] __aligned(4);
	struct cipher_pkt pkt = {
		.in_buf = bg_in,
		.in_len = CONFIG_APP_AESX_BG_LEN,
		.out_buf = bg_out,
		.out_buf_max = sizeof(bg_out),
	};
	struct cipher_aead_pkt ap = {
		.pkt = &pkt,
		.tag = bg_tag,
	};

	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	if ((dev == NULL) || !device_is_ready(dev) ||
	    (session(dev, &ctx_bg, CRYPTO_CIPHER_OP_ENCRYPT) != 0)) {
		return;
	}

	for (;;) {
		if (!bg_on) {
			k_sleep(K_MSEC(50));
			continue;
		}
		nonce_make(nonce, bg_ops);
		if (cipher_ccm_op(&ctx_bg, &ap, nonce) == 0) {
			bg_ops++;
		}
		(void)k_sleep(K_USEC(300));
	}
}

K_THREAD_DEFINE(aesx_bg, CONFIG_APP_AESX_BG_STACK, bg_fn, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

/* App crypto on every write (aesx crypt on|off); off sends and checks the plaintext pattern. */
static bool app_crypt = true;

/* Receiver */

static ssize_t aesx_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
			  uint16_t len, uint16_t offset, uint8_t flags)
{
	static uint8_t in[AESX_CT] __aligned(4);
	static uint8_t out[AESX_CT] __aligned(4);
	static uint8_t tag[AESX_TAG] __aligned(4);
	static uint8_t want[AESX_CT] __aligned(4);
	uint8_t nonce[16] __aligned(4);
	const uint8_t *b = buf;
	uint32_t seq;

	st.rx++;
	st.rx_last_ms = k_uptime_get();
	if (st.rx == 1U) {
		st.rx_first_ms = st.rx_last_ms;
	}
	if ((len != AESX_LEN) || (offset != 0U)) {
		st.rx_len++;
		return len;
	}

	seq = sys_get_le32(b);
	if (seq > st.next_seq) {
		st.rx_gap += seq - st.next_seq;
	}
	st.next_seq = seq + 1U;

	memcpy(in, &b[AESX_SEQ], AESX_CT);
	memcpy(tag, &b[AESX_SEQ + AESX_CT], AESX_TAG);
	nonce_make(nonce, seq);
	if (!app_crypt) {
		memcpy(out, in, AESX_CT);
	} else if ((backend_init() != 0) || (backend_ccm(false, nonce, in, out, tag) != 0)) {
		st.rx_tag++;
		return len;
	}
	plain_make(want, seq);
	if (memcmp(out, want, AESX_CT) != 0) {
		st.rx_data++;
	} else {
		st.rx_ok++;
	}

	return len;
}

BT_GATT_SERVICE_DEFINE(aesx_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(
		BT_UUID_128_ENCODE(0x6b6b0001, 0x1234, 0x5678, 0x1234, 0x56789abcdef0))),
	BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(
		BT_UUID_128_ENCODE(0x6b6b0002, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)),
		BT_GATT_CHRC_WRITE_WITHOUT_RESP, BT_GATT_PERM_WRITE, NULL, aesx_write, NULL),
);

/* Sender */

static void conn_first(struct bt_conn *conn, void *data)
{
	struct bt_conn **c = data;

	if (*c == NULL) {
		*c = bt_conn_ref(conn);
	}
}

/* The sender decrypts its own ciphertext before sending it (aesx check on|off). */
static bool self_check = true;

static int cmd_crypt(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);

	app_crypt = (strcmp(argv[1], "on") == 0);
	shell_print(sh, "aesx crypt %s", app_crypt ? "on" : "off");

	return 0;
}

static int cmd_check(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);

	self_check = (strcmp(argv[1], "on") == 0);
	shell_print(sh, "aesx check %s", self_check ? "on" : "off");

	return 0;
}

static int cmd_send(const struct shell *sh, size_t argc, char **argv)
{
	static uint8_t plain[AESX_CT] __aligned(4);
	static uint8_t ct[AESX_CT] __aligned(4);
	static uint8_t chk[AESX_CT] __aligned(4);
	static uint8_t tag[AESX_TAG] __aligned(4);
	static uint8_t ctag[AESX_TAG] __aligned(4);
	static uint8_t pdu[AESX_LEN];
	uint8_t nonce[16] __aligned(4);
	uint16_t handle = (uint16_t)strtoul(argv[1], NULL, 16);
	uint32_t count = strtoul(argv[2], NULL, 0);
	struct bt_conn *conn = NULL;
	int64_t t0;
	int err;

	ARG_UNUSED(argc);

	err = backend_init();
	if (err != 0) {
		shell_error(sh, "aesx: backend init %d", err);
		return err;
	}
	bt_conn_foreach(BT_CONN_TYPE_LE, conn_first, &conn);
	if (conn == NULL) {
		shell_error(sh, "aesx: not connected");
		return -ENOTCONN;
	}

	t0 = k_uptime_get();
	for (uint32_t seq = 0U; seq < count; seq++) {
		plain_make(plain, seq);
		nonce_make(nonce, seq);
		if (!app_crypt) {
			memcpy(ct, plain, AESX_CT);
			memset(tag, 0, AESX_TAG);
		} else if (backend_ccm(true, nonce, plain, ct, tag) != 0) {
			st.enc_fail++;
			continue;
		}
		if (self_check && app_crypt) {
			memcpy(ctag, tag, AESX_TAG);
			if ((backend_ccm(false, nonce, ct, chk, ctag) != 0) ||
			    (memcmp(chk, plain, AESX_CT) != 0)) {
				st.enc_bad++;
			}
		}
		sys_put_le32(seq, pdu);
		memcpy(&pdu[AESX_SEQ], ct, AESX_CT);
		memcpy(&pdu[AESX_SEQ + AESX_CT], tag, AESX_TAG);

		err = bt_gatt_write_without_response(conn, handle, pdu, AESX_LEN, false);
		if (err != 0) {
			break;
		}
		st.tx++;
		k_yield();
	}
	st.tx_ms = k_uptime_get() - t0;
	bt_conn_unref(conn);

	shell_print(sh, "aesx sent %u in %lld ms (err %d)", st.tx, st.tx_ms, err);

	return err;
}

/* Known answer against a host CCM: aesx vec <seq> prints write seq's ciphertext and tag. */
static int cmd_vec(const struct shell *sh, size_t argc, char **argv)
{
	static uint8_t plain[AESX_CT] __aligned(4);
	static uint8_t ct[AESX_CT] __aligned(4);
	static uint8_t tag[AESX_TAG] __aligned(4);
	uint8_t nonce[16] __aligned(4);
	uint32_t seq = strtoul(argv[1], NULL, 0);
	char hex[(2U * AESX_CT) + 1U];
	int err;

	ARG_UNUSED(argc);

	err = backend_init();
	if (err != 0) {
		return err;
	}
	plain_make(plain, seq);
	nonce_make(nonce, seq);
	err = backend_ccm(true, nonce, plain, ct, tag);
	(void)bin2hex(ct, AESX_CT, hex, sizeof(hex));
	shell_print(sh, "aesx vec ct %s", hex);
	(void)bin2hex(tag, AESX_TAG, hex, sizeof(hex));
	shell_print(sh, "aesx vec tag %s err %d", hex, err);

	return 0;
}

static int cmd_stats(const struct shell *sh, size_t argc, char **argv)
{
	int64_t ms = st.rx_last_ms - st.rx_first_ms;
	uint32_t kbps = (ms > 0) ? (uint32_t)(((uint64_t)st.rx_ok * AESX_LEN * 8U) / ms) : 0U;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "aesx tx %u enc_fail %u enc_bad %u | rx %u ok %u tag %u data %u gap %u "
		    "len %u | rx %lld ms %u kbps | tx %lld ms",
		    st.tx, st.enc_fail, st.enc_bad, st.rx, st.rx_ok, st.rx_tag, st.rx_data,
		    st.rx_gap, st.rx_len, ms, kbps, st.tx_ms);
	shell_print(sh, "aesx bg %s ops %u", bg_on ? "on" : "off", bg_ops);
	shell_print(sh, "aesx llcp_enc_retry %u", llcp_enc_retry);

	return 0;
}

static int cmd_reset(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	memset(&st, 0, sizeof(st));
	bg_ops = 0U;
	shell_print(sh, "aesx reset");

	return 0;
}

static int cmd_bg(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);

	bg_on = (strcmp(argv[1], "on") == 0);
	shell_print(sh, "aesx bg %s", bg_on ? "on" : "off");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(aesx_cmds,
	SHELL_CMD_ARG(send, NULL, "<handle-hex> <count>  encrypted writes to the first connection",
		      cmd_send, 3, 0),
	SHELL_CMD_ARG(stats, NULL, "print counters", cmd_stats, 1, 0),
	SHELL_CMD_ARG(reset, NULL, "zero counters", cmd_reset, 1, 0),
	SHELL_CMD_ARG(crypt, NULL, "on|off  app AES-CCM on each write", cmd_crypt, 2, 0),
	SHELL_CMD_ARG(check, NULL, "on|off  sender decrypts its own output", cmd_check, 2, 0),
	SHELL_CMD_ARG(vec, NULL, "<seq>  ciphertext and tag of write seq", cmd_vec, 2, 0),
	SHELL_CMD_ARG(bg, NULL, "on|off  background AES load", cmd_bg, 2, 0),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(aesx, &aesx_cmds, "application AES-CCM, AES engine contention test", NULL);
