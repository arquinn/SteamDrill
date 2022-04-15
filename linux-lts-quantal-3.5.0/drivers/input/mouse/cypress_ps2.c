/*
 * Cypress Trackpad PS/2 mouse driver
 *
 * Copyright (c) 2012 Cypress Semiconductor Corporation.
 *
 * Additional contributors include:
 *   Kamal Mostafa <kamal@canonical.com>
 *   Kyle Fazzari <git@status.e4ward.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/libps2.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include "cypress_ps2.h"

#define CYTP_DBG 1
#if CYTP_DBG
static int debug_level;
module_param_named(cy_debug, debug_level, int, 0644);
MODULE_PARM_DESC(cy_debug, "Set CyPS/2 debug output level (0, 1, or 2)");
#define cytp_dbg(fmt, ...)  \
	do {  \
		if (cytp && debug_level)  \
			pr_err(pr_fmt(fmt), ##__VA_ARGS__);  \
	} while (0)
#define cytp_dbg_dump(fmt, ...)	 \
	do {  \
		if (cytp && debug_level > 1)  \
			pr_err(pr_fmt(fmt), ##__VA_ARGS__);  \
	} while (0)
#else
#define cytp_dbg(fmt, ...)  do { cytp = cytp; } while (0)
#define cytp_dbg_dump(fmt, ...)	  do { cytp = cytp; } while (0)
#endif

static int read_timeout = 200;
module_param_named(cy_read_timeout, read_timeout, int, 0644);
MODULE_PARM_DESC(cy_read_timeout, "Set CyPS/2 cmd read timeout (default 200 msec)");


/* p is a pointer points to the buffer containing Cypress Keys. */
#define IS_CYPRESS_KEY(p) ((p[0] == CYPRESS_KEY_1) && (p[1] == CYPRESS_KEY_2))
#define CYTP_SET_PACKET_SIZE(n) { psmouse->pktsize = cytp->pkt_size = (n); }
#define CYTP_SET_MODE_BIT(x)  \
	do {  \
		if ((x) & CYTP_BIT_ABS_REL_MASK)  \
			cytp->mode = (cytp->mode & ~CYTP_BIT_ABS_REL_MASK) | (x);  \
		else  \
			cytp->mode |= (x);  \
	} while (0)
#define CYTP_CLEAR_MODE_BIT(x)	{ cytp->mode &= ~(x); }

#define CYTP_SUPPORT_ABS

static unsigned char cytp_rate[] = {10, 20, 40, 60, 100, 200};
static unsigned char cytp_resolution[] = {0x00, 0x01, 0x02, 0x03};

static int cypress_ps2_sendbyte(struct psmouse *psmouse, int value)
{
	struct cytp_data *cytp = psmouse->private;
	struct ps2dev *ps2dev = &psmouse->ps2dev;

	if (ps2_sendbyte(ps2dev, value & 0xff, CYTP_CMD_TIMEOUT) < 0) {
		cytp_dbg("send command 0x%02x failed, resp 0x%02x\n",
			 value & 0xff, ps2dev->nak);
		if (ps2dev->nak == CYTP_PS2_RETRY)
			return CYTP_PS2_RETRY;
		else
			return CYTP_PS2_ERROR;
	}

	cytp_dbg("send command 0x%02x success, resp 0xfa\n", value & 0xff);

	return 0;
}

static int cypress_ps2_ext_cmd(struct psmouse *psmouse, unsigned short cmd,
			       unsigned char data)
{
	struct ps2dev *ps2dev = &psmouse->ps2dev;
	int tries = CYTP_PS2_CMD_TRIES;
	int rc;

	ps2_begin_command(ps2dev);

	do {
		/*
		 * send extension command 0xE8 or 0xF3,
		 * if send extension command failed,
		 * try to send recovery command to make
		 * trackpad device return to ready wait command state.
		 * It alwasy success based on this recovery commands.
		 */
		rc = cypress_ps2_sendbyte(psmouse, cmd & 0xff);
		if (rc == CYTP_PS2_RETRY) {
			rc = cypress_ps2_sendbyte(psmouse, 0x00);
			if (rc == CYTP_PS2_RETRY)
				rc = cypress_ps2_sendbyte(psmouse, 0x0a);
		}
		if (rc == CYTP_PS2_ERROR)
			continue;

		rc = cypress_ps2_sendbyte(psmouse, data);
		if (rc == CYTP_PS2_RETRY)
			rc = cypress_ps2_sendbyte(psmouse, data);
		if (rc == CYTP_PS2_ERROR)
			continue;
		else
			break;
	} while (--tries > 0);

	ps2_end_command(ps2dev);

	return rc;
}

