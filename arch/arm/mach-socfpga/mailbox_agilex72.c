// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 Altera Corporation <www.altera.com>
 */

#include <hang.h>
#include <wait_bit.h>
#include <asm/arch/clock_manager.h>
#include <asm/arch/mailbox_agilex72.h>
#include <asm/arch/rsu.h>
#include <asm/arch/smc_api.h>
#include <asm/arch/system_manager.h>
#include <asm/arch/timer.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/secure.h>
#include <asm/system.h>

DECLARE_GLOBAL_DATA_PTR;

#define MBOX_READL(reg)			\
	readl(SOCFPGA_MAILBOX_ADDRESS + (reg))

#define MBOX_WRITEL(data, reg)		\
	writel(data, SOCFPGA_MAILBOX_ADDRESS + (reg))

#define MAILBOX_MAX_QUEUE_SIZE			11
#define MSG_HEADER_TRANSACTION_ID_MASK	GENMASK(31, 24)
#define MSG_HEADER_CHANNEL_NUM_MASK	GENMASK(31, 28)
#define MSG_HEADER_SEQUENCE_ID_MASK	GENMASK(27, 24)
#define MSG_HEADER_CHECKSUM_MASK	GENMASK(23, 23)
#define MSG_HEADER_LENGTH_MASK		GENMASK(22, 12)
#define MSG_HEADER_COMMAND_MASK		GENMASK(11, 0)

#define MSG_HEADER_MAKE(ch, seq, cks, len, cmd)			\
	(FIELD_PREP(MSG_HEADER_CHANNEL_NUM_MASK, (ch)) |	\
	FIELD_PREP(MSG_HEADER_SEQUENCE_ID_MASK, (seq)) |	\
	FIELD_PREP(MSG_HEADER_CHECKSUM_MASK, (cks)) |		\
	FIELD_PREP(MSG_HEADER_LENGTH_MASK, (len)) |		\
	FIELD_PREP(MSG_HEADER_COMMAND_MASK, (cmd)))

#define MSG_HEADER_GET_CHANNEL_NUM(x)       FIELD_GET(MSG_HEADER_CHANNEL_NUM_MASK, (x))
#define MSG_HEADER_GET_SEQUENCE_ID(x)       FIELD_GET(MSG_HEADER_SEQUENCE_ID_MASK, (x))
#define MSG_HEADER_GET_TRANSACTION_ID(x)    FIELD_GET(MSG_HEADER_TRANSACTION_ID_MASK, (x))
#define MSG_HEADER_GET_CHECKSUM(x)          FIELD_GET(MSG_HEADER_CHECKSUM_MASK, (x))
#define MSG_HEADER_GET_LENGTH(x)            FIELD_GET(MSG_HEADER_LENGTH_MASK, (x))
#define MSG_HEADER_GET_CMD_RESP(x)          FIELD_GET(MSG_HEADER_COMMAND_MASK, (x))

/* Each index to this message buffers correspond to the ring buffer index */
static struct mailbox_message
	msg_buff[MAILBOX_MAX_QUEUE_SIZE] __section(".data");
static struct mailbox_response
	resp_buff[MAILBOX_MAX_QUEUE_SIZE] __section(".data");

static u32 mcsr_s2c_get_current_cin(void)
{
	return MBOX_READL(S2C_RBUF_CIN_REG_OFST);
}

static void mcsr_s2c_update_cin(u32 index)
{
	MBOX_WRITEL(index, S2C_RBUF_CIN_REG_OFST);
}

static u32 mcsr_s2c_get_previous_cout(u32 cout)
{
	return (!cout) ? (S2C_NUM_DESC - 1) : (cout - 1);
}

static void mcsr_s2c_desc_set(struct mcsr_s2c_desc *d,
			      u64 addr, u32 size_words)
{
	u32 addr_high;

	d->addr_low = lower_32_bits(addr);
	addr_high = upper_32_bits(addr);
	d->word1 = FIELD_PREP(MCSR_ADDR_HIGH_MASK, addr_high) |
		   FIELD_PREP(MCSR_SIZE_MASK, size_words);
}