static int cypress_ps2_read_cmd_status(struct psmouse *psmouse,
				       unsigned char cmd,
				       unsigned char *param)
{
	int i;
	int rc;
	struct ps2dev *ps2dev = &psmouse->ps2dev;
	struct cytp_data *cytp = psmouse->private;
	enum psmouse_state old_state;
	unsigned char old_pktsize;

	ps2_begin_command(&psmouse->ps2dev);

	old_state = psmouse->state;
	psmouse->state = PSMOUSE_CMD_CYTP;
	psmouse->pktcnt = 0;
	old_pktsize = psmouse->pktsize;
	psmouse->pktsize = 3;
	if (cmd == CYTP_CMD_READ_VITAL_STATISTICS)
		psmouse->pktsize = 8;
	memset(param, 0, psmouse->pktsize);

	rc = cypress_ps2_sendbyte(psmouse, 0xe9);
	if (rc < 0)
		goto out;

	wait_event_timeout(ps2dev->wait,
			   (psmouse->pktcnt >= psmouse->pktsize), msecs_to_jiffies(read_timeout));

	memcpy(param, psmouse->packet, psmouse->pktsize);

	cytp_dbg("Command 0x%02x response data: (0x)", cmd);
	for (i = 0; i < psmouse->pktsize; i++)
		cytp_dbg(" %02x", param[i]);
	cytp_dbg("\n");

out:
	psmouse->state = old_state;
	psmouse->pktcnt = 0;
	psmouse->pktsize = old_pktsize;

	ps2_end_command(&psmouse->ps2dev);

	return rc;
}

static int cypress_verify_cmd_state(struct psmouse *psmouse,
				    unsigned char cmd, unsigned char *param)
{
	struct cytp_data *cytp = psmouse->private;
	bool rate_match = 0;
	bool resolution_match = 0;
	int i;

	/* callers will do further checking. */
	if ((cmd == CYTP_CMD_READ_CYPRESS_ID) ||
	    (cmd == CYTP_CMD_STANDARD_MODE) ||
	    (cmd == CYTP_CMD_READ_VITAL_STATISTICS))
		return 0;
	if (((~param[0] & DFLT_RESP_BITS_VALID) == DFLT_RESP_BITS_VALID) &&
	    ((param[0] & DFLT_RESP_BIT_MODE) == DFLT_RESP_STREAM_MODE)) {
		for (i = 0; i < sizeof(cytp_resolution); i++)
			if (cytp_resolution[i] == param[1])
				resolution_match =  1;

		for (i = 0; i < sizeof(cytp_rate); i++)
			if (cytp_rate[i] == param[2])
				rate_match = 1;

		if (resolution_match && rate_match)
			return 0;
	}

	cytp_dbg("verify cmd state failed.\n");
	return -1;
}

static int cypress_send_ext_cmd(struct psmouse *psmouse, unsigned char cmd,
				unsigned char *param)
{
	struct cytp_data *cytp = psmouse->private;
	int tries = CYTP_PS2_CMD_TRIES;
	int rc;

	cytp_dbg("send extension cmd 0x%02x, [%d %d %d %d]\n",
		 cmd, DECODE_CMD_AA(cmd), DECODE_CMD_BB(cmd),
		 DECODE_CMD_CC(cmd), DECODE_CMD_DD(cmd));
	do {
		cypress_ps2_ext_cmd(psmouse,
				    PSMOUSE_CMD_SETRES, DECODE_CMD_DD(cmd));
		cypress_ps2_ext_cmd(psmouse,
				    PSMOUSE_CMD_SETRES, DECODE_CMD_CC(cmd));
		cypress_ps2_ext_cmd(psmouse,
				    PSMOUSE_CMD_SETRES, DECODE_CMD_BB(cmd));
		cypress_ps2_ext_cmd(psmouse,
				    PSMOUSE_CMD_SETRES, DECODE_CMD_AA(cmd));

		rc = cypress_ps2_read_cmd_status(psmouse, cmd, param);
		if ((rc == 0) &&
		    (cypress_verify_cmd_state(psmouse, cmd, param) == 0))
			break;
	} while (--tries > 0);


	if (tries <= 0)
		return -1;

	return 0;

}

int cypress_detect(struct psmouse *psmouse, bool set_properties)
{
	unsigned char param[3];

	if (cypress_send_ext_cmd(psmouse, CYTP_CMD_READ_CYPRESS_ID, param))
		return -1;

	if (!IS_CYPRESS_KEY(param))
		return -ENODEV;

	if (set_properties) {
		psmouse->vendor = "Cypress";
		psmouse->name = "Trackpad";
	}

	return 0;
}