static void mcsr_s2c_init(void)
{
	u32 cin = MBOX_READL(S2C_RBUF_CIN_REG_OFST);
	u32 cout =  MBOX_READL(S2C_COUT_OFST);

	MBOX_WRITEL(0, S2C_RBUF_CIN_REG_OFST);
	pr_debug("%s %d: S2C_HEAD = 0x%x %d\n", __func__, __LINE__,
		 SOCFPGA_MAILBOX_ADDRESS + S2C_RBUF_CIN_REG_OFST, cin);

	pr_debug("%s %d: S2C_TAIL = 0x%x %d\n", __func__, __LINE__,
		 SOCFPGA_MAILBOX_ADDRESS + S2C_COUT_OFST, cout);
}

static void mcsr_s2c_write_desc(u32 index, struct mcsr_s2c_desc *d)
{
	long reg = SOCFPGA_MAILBOX_ADDRESS + S2C_RBUF_DESC_OFST +
		   index * S2C_DESC_REGS * sizeof(u32);

	u32 *desc = (u32 *)reg;

	desc[0] = d->addr_low;
	desc[1] = d->word1;
	pr_debug("%s %d: S2C: reg = %p resp buffer addr = %x\n",
		 __func__, __LINE__, (u32 *)reg,
		 d->addr_low);
}

static bool mcsr_s2c_is_full(void)
{
	u32 cin =  MBOX_READL(S2C_RBUF_CIN_REG_OFST);
	u32 cout = MBOX_READL(S2C_COUT_OFST);

	return ((cin + 1) % S2C_NUM_DESC) == cout;
}

static bool mcsr_s2c_sw_is_full(u8 cin, u8 cout)
{
	u32 head = cin;
	u32 tail = cout;

	return ((head + 1) % S2C_NUM_DESC) == tail;
}

static void mcsr_s2c_write_cin(u32 index)
{
	MBOX_WRITEL(index, S2C_RBUF_CIN_REG_OFST);
	pr_debug("%s %d: S2C: set CIN to %d\n",
		 __func__, __LINE__, index);
}

static u32 mcsr_s2c_increment_cin(u32 cin)
{
	return (cin + 1) % S2C_NUM_DESC;
}

static bool mcsr_s2c_push(struct mcsr_s2c_desc *d)
{
	u32 head = MBOX_READL(S2C_RBUF_CIN_REG_OFST);

	if (mcsr_s2c_is_full()) {
		printf("%s %d: S2C is full!\n", __func__, __LINE__);
		return false;
	}

	mcsr_s2c_write_desc(head, d);

	/* After we write n to head index, we increment it by 1. */
	MBOX_WRITEL((head + 1) % S2C_NUM_DESC, S2C_RBUF_CIN_REG_OFST);
	return true;
}

static u32 mcsr_s2c_get_cout(void)
{
	u32 cout = MBOX_READL(S2C_COUT_OFST);

	return cout;
}

static u32 mcsr_s2c_decrement_cout(u32 cout)
{
	if (!cout)
		cout = S2C_NUM_DESC - 1;
	else
		cout--;

	return cout;
}

static u32 mcsr_c2s_get_current_cin(void)
{
	return MBOX_READL(C2S_RW_CIN_REG_OFST);
}

static void mcsr_c2s_desc_set(struct mcsr_c2s_desc *d, u64 addr,
			      u32 size_words)
{
	u32 addr_high;

	d->addr_low = lower_32_bits(addr);
	addr_high = upper_32_bits(addr);
	d->word1 = FIELD_PREP(MCSR_ADDR_HIGH_MASK, addr_high) |
		   FIELD_PREP(MCSR_SIZE_MASK, size_words);
}

static void mcsr_c2s_init(void)
{
	MBOX_WRITEL(0, C2S_RW_CIN_REG_OFST);

	/* Reset the Client MCSR */
	MBOX_WRITEL(1, C2S_RW_RESET_REG_OFST);
}