static int cypress_read_fw_version(struct psmouse *psmouse)
{
	struct cytp_data *cytp = psmouse->private;
	unsigned char param[3];

	if (cypress_send_ext_cmd(psmouse, CYTP_CMD_READ_CYPRESS_ID, param))
		return -1;

	if (!IS_CYPRESS_KEY(param))
		return -ENODEV;

	cytp->fw_version = param[2] & FW_VERSION_MASX;
	cytp->vital_statics_supported = (param[2] & VITAL_STATICS_MASK) ? 1 : 0;

	/*
	 * Trackpad fw_version 11 (in Dell XPS12) yields a bogus response to
	 * CYTP_CMD_READ_VITAL_STATISTICS so do not try to use it. LP: #1103594.
	 */
	if (cytp->fw_version >= 11)
		cytp->vital_statics_supported = 0;

	cytp_dbg("cytp->fw_version = %d\n", cytp->fw_version);
	cytp_dbg("cytp->vital_statics_supported = %d\n",
		 cytp->vital_statics_supported);
	return 0;
}

static int cypress_read_vital_statistics(struct psmouse *psmouse)
{
	struct cytp_data *cytp = psmouse->private;
	unsigned char param[8];

	/* set default values for vital statistics not supported trackpad. */
	cytp->tp_width = CYTP_DEFAULT_WIDTH;
	cytp->tp_high = CYTP_DEFAULT_HIGH;
	cytp->tp_max_abs_x = CYTP_ABS_MAX_X;
	cytp->tp_max_abs_y = CYTP_ABS_MAX_Y;
	cytp->tp_min_pressure = CYTP_MIN_PRESSURE;
	cytp->tp_max_pressure = CYTP_MAX_PRESSURE;
	cytp->tp_res_x = cytp->tp_max_abs_x / cytp->tp_width;
	cytp->tp_res_y = cytp->tp_max_abs_y / cytp->tp_high;

	if (!cytp->vital_statics_supported)
		return 0;

	memset(param, 0, sizeof(param));
	if (cypress_send_ext_cmd(psmouse, CYTP_CMD_READ_VITAL_STATISTICS, param) == 0) {
		/* Update trackpad parameters. */
		cytp->tp_max_abs_x = (param[1] << 8) | param[0];
		cytp->tp_max_abs_y = (param[3] << 8) | param[2];
		cytp->tp_min_pressure = param[4];
		cytp->tp_max_pressure = param[5];

		if (param[6] & VITAL_BIT_APA)
			cytp->tp_type = CYTP_APA;
		else if (param[6] & VITAL_BIT_MTG)
			cytp->tp_type = CYTP_MTG;
		else
			cytp->tp_type = CYTP_STG;
		cytp->tp_palm = (param[6] & VITAL_BIT_PALM) ? 1 : 0;
		cytp->tp_stubborn = (param[6] & VITAL_BIT_STUBBORN) ? 1 : 0;
		cytp->tp_2f_jitter = (param[6] & VITAL_BIT_2F_JITTER) >> 4;
		cytp->tp_1f_jitter = (param[6] & VITAL_BIT_1F_JITTER) >> 2;
		cytp->tp_abs_packet_format_set =
			(param[7] & VITAL_BIT_ABS_PKT_FORMAT_SET) >> 4;
		cytp->tp_2f_spike = (param[7] & VITAL_BIT_2F_SPIKE) >> 2;
		cytp->tp_1f_spike = (param[7] & VITAL_BIT_1F_SPIKE);

	}

	if (!cytp->tp_max_pressure ||
	    (cytp->tp_max_pressure < cytp->tp_min_pressure) ||
	    (!cytp->tp_width || !cytp->tp_high) ||
	    (!cytp->tp_max_abs_x) ||
	    (cytp->tp_max_abs_x < cytp->tp_width) ||
	    (!cytp->tp_max_abs_y) ||
	    (cytp->tp_max_abs_y < cytp->tp_high))
		return -1;

	cytp->tp_res_x = cytp->tp_max_abs_x / cytp->tp_width;
	cytp->tp_res_y = cytp->tp_max_abs_y / cytp->tp_high;

	cytp_dbg_dump("Dump trackpad hardware configuration as below:\n");
	cytp_dbg_dump("cytp->tp_width = %d\n", cytp->tp_width);
	cytp_dbg_dump("cytp->tp_high = %d\n", cytp->tp_high);
	cytp_dbg_dump("cytp->tp_max_abs_x = %d\n", cytp->tp_max_abs_x);
	cytp_dbg_dump("cytp->tp_max_abs_y = %d\n", cytp->tp_max_abs_y);
	cytp_dbg_dump("cytp->tp_min_pressure = %d\n", cytp->tp_min_pressure);
	cytp_dbg_dump("cytp->tp_max_pressure = %d\n", cytp->tp_max_pressure);
	cytp_dbg_dump("cytp->tp_res_x = %d\n", cytp->tp_res_x);
	cytp_dbg_dump("cytp->tp_res_y = %d\n", cytp->tp_res_y);
	cytp_dbg_dump("cytp->tp_type = %d\n", cytp->tp_type);
	cytp_dbg_dump("cytp->tp_palm = %d\n", cytp->tp_palm);
	cytp_dbg_dump("cytp->tp_stubborn = %d\n", cytp->tp_stubborn);
	cytp_dbg_dump("cytp->tp_1f_jitter = %d\n", cytp->tp_1f_jitter);
	cytp_dbg_dump("cytp->tp_2f_jitter = %d\n", cytp->tp_2f_jitter);
	cytp_dbg_dump("cytp->tp_1f_spike = %d\n", cytp->tp_1f_spike);
	cytp_dbg_dump("cytp->tp_2f_spike = %d\n", cytp->tp_2f_spike);
	cytp_dbg_dump("cytp->tp_abs_packet_format_set = %d\n",
		      cytp->tp_abs_packet_format_set);

	return 0;
}