static void mcsr_c2s_write_desc(u32 index, struct mcsr_c2s_desc *d)
{
	long reg = SOCFPGA_MAILBOX_ADDRESS + C2S_RW_DESC_OFST +
		   index * C2S_DESC_REGS * sizeof(u32);

	u32 *desc = (u32 *)reg;

	desc[0] = d->addr_low;
	desc[1] = d->word1;
	pr_debug("%s %d: C2S: Write to REG %x %x  Addr= %lx\n",
		 __func__, __LINE__, desc[0], desc[1], reg);
}

static bool mcsr_c2s_is_full(void)
{
	u32 cin = MBOX_READL(C2S_RW_CIN_REG_OFST);
	u32 cout = MBOX_READL(C2S_RO_COUT_REG_OFST);

	return ((cin + 1) % C2S_NUM_DESC) == cout;
}

static bool mcsr_c2s_push(struct mcsr_c2s_desc *d)
{
	u32 head = MBOX_READL(C2S_RW_CIN_REG_OFST);

	if (mcsr_c2s_is_full())
		return false;

	mcsr_c2s_write_desc(head, d);
	MBOX_WRITEL((head + 1) % C2S_NUM_DESC, C2S_RW_CIN_REG_OFST);

	return true;
}

static int mcsr_c2s_mailbox_send(u32 header_cmd, u32 *args, unsigned int len)
{
	int status = 0;
	u32 i = 0U;
	u32 current_tx_desc_index = 0U;
	struct mcsr_c2s_desc current_desc;

	if (len > MBOX_MSG_MAX_LENGTH)
		return -EINVAL;

	if (mcsr_c2s_is_full()) {
		status = MBOX_BUFFER_FULL;
	} else {
		current_tx_desc_index = mcsr_c2s_get_current_cin();
		msg_buff[current_tx_desc_index].msg_header = header_cmd;

		for (i = 0U; i < len; i++)
			msg_buff[current_tx_desc_index].msg_data[i] =
				args[i];

		mcsr_c2s_desc_set(&current_desc,
				  (u64)(&msg_buff[current_tx_desc_index]),
				  MCSR_DESC_MAX_WORD);

		mcsr_c2s_push(&current_desc);
	}

	return status;
}

static int mbox_send_cmd_only_common(u8 id, u32 cmd,
				     u8 is_indirect, u32 len,
				     u32 *arg)
{
	u32 status;

	(void)is_indirect;

	status = mcsr_c2s_mailbox_send(MBOX_CLIENT_ID_CMD(MBOX_CLIENT_ID_UBOOT) |
				       MBOX_JOB_ID_CMD(id) |
				       MBOX_CMD_LEN_CMD(len) |
				       cmd, arg, len);
	return status;
}

static int mcsr_mailbox_wait_for_response(u8 client_id, u32 job_id,
					  u32 *response,
					  unsigned int *resp_buf_len,
					  bool ignore_job_id)
{
	u32 buf_len;
	u32 buf_index = 0;

	unsigned int polling_counter = MBOX_POLL_MAX_ITERATIONS;
	u32 snapshot_cin = 0, snapshot_cout = 0;

	while (polling_counter) {
		/* Check for response in the S2C queue. */
		snapshot_cin = mcsr_s2c_get_current_cin();
		snapshot_cout = mcsr_s2c_get_cout();

		if (!mcsr_s2c_sw_is_full(snapshot_cin, snapshot_cout)) {
			u32 tail = snapshot_cout;

#ifdef DEBUG
			pr_debug("%s %d: Dumping Ring Buffer\n",
				 __func__, __LINE__);
			for (int ringbuf_idx = 0;
			     ringbuf_idx < MAILBOX_MAX_QUEUE_SIZE;
			     ringbuf_idx++) {
				pr_debug("  RING BUFFER [%d] = %x Addr = %llx\n",
					 ringbuf_idx,
					 resp_buff[ringbuf_idx].msg_header,
					 (u64)(&resp_buff[ringbuf_idx]));
			}
#endif
			while (1) {
				u32 seq_id, ch_num;
				u32 msg_hdr;

				tail = mcsr_s2c_decrement_cout(tail);
				msg_hdr = resp_buff[tail].msg_header;
				seq_id = MSG_HEADER_GET_SEQUENCE_ID(msg_hdr);
				ch_num = MSG_HEADER_GET_CHANNEL_NUM(msg_hdr);

				if ((seq_id == job_id && ch_num == client_id) ||
				    ignore_job_id) {
					pr_debug("%s %d: CH NUM = %d seq id = %d\n",
						 __func__, __LINE__, ch_num, seq_id);
					if (resp_buf_len) {
						buf_len = *resp_buf_len;
						buf_index = 0;
					} else {
						buf_len = 0;
					}
					while (buf_len &&
					       (buf_index <=
						MBOX_RESP_MAX_LENGTH)) {
						response[buf_index] =
							resp_buff[tail].msg_data[buf_index];
						buf_index++;
						buf_len--;
					}
				}

				if (tail == mcsr_s2c_increment_cin(snapshot_cin)) {
					snapshot_cin = mcsr_s2c_decrement_cout(snapshot_cout);
					mcsr_s2c_write_cin(snapshot_cin);
					break;
				}
			}
		}
		polling_counter--;
	}

	if (!polling_counter)
		return MBOX_TIMEOUT;
	else
		return MBOX_RET_OK;
}

static __always_inline int __mbox_rcv_resp(u32 *resp_buf, u32 resp_buf_max_len)
{
	mcsr_mailbox_wait_for_response(0, 0, resp_buf, (u32 *)&resp_buf_max_len, true);
	return 0;
}

static int mbox_send_cmd_common(u8 id, u32 cmd, u8 is_indirect,
				u32 len, u32 *arg, u8 urgent,
				u32 *resp_buf_len,
				u32 *resp_buf)
{
	u32 status;

	/* To standardise the function declaration */
	(void)urgent;

	/* Push the command to mailbox FIFO */
	status = mbox_send_cmd_only_common(id, cmd, false, len, arg);

	if (!status) {
		/*
		 * poll for response till we get the response.
		 * Blocking call.
		 */
		status = mcsr_mailbox_wait_for_response(MBOX_CLIENT_ID_UBOOT,
							id, resp_buf,
							resp_buf_len,
							/* Same job id only */
							false);
	}

	return status;
}

static int mbox_send_cmd_common_retry(u8 id, u32 cmd,
				      u8 is_indirect,
				      u32 len, u32 *arg,
				      u8 urgent,
				      u32 *resp_buf_len,
				      u32 *resp_buf)
{
	int ret;
	int i;

	for (i = 0; i < MBOX_RETRY_COUNT; i++) {
		ret = mbox_send_cmd_common(id, cmd, is_indirect, len, arg,
					   urgent, resp_buf_len, resp_buf);
		if (ret == MBOX_RESP_TIMEOUT || ret == MBOX_RESP_DEVICE_BUSY)
			/* wait for 2ms before resend */
			udelay(MBOX_RETRY_DELAY_US);
		else
			break;
	}

	return ret;
}

int mbox_init(void)
{
	struct mcsr_s2c_desc resp_desc;
	u32 curr_cout = 0;

	/* C2S must be init first to reset the MCSR */
	mcsr_c2s_init();
	mcsr_s2c_init();

	/* Pre-populate S2C response descriptors so SDM can send responses */
	for (int i = 0; i < MAILBOX_MAX_QUEUE_SIZE; i++) {
		mcsr_s2c_desc_set(&resp_desc,
				  (u64)(&resp_buff[i]),
				  MBOX_RESP_MAX_LENGTH);
		mcsr_s2c_push(&resp_desc);
	}

	/* Update CIN = COUT - 1, full buffer state */
	curr_cout = mcsr_s2c_get_cout();
	mcsr_s2c_update_cin(mcsr_s2c_get_previous_cout(curr_cout));

	return 0;
}

#ifdef CONFIG_CADENCE_XSPI
int mbox_qspi_close(void)
{
	return mbox_send_cmd(MBOX_ID_UBOOT, MBOX_QSPI_CLOSE, MBOX_CMD_DIRECT,
			     0, NULL, 0, 0, NULL);
}