static int cypress_query_hardware(struct psmouse *psmouse)
{
	int ret;

	if (cypress_read_fw_version(psmouse))
		return -1;

	ret = cypress_read_vital_statistics(psmouse);
	if (ret)
		return -1;

	return 0;
}

static int cypress_set_absolute_mode(struct psmouse *psmouse)
{
	struct cytp_data *cytp = psmouse->private;
	unsigned char param[3];

	if (cypress_send_ext_cmd(psmouse, CYTP_CMD_ABS_WITH_PRESSURE_MODE, param) < 0)
		return -1;

	CYTP_SET_MODE_BIT(CYTP_BIT_ABS_PRESSURE);
	CYTP_SET_PACKET_SIZE(5);

	return 0;
}

/*
 * reset trackpad device to standard relative mode.
 * This is also the defalut mode when trackpad powered on.
 */
static void cypress_reset(struct psmouse *psmouse)
{
	struct cytp_data *cytp = psmouse->private;

	psmouse_reset(psmouse);

	CYTP_SET_MODE_BIT(CYTP_BIT_STANDARD_REL);
	CYTP_SET_PACKET_SIZE(3);

	cytp->prev_contact_cnt = 0;
}

static int cypress_set_input_params(struct input_dev *input,
				    struct cytp_data *cytp)
{
	int ret;

	if (cytp->mode & CYTP_BIT_ABS_MASK) {
		__set_bit(EV_ABS, input->evbit);
		input_set_abs_params(input, ABS_X, 0, cytp->tp_max_abs_x, 0, 0);
		input_set_abs_params(input, ABS_Y, 0, cytp->tp_max_abs_y, 0, 0);
		input_set_abs_params(input, ABS_PRESSURE,
				     cytp->tp_min_pressure, cytp->tp_max_pressure, 0, 0);
		input_set_abs_params(input, ABS_TOOL_WIDTH, 0, 255, 0, 0);

		/* finger position */
		input_set_abs_params(input, ABS_MT_POSITION_X, 0, cytp->tp_max_abs_x, 0, 0);
		input_set_abs_params(input, ABS_MT_POSITION_Y, 0, cytp->tp_max_abs_y, 0, 0);
		input_set_abs_params(input, ABS_MT_PRESSURE, 0, 255, 0, 0);

		ret = input_mt_init_slots(input, CYTP_MAX_MT_SLOTS);
		if (ret < 0) {
			return ret;
		}

		if (cytp->tp_res_x && cytp->tp_res_x) {
			input_abs_set_res(input, ABS_X, cytp->tp_res_x);
			input_abs_set_res(input, ABS_Y, cytp->tp_res_y);

			input_abs_set_res(input, ABS_MT_POSITION_X,
					  cytp->tp_res_x);
			input_abs_set_res(input, ABS_MT_POSITION_Y,
					  cytp->tp_res_y);

		}

		__set_bit(INPUT_PROP_BUTTONPAD, input->propbit);
		__set_bit(EV_KEY, input->evbit);
		__set_bit(BTN_TOUCH, input->keybit);
		__set_bit(BTN_TOOL_FINGER, input->keybit);
		__set_bit(BTN_TOOL_DOUBLETAP, input->keybit);
		__set_bit(BTN_TOOL_TRIPLETAP, input->keybit);
		__set_bit(BTN_TOOL_QUADTAP, input->keybit);
		__set_bit(BTN_TOOL_QUINTTAP, input->keybit);

		__set_bit(BTN_LEFT, input->keybit);
		__set_bit(BTN_RIGHT, input->keybit);
		__set_bit(BTN_MIDDLE, input->keybit);

		__clear_bit(EV_REL, input->evbit);
		__clear_bit(REL_X, input->relbit);
		__clear_bit(REL_Y, input->relbit);
	} else {
		__set_bit(INPUT_PROP_BUTTONPAD, input->propbit);
		__set_bit(EV_REL, input->evbit);
		__set_bit(REL_X, input->relbit);
		__set_bit(REL_Y, input->relbit);
		__set_bit(REL_WHEEL, input->relbit);
		__set_bit(REL_HWHEEL, input->relbit);

		__set_bit(EV_KEY, input->evbit);
		__set_bit(BTN_LEFT, input->keybit);
		__set_bit(BTN_RIGHT, input->keybit);
		__set_bit(BTN_MIDDLE, input->keybit);

		__clear_bit(EV_ABS, input->evbit);
	}

	input_set_drvdata(input, cytp);

	return 0;
}

static int cypress_get_finger_count(unsigned char header_byte)
{
	unsigned char bits6_7;
	int finger_count;

	bits6_7 = header_byte >> 6;
	finger_count = bits6_7 & 0x03;

	if (finger_count != 1) {
		if (header_byte & ABS_HSCROLL_BIT) {
			if (finger_count == 0) {
				/* HSCROLL gets added on to 0 finger count. */
				finger_count = 4;
				/* should remove HSCROLL bit. */
			} else {
				if (finger_count == 2) {
					finger_count = 5;
				} else {
					/* Invalid contact (e.g. palm). Ignore it. */
					finger_count = 0;
				}
			}
		}
	}

	return finger_count;
}

static int new_slot_id(void)
{
	static int id = -1;

	id++;
	if (id >= CYTP_MAX_MT_SLOTS)
		id = 0;

	return id;
}

#define FINGER_MAX_JITTER_DISTANCE 25
static inline bool same_finger(struct cytp_contact *prev_contact,
			       struct cytp_contact *contact)
{
	if (abs(prev_contact->x - contact->x) < FINGER_MAX_JITTER_DISTANCE) {
		if (abs(prev_contact->y - contact->y) < FINGER_MAX_JITTER_DISTANCE) {
			return true;
		}
	}

	return false;
}

#define ID_NULL	  ((unsigned char)0x00)
#define ID_NEW	  ((unsigned char)0x01)
#define ID_KEEP	  ((unsigned char)0x02)
#define ID_UPDATE ((unsigned char)0x03)
static const unsigned char
new_slot_id_flags[CYTP_MAX_CONTACTS+1][CYTP_MAX_CONTACTS+1] = {
	/* previous contact_cnt = 0. */
	{ ID_NULL, ID_NEW, ID_NEW, ID_NEW, ID_NEW, ID_NEW },

	/* previous contact_cnt = 1. */
	{ ID_NULL, ID_KEEP, ID_UPDATE, ID_NEW, ID_NEW, ID_NEW },

	/* previous contact_cnt = 2. */
	{ ID_NULL, ID_UPDATE, ID_KEEP, ID_NEW, ID_NEW, ID_NEW },

	/* previous contact_cnt = 3. */
	{ ID_NULL, ID_NEW, ID_NEW, ID_KEEP, ID_NEW, ID_NEW },

	/* previous contact_cnt = 4. */
	{ ID_NULL, ID_NEW, ID_NEW, ID_NEW, ID_KEEP, ID_NEW },

	/* previous contact_cnt = 5. */
	{ ID_NULL, ID_NEW, ID_NEW, ID_NEW, ID_NEW, ID_KEEP },
};

static int cypress_cal_finger_id(struct cytp_data *cytp,
				 struct cytp_report_data *new_report)
{
	int i, j;
	bool slot_id_updated;
	struct cytp_report_data *prev_report = &cytp->prev_report_data;
	unsigned char flag =
		new_slot_id_flags[prev_report->contact_cnt][new_report->contact_cnt];

	if (flag == ID_NEW) {
		for (i = 0; i < new_report->contact_cnt; i++) {
			new_report->contacts[i].id = new_slot_id();
		}

	} else if (flag == ID_KEEP) {
		/* firmware will ensure that the finger id is report in same order. */
		for (i = 0; i < new_report->contact_cnt; i++) {
			new_report->contacts[i].id = prev_report->contacts[i].id;
		}

	} else if (flag == ID_UPDATE) {
		for (i = 0; i < new_report->contact_cnt; i++) {
			slot_id_updated = false;
			for (j = 0; j < prev_report->contact_cnt; j++) {
				if (same_finger(&new_report->contacts[i],
						&prev_report->contacts[j])) {
					new_report->contacts[i].id = prev_report->contacts[j].id;
					slot_id_updated = true;
					break;
				}
			}

			if (!slot_id_updated)
				new_report->contacts[i].id = new_slot_id();
		}
	}