int mbox_qspi_open(void)
{
	int ret;
	u32 resp_buf[1];
	u32 resp_buf_len;

	ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_QSPI_OPEN, MBOX_CMD_DIRECT,
			    0, NULL, 0, 0, NULL);
	if (ret) {
		/* retry again by closing and reopen the QSPI again */
		ret = mbox_qspi_close();
		if (ret)
			return ret;

		ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_QSPI_OPEN,
				    MBOX_CMD_DIRECT, 0, NULL, 0, 0, NULL);
		if (ret)
			return ret;
	}

	/* HPS will directly control the QSPI controller, no longer mailbox */
	resp_buf_len = 1;
	ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_QSPI_DIRECT, MBOX_CMD_DIRECT,
			    0, NULL, 0, (u32 *)&resp_buf_len,
			    (u32 *)&resp_buf);
	if (ret)
		goto error;

	/* Store QSPI controller ref clock frequency */
	ret = cm_set_qspi_controller_clk_hz(resp_buf[0]);
	if (ret)
		goto error;

	return 0;

error:
	mbox_qspi_close();

	return ret;
}

/* get QSPI size and erasesize */
int mbox_qspi_get_device_info(u32 *resp_buf, u32 resp_buf_len)
{
	int ret;

	if (!IS_ENABLED(CONFIG_XPL_BUILD) && IS_ENABLED(CONFIG_SPL_ATF)) {
		ret = smc_send_mailbox(MBOX_QSPI_GET_DEVICE_INFO, 0, NULL, 0,
				       (u32 *)&resp_buf_len, (u32 *)resp_buf);
	} else {
		ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_QSPI_GET_DEVICE_INFO,
				    MBOX_CMD_DIRECT, 0, NULL, 0, (u32 *)&resp_buf_len,
				    (u32 *)resp_buf);
	}

	if (ret) {
		debug("%s: Failed to retrieve QSPI Device INFO: %d\n", __func__, ret);
		return ret;
	}

	debug("Successfully retrieve QSPI Device INFO.\n");
	return 0;
}

int mbox_rsu_get_spt_offset(u32 *resp_buf, u32 resp_buf_len)
{
#if !defined(CONFIG_XPL_BUILD) && defined(CONFIG_SPL_ATF)
	return smc_send_mailbox(MBOX_GET_SUBPARTITION_TABLE, 0, NULL, 0,
				(u32 *)&resp_buf_len, (u32 *)resp_buf);
#else
	return mbox_send_cmd(MBOX_ID_UBOOT, MBOX_GET_SUBPARTITION_TABLE,
			     MBOX_CMD_DIRECT, 0, NULL, 0, (u32 *)&resp_buf_len,
			     (u32 *)resp_buf);
#endif
}

int mbox_rsu_status(u32 *resp_buf, u32 resp_buf_len)
{
	int ret;
	struct rsu_status_info *info = (struct rsu_status_info *)resp_buf;

	info->retry_counter = -1;

#if !defined(CONFIG_XPL_BUILD) && defined(CONFIG_SPL_ATF)
	ret = smc_send_mailbox(MBOX_RSU_STATUS, 0, NULL, 0,
			       (u32 *)&resp_buf_len, (u32 *)resp_buf);
#else
	ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_RSU_STATUS, MBOX_CMD_DIRECT, 0,
			    NULL, 0, (u32 *)&resp_buf_len, (u32 *)resp_buf);
#endif

	if (ret)
		return ret;

	if (info->retry_counter != -1)
		if (!RSU_VERSION_ACMF_VERSION(info->version))
			info->version |= FIELD_PREP(RSU_VERSION_ACMF_MASK, 1);

	return ret;
}

int mbox_rsu_update(u32 *flash_offset)
{
#if !defined(CONFIG_XPL_BUILD) && defined(CONFIG_SPL_ATF)
	return smc_send_mailbox(MBOX_RSU_UPDATE, 2, (u32 *)flash_offset, 0,
				0, NULL);
#else
	return mbox_send_cmd(MBOX_ID_UBOOT, MBOX_RSU_UPDATE, MBOX_CMD_DIRECT, 2,
			     (u32 *)flash_offset, 0, 0, NULL);
#endif
}

#else
int mbox_rsu_get_spt_offset(u32 *resp_buf, u32 resp_buf_len)
{
	return MBOX_FUNC_NOT_SUPPORTED;
}