	if (new_report->contact_cnt == 0)
		cytp->zero_packet_cnt++;
	else
		cytp->zero_packet_cnt = 0;
	cytp->prev_report_data = *new_report;

	return 0;
}

static int position_adjust_array[4] = {
	50, -50, 100, -100
};
static void cypress_simulate_fingers(struct cytp_data *cytp,
				     struct cytp_report_data *report_data)
{
	int i;

	if (report_data->contact_cnt >= 3) {
		for (i = 1; i < report_data->contact_cnt; i++) {
			report_data->contacts[i].x =
				report_data->contacts[0].x + position_adjust_array[i - 1];
			report_data->contacts[i].y = report_data->contacts[0].y;
			report_data->contacts[i].z = report_data->contacts[0].z;

			if (report_data->contacts[i].x < 0)
				report_data->contacts[i].x = 0;
			if (report_data->contacts[i].x > cytp->tp_max_abs_x)
				report_data->contacts[i].x = cytp->tp_max_abs_x;
		}
	}
}

static int cypress_parse_packet(const unsigned char packet[],
				struct cytp_data *cytp, struct cytp_report_data *report_data)
{
	int i;
	unsigned char header_byte = packet[0];

	memset(report_data, 0, sizeof(struct cytp_report_data));
	if (cytp->mode & CYTP_BIT_ABS_MASK) {
		report_data->contact_cnt = cypress_get_finger_count(header_byte);

		if (report_data->contact_cnt > CYTP_MAX_CONTACTS) {
			/* report invalid data as zero package except the button data. */
			report_data->contact_cnt = 0;
			cytp_dbg("cypress_parse_packet: received invalid packet.\n");
		}

		report_data->tap = (header_byte & ABS_MULTIFINGER_TAP) ? 1 : 0;

		/* Remove HSCROLL bit */
		if (report_data->contact_cnt == 4)
			header_byte &= ~(ABS_HSCROLL_BIT);

		if (report_data->contact_cnt == 1) {
			report_data->contacts[0].x =
				((packet[1] & 0x70) << 4) | packet[2];
			report_data->contacts[0].y =
				((packet[1] & 0x07) << 8) | packet[3];
			if (cytp->mode & CYTP_BIT_ABS_PRESSURE)
				report_data->contacts[0].z = packet[4];

			if ((packet[1] & ABS_EDGE_MOTION_MASK) != ABS_EDGE_MOTION_MASK) {
				report_data->vscroll = (header_byte & ABS_VSCROLL_BIT) ? 1 : 0;
				report_data->hscroll = (header_byte & ABS_HSCROLL_BIT) ? 1 : 0;
			}

		} else if (report_data->contact_cnt == 2) {
			report_data->contacts[0].x =
				((packet[1] & 0x70) << 4) | packet[2];
			report_data->contacts[0].y =
				((packet[1] & 0x07) << 8) | packet[3];
			if (cytp->mode & CYTP_BIT_ABS_PRESSURE)
				report_data->contacts[0].z = packet[4];

			report_data->contacts[1].x =
				((packet[5] & 0xf0) << 4) | packet[6];
			report_data->contacts[1].y =
				((packet[5] & 0x0f) << 8) | packet[7];
			if (cytp->mode & CYTP_BIT_ABS_PRESSURE)
				report_data->contacts[1].z = report_data->contacts[0].z;

		} else if (report_data->contact_cnt >= 3) {
			report_data->contacts[0].x =
				((packet[1] & 0xf0) << 4) | packet[2];
			report_data->contacts[0].y =
				((packet[1] & 0x0f) << 8) | packet[3];
			if (cytp->mode & CYTP_BIT_ABS_PRESSURE)
				report_data->contacts[0].z = packet[4];

			cypress_simulate_fingers(cytp, report_data);

			report_data->vscroll = (header_byte & ABS_VSCROLL_BIT) ? 1 : 0;
		}

		cypress_cal_finger_id(cytp, report_data);

		report_data->left = (header_byte & BTN_LEFT_BIT) ? 1 : 0;
		report_data->right = (header_byte & BTN_RIGHT_BIT) ? 1 : 0;

	} else {
		report_data->contact_cnt = 1;
		report_data->contacts[0].x =
			(packet[0] & REL_X_SIGN_BIT) ? -packet[1] : packet[1];
		report_data->contacts[0].y =
			(packet[0] & REL_Y_SIGN_BIT) ? -packet[2] : packet[2];
		report_data->vscroll = packet[3];
		report_data->left = (packet[0] & BTN_LEFT_BIT) ? 1 : 0;
		report_data->right = (packet[0] & BTN_RIGHT_BIT) ? 1 : 0;

		if (cytp->mode & CYTP_BIT_STANDARD_REL)
			report_data->middle =
				(packet[0] & BTN_MIDDLE_BIT) ? 1 : 0;
		if (cytp->mode & CYTP_BIT_CYPRESS_REL) {
			report_data->left =
				(packet[0] & BTN_MIDDLE_BIT) ? 1 : 0;
			report_data->hscroll = packet[4];
		}
	}