int mbox_rsu_status(u32 *resp_buf, u32 resp_buf_len)
{
	return MBOX_FUNC_NOT_SUPPORTED;
}

int mbox_rsu_update(u32 *flash_offset)
{
	return MBOX_FUNC_NOT_SUPPORTED;
}

#endif /* CONFIG_CADENCE_XSPI */

int mbox_reset_cold(void)
{
#if !CONFIG_IS_ENABLED(XPL_BUILD) && CONFIG_IS_ENABLED(SPL_ATF)
	psci_system_reset();
#else
	int ret;

	ret = mbox_send_cmd(MBOX_ID_UBOOT, MBOX_REBOOT_HPS, MBOX_CMD_DIRECT,
			    0, NULL, 0, 0, NULL);
	if (ret) {
		/* mailbox sent failure, wait for watchdog to kick in */
		hang();
	}
#endif
	return 0;
}

/* Accepted commands: CONFIG_STATUS or RECONFIG_STATUS */
static int mbox_get_fpga_config_status_common(u32 cmd)
{
	u32 reconfig_status_resp_len;
	u32 reconfig_status_resp[RECONFIG_STATUS_RESPONSE_LEN];
	int ret;

	reconfig_status_resp_len = RECONFIG_STATUS_RESPONSE_LEN;
	ret = mbox_send_cmd_common_retry(MBOX_ID_UBOOT, cmd,
					 MBOX_CMD_DIRECT, 0, NULL, 0,
					 &reconfig_status_resp_len,
					 reconfig_status_resp);

	if (ret)
		return ret;

	/* Check for any error */
	ret = reconfig_status_resp[RECONFIG_STATUS_STATE];
	if (ret && ret != MBOX_CFGSTAT_STATE_CONFIG)
		return ret;

	/* Make sure nStatus is not 0 */
	ret = reconfig_status_resp[RECONFIG_STATUS_PIN_STATUS];
	if (!(ret & RCF_PIN_STATUS_NSTATUS))
		return MBOX_CFGSTAT_STATE_ERROR_HARDWARE;

	ret = reconfig_status_resp[RECONFIG_STATUS_SOFTFUNC_STATUS];
	if (ret & RCF_SOFTFUNC_STATUS_SEU_ERROR)
		return MBOX_CFGSTAT_STATE_ERROR_HARDWARE;

	if ((ret & RCF_SOFTFUNC_STATUS_CONF_DONE) &&
	    (ret & RCF_SOFTFUNC_STATUS_INIT_DONE) &&
	    !reconfig_status_resp[RECONFIG_STATUS_STATE])
		return 0;	/* configuration success */

	return MBOX_CFGSTAT_STATE_CONFIG;
}

int mbox_get_fpga_config_status(u32 cmd)
{
	return mbox_get_fpga_config_status_common(cmd);
}

int mbox_send_cmd(u8 id, u32 cmd, u8 is_indirect, u32 len, u32 *arg,
		  u8 urgent, u32 *resp_buf_len, u32 *resp_buf)
{
	return mbox_send_cmd_common_retry(id, cmd, is_indirect, len, arg,
					  urgent, resp_buf_len, resp_buf);
}

int mbox_hps_stage_notify(u32 execution_stage)
{
#if !defined(CONFIG_XPL_BUILD) && defined(CONFIG_SPL_ATF)
	return smc_send_mailbox(MBOX_HPS_STAGE_NOTIFY, 1, &execution_stage,
				0, 0, NULL);
#else
	return mbox_send_cmd(MBOX_ID_UBOOT, MBOX_HPS_STAGE_NOTIFY,
			     MBOX_CMD_DIRECT, 1, &execution_stage, 0, 0, NULL);
#endif
}

int mbox_send_cmd_only(u8 id, u32 cmd, u8 is_indirect, u32 len, u32 *arg)
{
	return mbox_send_cmd_only_common(id, cmd, is_indirect, len, arg);
}

int mbox_rcv_resp(u32 *resp_buf, u32 resp_buf_max_len)
{
	return __mbox_rcv_resp(resp_buf, resp_buf_max_len);
}