	/* This is only true if one of the mouse buttons were tapped.
	 * Make sure it doesn't turn into a click. The regular tap-to-
	 * click functionality will handle that on its own. If we don't
	 * do this, disabling tap-to-click won't affect the mouse button
	 * zones. */
	if (report_data->tap)
		report_data->left = 0;

	if (report_data->contact_cnt <= 0)
		return 0;

	cytp_dbg_dump("cypress_parse_packet cytp->zero_packet_cnt = %d\n", cytp->zero_packet_cnt);
	cytp_dbg_dump("Dump parsed report data as below:\n");
	cytp_dbg_dump("contact_cnt = %d\n", report_data->contact_cnt);
	for (i = 0; i < report_data->contact_cnt; i++) {
		cytp_dbg_dump("contacts[%d].x = %d\n", i, report_data->contacts[i].x);
		cytp_dbg_dump("contacts[%d].y = %d\n", i, report_data->contacts[i].y);
		cytp_dbg_dump("contacts[%d].z = %d\n", i, report_data->contacts[i].z);
		cytp_dbg_dump("contacts[%d].id = %d\n", i, report_data->contacts[i].id);
	}
	cytp_dbg_dump("vscroll = %d\n", report_data->vscroll);
	cytp_dbg_dump("hscroll = %d\n", report_data->hscroll);
	cytp_dbg_dump("left = %d\n", report_data->left);
	cytp_dbg_dump("right = %d\n", report_data->right);
	cytp_dbg_dump("middle = %d\n", report_data->middle);

	return 0;
}

static void cypress_process_packet(struct psmouse *psmouse, bool zero_pkt)
{
	int i;
	struct input_dev *input = psmouse->dev;
	struct cytp_data *cytp = psmouse->private;
	struct cytp_report_data report_data;
	unsigned int mask;
	struct cytp_contact *contact;
	int slot;

	if (cypress_parse_packet(psmouse->packet, cytp, &report_data))
		return;

	if (cytp->mode & CYTP_BIT_ABS_MASK) {
		mask = 0;
		for (i = 0; i < report_data.contact_cnt; i++) {
			contact = &report_data.contacts[i];
			slot = contact->id;

			mask |= (1 << slot);
			input_mt_slot(input, slot);
			input_mt_report_slot_state(input, MT_TOOL_FINGER, true);
			input_report_abs(input, ABS_MT_POSITION_X, contact->x);
			input_report_abs(input, ABS_MT_POSITION_Y, contact->y);
			input_report_abs(input, ABS_MT_PRESSURE, contact->z);
		}

		/* Invalidate all unreported slots */
		for (i = 0; i < CYTP_MAX_MT_SLOTS; i++) {
			if (mask & (1 << i))
				continue;

			input_mt_slot(input, i);
			input_mt_report_slot_state(input, MT_TOOL_FINGER, false);
		}

		input_mt_report_pointer_emulation(input, true);

		input_report_key(input, BTN_LEFT, report_data.left);
		input_report_key(input, BTN_RIGHT, report_data.right);
		input_report_key(input, BTN_MIDDLE, report_data.middle);

		input_sync(input);

	} else {
		if (report_data.contact_cnt == 1) {
			input_report_rel(input, REL_X, report_data.contacts[0].x);
			input_report_rel(input, REL_Y, report_data.contacts[0].y);
		}

		input_report_rel(input, REL_WHEEL, report_data.vscroll);
		if (cytp->mode & CYTP_BIT_CYPRESS_REL)
			input_report_rel(input, REL_HWHEEL, report_data.hscroll);

		input_report_key(input, BTN_LEFT, report_data.left);
		input_report_key(input, BTN_RIGHT, report_data.right);
		input_report_key(input, BTN_MIDDLE, report_data.middle);

		input_sync(input);
	}
}

static psmouse_ret_t cypress_validate_byte(struct psmouse *psmouse)
{
	int contact_cnt;
	int index = psmouse->pktcnt - 1;
	unsigned char *packet = psmouse->packet;
	struct cytp_data *cytp = psmouse->private;

	if (index < 0 || index > cytp->pkt_size)
		return PSMOUSE_BAD_DATA;

	if ((index == 0) && ((packet[0] & 0xfc) == 0)) {
		/* call packet process for reporting finger leave. */
		cypress_process_packet(psmouse, 1);
		return PSMOUSE_FULL_PACKET;
	}

	if (cytp->mode & CYTP_BIT_ABS_MASK) {
		if (index == 0) {
			if ((packet[0] & 0x08) == 0x08)
				return PSMOUSE_BAD_DATA;

			contact_cnt = cypress_get_finger_count(packet[0]);

			if (contact_cnt > 5)
				return PSMOUSE_BAD_DATA;

			if (cytp->mode & CYTP_BIT_ABS_NO_PRESSURE) {
				CYTP_SET_PACKET_SIZE(4);
				if (contact_cnt == 2)
					CYTP_SET_PACKET_SIZE(7);
			} else {
				CYTP_SET_PACKET_SIZE(5);
				if (contact_cnt == 2)
					CYTP_SET_PACKET_SIZE(8);
			}
		}

		return PSMOUSE_GOOD_DATA;
	} else {
		if (index == 0) {
			if ((packet[0] & 0x08) != 0x08)
				return PSMOUSE_BAD_DATA;

			CYTP_SET_PACKET_SIZE(3);
			if (cytp->mode & CYTP_BIT_CYPRESS_REL)
				CYTP_SET_PACKET_SIZE(5);
		}

		return PSMOUSE_GOOD_DATA;
	}
}

static psmouse_ret_t cypress_protocol_handler(struct psmouse *psmouse)
{
	struct cytp_data *cytp = psmouse->private;

	if (psmouse->pktcnt >= cytp->pkt_size) {
		cypress_process_packet(psmouse, 0);
		return PSMOUSE_FULL_PACKET;
	}

	return cypress_validate_byte(psmouse);
}

static void cypress_set_rate(struct psmouse *psmouse, unsigned int rate)
{
	struct cytp_data *cytp = psmouse->private;

	if (rate >= 80) {
		psmouse->rate = 80;
		CYTP_SET_MODE_BIT(CYTP_BIT_HIGH_RATE);
	} else {
		psmouse->rate = 40;
		CYTP_CLEAR_MODE_BIT(CYTP_BIT_HIGH_RATE);
	}

	ps2_command(&psmouse->ps2dev, (unsigned char *)&psmouse->rate,
		    PSMOUSE_CMD_SETRATE);
}

static void cypress_disconnect(struct psmouse *psmouse)
{
	cypress_reset(psmouse);
	kfree(psmouse->private);
	psmouse->private = NULL;
}

static int cypress_reconnect(struct psmouse *psmouse)
{
	int tries = CYTP_PS2_CMD_TRIES;
	int rc;

	do {
		cypress_reset(psmouse);
		rc = cypress_detect(psmouse, false);
	} while (rc && (--tries > 0));

	if (rc)
		return -1;

	if (cypress_query_hardware(psmouse)) {
		pr_err("Reconnect: unable to query Trackpad hardware.\n");
		return -1;
	}

	if (cypress_set_absolute_mode(psmouse)) {
		pr_err("Reconnect: Unable to initialize Cypress absolute mode.\n");
		return -1;
	}

	return 0;
}

int cypress_init(struct psmouse *psmouse)
{
	struct cytp_data *cytp;

	cytp = (struct cytp_data *)kzalloc(sizeof(struct cytp_data), GFP_KERNEL);
	psmouse->private = (void *)cytp;
	if (cytp == NULL)
		return -ENOMEM;

	cypress_reset(psmouse);

	if (cypress_query_hardware(psmouse)) {
		pr_err("Unable to query Trackpad hardware.\n");
		goto err_exit;
	}

	if (cypress_set_absolute_mode(psmouse)) {
		pr_err("Reconnect: Unable to initialize Cypress absolute mode.\n");
		goto err_exit;
	}

	if (cypress_set_input_params(psmouse->dev, cytp) < 0)
		return -1;

	psmouse->model = 1;
	psmouse->protocol_handler = cypress_protocol_handler;
	psmouse->set_rate = cypress_set_rate;
	psmouse->disconnect = cypress_disconnect;
	psmouse->reconnect = cypress_reconnect;
	psmouse->cleanup = cypress_reset;
	psmouse->pktsize = 8;
	psmouse->resync_time = 0;

	return 0;

err_exit:
	/*
	 * Reset Cypress Trackpad as a standard mouse. Then
	 * let psmouse driver commmunicating with it as default PS2 mouse.
	 */
	cypress_reset(psmouse);

	psmouse->private = NULL;
	kfree(cytp);

	return -1;
}

bool cypress_supported(void)
{
	return true;
}
