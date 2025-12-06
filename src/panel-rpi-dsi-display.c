/*
** Copyright (C) CNflysky. All rights reserved.
*/

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <video/mipi_display.h>
#include <linux/of.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/slab.h>

struct power_on_timing {
	unsigned long post_reset;
	unsigned long reset_low;
	unsigned long after_reset;
	unsigned long slpout;
};

struct rpi_dsi_display_desc {
	const struct drm_display_mode *mode;
	unsigned int lanes;
	unsigned long flags;
	enum mipi_dsi_pixel_format format;
	int (*init_sequence)(struct mipi_dsi_device *dsi);
	const struct power_on_timing *pwr_timing;
	bool do_sw_reset;
};

struct rpi_dsi_display {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	const struct rpi_dsi_display_desc *desc;
	struct gpio_desc *reset;
	enum drm_panel_orientation orientation;
};

inline static struct rpi_dsi_display *
to_rpi_dsi_display(struct drm_panel *panel)
{
	return container_of(panel, struct rpi_dsi_display, panel);
}

/* 辅助函数：发送带多个参数的命令 */
static int dcs_write_command(struct mipi_dsi_device *dsi, const u8 *data, size_t len)
{
	int ret;
	
	if (len == 0)
		return -EINVAL;
	
	/* 如果只有一个字节，发送无参数的命令 */
	if (len == 1) {
		ret = mipi_dsi_dcs_write(dsi, data[0], NULL, 0);
	} else {
		/* 发送命令和参数 */
		ret = mipi_dsi_dcs_write(dsi, data[0], &data[1], len - 1);
	}
	
	return ret;
}

/* 宏：发送序列命令 */
#define SEND_CMD(dsi, ...) \
	do { \
		u8 cmd_data[] = {__VA_ARGS__}; \
		ret = dcs_write_command(dsi, cmd_data, ARRAY_SIZE(cmd_data)); \
		if (ret < 0) error_count++; \
	} while (0)

inline static int w280bf036i_init_sequence(struct mipi_dsi_device *dsi)
{
	int ret = 0;
	int error_count = 0;
	
	/* Command2 BK3 Selection: Enable the BK function of Command2 */
	SEND_CMD(dsi, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x13);
	
	/* Unknown */
	SEND_CMD(dsi, 0xEF, 0x08);
	
	/* Command2 BK0 Selection: Disable the BK function of Command2 */
	SEND_CMD(dsi, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x10);
	
	/* Display Line Setting */
	SEND_CMD(dsi, 0xC0, 0x4f, 0x00);
	
	/* Porch Control */
	SEND_CMD(dsi, 0xC1, 0x10, 0x0c);
	
	/* Inversion selection & Frame Rate Control */
	SEND_CMD(dsi, 0xC2, 0x07, 0x14);
	
	/* Unknown */
	SEND_CMD(dsi, 0xCC, 0x10);
	
	/* Positive Voltage Gamma Control */
	{
		u8 params[] = {0xB0, 0x0a, 0x18, 0x1e, 0x12, 0x16,
			       0x0c, 0x0e, 0x0d, 0x0c, 0x29, 0x06, 0x14,
			       0x13, 0x29, 0x33, 0x1c};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}
	
	/* Negative Voltage Gamma Control */
	{
		u8 params[] = {0xB1, 0x0a, 0x19, 0x21, 0x0a, 0x0c,
			       0x00, 0x0c, 0x03, 0x03, 0x23, 0x01, 0x0e,
			       0x0c, 0x27, 0x2b, 0x1c};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}

	/* Command2 BK1 Selection: Enable the BK function of Command2 */
	SEND_CMD(dsi, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x11);
	
	/* Vop Amplitude setting */
	SEND_CMD(dsi, 0xB0, 0x5d);
	
	/* VCOM amplitude setting */
	SEND_CMD(dsi, 0xB1, 0x61);
	
	/* VGH Voltage setting */
	SEND_CMD(dsi, 0xB2, 0x84);
	
	/* TEST Command Setting */
	SEND_CMD(dsi, 0xB3, 0x80);
	
	/* VGL Voltage setting */
	SEND_CMD(dsi, 0xB5, 0x4d);
	
	/* Power Control 1 */
	SEND_CMD(dsi, 0xB7, 0x85);
	
	/* Power Control 2 */
	SEND_CMD(dsi, 0xB8, 0x20);
	
	/* Source pre_drive timing set1 */
	SEND_CMD(dsi, 0xC1, 0x78);
	
	/* Source EQ2 Setting */
	SEND_CMD(dsi, 0xC2, 0x78);
	
	/* MIPI Setting 1 */
	SEND_CMD(dsi, 0xD0, 0x88);
	
	/* GIP Code */
	SEND_CMD(dsi, 0xE0, 0x00, 0x00, 0x02);
	
	{
		u8 params[] = {0xE1, 0x06, 0xa0, 0x08, 0xa0, 0x05,
			       0xa0, 0x07, 0xa0, 0x00, 0x44, 0x44};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}
	
	{
		u8 params[] = {0xE2, 0x20, 0x20, 0x44, 0x44, 0x96,
			       0xa0, 0x00, 0x00, 0x96, 0xa0, 0x00, 0x00};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}
	
	SEND_CMD(dsi, 0xE3, 0x00, 0x00, 0x22, 0x22);
	
	SEND_CMD(dsi, 0xE4, 0x44, 0x44);
	
	{
		u8 params[] = {0xE5, 0x0d, 0x91, 0xa0, 0xa0, 0x0f,
			       0x93, 0xa0, 0xa0, 0x09, 0x8d, 0xa0, 0xa0,
			       0x0b, 0x8f, 0xa0, 0xa0};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}
	
	SEND_CMD(dsi, 0xE6, 0x00, 0x00, 0x22, 0x22);
	
	SEND_CMD(dsi, 0xE7, 0x44, 0x44);
	
	{
		u8 params[] = {0xE8, 0x0c, 0x90, 0xa0, 0xa0, 0x0e,
			       0x92, 0xa0, 0xa0, 0x08, 0x8c, 0xa0, 0xa0,
			       0x0a, 0x8e, 0xa0, 0xa0};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}
	
	SEND_CMD(dsi, 0xE9, 0x36, 0x00);
	
	{
		u8 params[] = {0xEB, 0x00, 0x01, 0xe4, 0xe4, 0x44,
			       0x88, 0x40};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}
	
	{
		u8 params[] = {0xED, 0xff, 0x45, 0x67, 0xfa, 0x01,
			       0x2b, 0xcf, 0xff, 0xff, 0xfc, 0xb2, 0x10,
			       0xaf, 0x76, 0x54, 0xff};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}
	
	{
		u8 params[] = {0xEF, 0x10, 0x0d, 0x04, 0x08, 0x3f,
			       0x1f};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}
	
	/* disable Command2 */
	SEND_CMD(dsi, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x00);
	
	/* Set tearing effect on */
	SEND_CMD(dsi, 0x35, 0x00);
	
	return error_count;
}

/* 专门为3.97寸ST7701S屏幕创建的初始化序列 */
inline static int st7701s_397_init_sequence(struct mipi_dsi_device *dsi)
{
	int ret = 0;
	int error_count = 0;
	
	/* 增加初始延时，确保屏幕电源稳定 */
	msleep(50);
	
	/* Command2 BK3 Selection: Enable the BK function of Command2 */
	SEND_CMD(dsi, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x13);
	
	/* Unknown */
	SEND_CMD(dsi, 0xEF, 0x08);
	
	/* Command2 BK0 Selection: Disable the BK function of Command2 */
	SEND_CMD(dsi, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x10);
	
	/* Display Line Setting - 针对480x800分辨率优化 */
	SEND_CMD(dsi, 0xC0, 0x63, 0x00);
	
	/* Porch Control - 针对480x800分辨率优化 */
	SEND_CMD(dsi, 0xC1, 0x09, 0x02);
	
	/* Inversion selection & Frame Rate Control */
	SEND_CMD(dsi, 0xC2, 0x20, 0x02);
	
	/* Unknown */
	SEND_CMD(dsi, 0xCC, 0x18);
	
	/* Positive Voltage Gamma Control */
	{
		u8 params[] = {0xB0, 0x40, 0x0E, 0x51, 0x0F, 0x11, 0x07, 0x00,
			       0x09, 0x06, 0x1E, 0x04, 0x12, 0x11, 0x64, 0x29, 0xDF};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}
	
	/* Negative Voltage Gamma Control */
	{
		u8 params[] = {0xB1, 0x40, 0x07, 0x4C, 0x0A, 0x0E, 0x04, 0x00, 0x08,
			       0x08, 0x09, 0x1D, 0x01, 0x0E, 0x0c, 0x6A, 0x34, 0xDF};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}

	/* Command2 BK1 Selection: Enable the BK function of Command2 */
	SEND_CMD(dsi, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x11);
	
	/* Vop Amplitude setting */
	SEND_CMD(dsi, 0xB0, 0x30);
	
	/* VCOM amplitude setting */
	SEND_CMD(dsi, 0xB1, 0x48);
	
	/* VGH Voltage setting */
	SEND_CMD(dsi, 0xB2, 0x80);
	
	/* TEST Command Setting */
	SEND_CMD(dsi, 0xB3, 0x80);
	
	/* VGL Voltage setting */
	SEND_CMD(dsi, 0xB5, 0x4F);
	
	/* Power Control 1 */
	SEND_CMD(dsi, 0xB7, 0x85);
	
	/* Power Control 2 */
	SEND_CMD(dsi, 0xB8, 0x23);
	
	/* Power Control 3 */
	SEND_CMD(dsi, 0xB9, 0x22, 0x13);

	SEND_CMD(dsi, 0xBB, 0x03);
	
	SEND_CMD(dsi, 0xBC, 0x10);

	SEND_CMD(dsi, 0xC0, 0x89);
	
	/* Source pre_drive timing set1 */
	SEND_CMD(dsi, 0xC1, 0x78);
	
	/* Source EQ2 Setting */
	SEND_CMD(dsi, 0xC2, 0x78);

	SEND_CMD(dsi, 0xEF, 0x08, 0x08, 0x08, 0x4C, 0x3F, 0x54);
	
	/* MIPI Setting 1 */
	SEND_CMD(dsi, 0xD0, 0x88);
	
	/* Sunlight Readable Enhancement */
	SEND_CMD(dsi, 0xE0, 0x00, 0x00, 0x02);
	
	/* Noise Reduce Control */
	{
		u8 params[] = {0xE1, 0x04, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00,
			       0x00, 0x00, 0x10, 0x10};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}
	
	/* Sharpness Control */
	{
		u8 params[] = {0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}
	
	/* Color Calibration Control */
	SEND_CMD(dsi, 0xE3, 0x00, 0x00, 0x33, 0x00);
	
	/* Skin Tone Preservation Control */
	SEND_CMD(dsi, 0xE4, 0x22, 0x00);

	{
		u8 params[] = {0xE5, 0x03, 0x34, 0xAF, 0xB3, 0x05, 0x34, 0xAF,
			       0xB3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}
	
	SEND_CMD(dsi, 0xE6, 0x00, 0x00, 0x33, 0x00);
	
	SEND_CMD(dsi, 0xE7, 0x22, 0x00);
	
	{
		u8 params[] = {0xE8, 0x04, 0x34, 0xAF, 0xB3, 0x06, 0x34, 0xAF,
			       0xB3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}
	
	{
		u8 params[] = {0xEB, 0x02, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}
	
	SEND_CMD(dsi, 0xEC, 0x00, 0x00);
	
	{
		u8 params[] = {0xED, 0xFA, 0x45, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF,
			       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xB0, 0x54, 0xAF};
		ret = dcs_write_command(dsi, params, ARRAY_SIZE(params));
		if (ret < 0) error_count++;
	}
	
	SEND_CMD(dsi, 0xEF, 0x08, 0x08, 0x08, 0x45, 0x3F, 0x54);
	
	/* disable Command2 */
	SEND_CMD(dsi, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x13);

	SEND_CMD(dsi, 0xE6, 0x16, 0x7c);
	
	SEND_CMD(dsi, 0xE8, 0x00, 0x0E);
	
	SEND_CMD(dsi, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x00);

	/* 软件复位 */
	SEND_CMD(dsi, 0x11);
	
	/* 增加延时确保软件复位完成 */
	msleep(200);

	SEND_CMD(dsi, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x13);
	
	SEND_CMD(dsi, 0xE8, 0x00, 0x0C);
	
	msleep(30); /* 增加延时 */
	
	SEND_CMD(dsi, 0xE8, 0x00, 0x00);

	SEND_CMD(dsi, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x00);

	/* 设置 tearing effect on */
	SEND_CMD(dsi, 0x35, 0x00);
	
	/* 开启显示 */
	SEND_CMD(dsi, 0x29);

	/* 增加最终延时，确保显示稳定 */
	msleep(300);
	
	/* 如果错误次数过多，可能是屏幕未连接或硬件故障 */
	if (error_count > 15) {
		dev_warn(&dsi->dev, "Detected %d DSI command errors, panel may not be connected\n", error_count);
		return -ENODEV;
	}
	
	return error_count;
}

inline static int tdo_qhd0500d5_init_sequence(struct mipi_dsi_device *dsi)
{
	int ret = 0;
	int error_count = 0;
	
	/* enable backlight */
	SEND_CMD(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x2C);
	
	return error_count;
}

static int rpi_dsi_display_prepare(struct drm_panel *panel)
{
	struct rpi_dsi_display *rpi_dsi_display = to_rpi_dsi_display(panel);
	int retry_count = 0;
	int ret;
	
	/* 添加初始延时，确保电源稳定 */
	msleep(30);
	
	if (rpi_dsi_display->reset) {
		gpiod_set_value_cansleep(rpi_dsi_display->reset, 1);
		msleep(rpi_dsi_display->desc->pwr_timing->post_reset);
		gpiod_set_value_cansleep(rpi_dsi_display->reset, 0);
		msleep(rpi_dsi_display->desc->pwr_timing->reset_low);
		gpiod_set_value_cansleep(rpi_dsi_display->reset, 1);
		msleep(rpi_dsi_display->desc->pwr_timing->after_reset);
	}

	if (rpi_dsi_display->desc->do_sw_reset) {
		/* 添加软件复位 */
		ret = mipi_dsi_dcs_soft_reset(rpi_dsi_display->dsi);
		if (ret < 0) {
			dev_warn(panel->dev, "Soft reset failed: %d\n", ret);
		}
		msleep(rpi_dsi_display->desc->pwr_timing->after_reset);
	}

	/* 添加重试机制，但减少重试次数和延时，避免长时间阻塞系统启动 */
	while (retry_count < 2) { /* 减少到2次重试 */
		if (rpi_dsi_display->desc->init_sequence) {
			ret = rpi_dsi_display->desc->init_sequence(rpi_dsi_display->dsi);
			if (ret == 0) {
				break; /* 成功，退出重试循环 */
			}
			dev_warn(panel->dev, "Init sequence attempt %d failed, retrying...\n",
				 retry_count + 1);
			retry_count++;
			msleep(50); /* 减少重试延时 */
		}
	}
	
	if (retry_count >= 2) {
		dev_warn(panel->dev, "Panel initialization failed after 2 attempts, but continuing boot\n");
		/* 不返回错误，允许系统继续启动 */
	}
	
	/* 尝试退出睡眠模式 */
	ret = mipi_dsi_dcs_exit_sleep_mode(rpi_dsi_display->dsi);
	if (ret < 0) {
		dev_warn(panel->dev, "Exit sleep mode failed: %d\n", ret);
	}
	msleep(rpi_dsi_display->desc->pwr_timing->slpout);
	
	return 0; /* 总是返回成功，允许系统继续启动 */
}

inline static int rpi_dsi_display_enable(struct drm_panel *panel)
{
	struct rpi_dsi_display *rpi_dsi_display = to_rpi_dsi_display(panel);
	int ret = mipi_dsi_dcs_set_display_on(rpi_dsi_display->dsi);
	if (ret < 0) {
		dev_warn(panel->dev, "Set display on failed: %d\n", ret);
	}
	return 0;
}

inline static int rpi_dsi_display_disable(struct drm_panel *panel)
{
	struct rpi_dsi_display *rpi_dsi_display = to_rpi_dsi_display(panel);
	int ret = mipi_dsi_dcs_set_display_off(rpi_dsi_display->dsi);
	if (ret < 0) {
		dev_warn(panel->dev, "Set display off failed: %d\n", ret);
	}
	return ret;
}

static int rpi_dsi_display_unprepare(struct drm_panel *panel)
{
	struct rpi_dsi_display *rpi_dsi_display = to_rpi_dsi_display(panel);
	int ret = mipi_dsi_dcs_enter_sleep_mode(rpi_dsi_display->dsi);
	if (ret < 0) {
		dev_warn(panel->dev, "Enter sleep mode failed: %d\n", ret);
	}
	if (rpi_dsi_display->reset)
		gpiod_set_value_cansleep(rpi_dsi_display->reset, 0);
	return ret;
}

static int rpi_dsi_display_get_modes(struct drm_panel *panel,
				     struct drm_connector *connector)
{
	struct rpi_dsi_display *rpi_dsi_display = to_rpi_dsi_display(panel);
	const struct drm_display_mode *desc_mode = rpi_dsi_display->desc->mode;
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, desc_mode);
	if (!mode) {
		dev_err(&rpi_dsi_display->dsi->dev,
			"failed to add mode %ux%u@%u\n", desc_mode->hdisplay,
			desc_mode->vdisplay, drm_mode_vrefresh(desc_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = desc_mode->width_mm;
	connector->display_info.height_mm = desc_mode->height_mm;

	drm_connector_set_orientation_from_panel(connector, panel);
	return 1;
}

inline static enum drm_panel_orientation
rpi_dsi_display_get_orientation(struct drm_panel *panel)
{
	struct rpi_dsi_display *rpi_dsi_display = to_rpi_dsi_display(panel);
	return rpi_dsi_display->orientation;
}

static const struct drm_panel_funcs rpi_dsi_display_funcs = {
	.disable = rpi_dsi_display_disable,
	.unprepare = rpi_dsi_display_unprepare,
	.prepare = rpi_dsi_display_prepare,
	.enable = rpi_dsi_display_enable,
	.get_modes = rpi_dsi_display_get_modes,
	.get_orientation = rpi_dsi_display_get_orientation,
};

static int rpi_dsi_display_set_brightness(struct backlight_device *bl)
{
	struct rpi_dsi_display *rpi_dsi_display = bl_get_data(bl);
	struct mipi_dsi_device *dsi = rpi_dsi_display->dsi;
	uint8_t brightness = bl->props.brightness;
	int ret = 0;
	ret = mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_DISPLAY_BRIGHTNESS,
				 &brightness, sizeof(brightness));
	if (ret < 0)
		return ret;
	return 0;
}

static const struct backlight_ops rpi_dsi_display_bl_ops = {
	.update_status = rpi_dsi_display_set_brightness,
};

static const struct drm_display_mode w280bf036i_mode = {
	.clock = 22572,

	.hdisplay = 480,
	.hsync_start = 480 + /* HFP */ 30,
	.hsync_end = 480 + 30 + /* HSync */ 10,
	.htotal = 480 + 30 + 10 + /* HBP */ 30,

	.vdisplay = 640,
	.vsync_start = 640 + /* VFP */ 20,
	.vsync_end = 640 + 20 + /* VSync */ 4,
	.vtotal = 640 + 20 + 4 + /* VBP */ 20,

	.width_mm = 43,
	.height_mm = 57,

	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

/* 3.97寸ST7701S屏幕的显示模式配置 (480x800) */
static const struct drm_display_mode st7701s_397_mode = {
	.clock = 25000, /* 增加时钟频率以支持更高分辨率 */

	.hdisplay = 480,
	.hsync_start = 480 + /* HFP */ 20,
	.hsync_end = 480 + 20 + /* HSync */ 10,
	.htotal = 480 + 20 + 10 + /* HBP */ 30,

	.vdisplay = 800,
	.vsync_start = 800 + /* VFP */ 15,
	.vsync_end = 800 + 15 + /* VSync */ 8,
	.vtotal = 800 + 15 + 8 + /* VBP */ 20,

	.width_mm = 52,
	.height_mm = 86,

	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

/* panel timing copied from panel-sharp-ls043t1le01.c */
static const struct drm_display_mode tdo_qhd0500d5_mode = {
	.clock = 41496,

	.hdisplay = 540,
	.hsync_start = 540 + /* HFP */ 48,
	.hsync_end = 540 + 48 + /* HSync */ 32,
	.htotal = 540 + 48 + 32 + /* HBP */ 80,
 
	.vdisplay = 960,
	.vsync_start = 960 + /* VFP */ 3,
	.vsync_end = 960 + 3 + /* VSync */ 10,
	.vtotal = 960 + 3 + 10 + /* VBP */ 15,

	.width_mm = 65,
	.height_mm = 116,

	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static const struct power_on_timing w280bf036i_pwr_timing = { .post_reset = 50,
							      .reset_low = 20,
							      .after_reset = 120,
							      .slpout = 120 };

/* 为3.97寸ST7701S屏幕创建专门的电源时序配置 */
static const struct power_on_timing st7701s_397_pwr_timing = { .post_reset = 100,
							      .reset_low = 100,
							      .after_reset = 200,
							      .slpout = 200 };

static const struct rpi_dsi_display_desc w280bf036i_desc = {
	.mode = &w280bf036i_mode,
	.lanes = 1,
	.flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
		 MIPI_DSI_MODE_LPM,
	.format = MIPI_DSI_FMT_RGB888,
	.init_sequence = w280bf036i_init_sequence,
	.pwr_timing = &w280bf036i_pwr_timing,
	.do_sw_reset = true
};

/* 3.97寸ST7701S屏幕的描述结构 */
static const struct rpi_dsi_display_desc st7701s_397_desc = {
	.mode = &st7701s_397_mode,
	.lanes = 2,
	.flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
		 MIPI_DSI_MODE_LPM,
	.format = MIPI_DSI_FMT_RGB888,
	.init_sequence = st7701s_397_init_sequence,
	.pwr_timing = &st7701s_397_pwr_timing,
	.do_sw_reset = true
};

static const struct power_on_timing tdo_qhd0500d5_pwr_timing = {
	.post_reset = 50,
	.reset_low = 50,
	.after_reset = 120,
	.slpout = 150
};

static const struct rpi_dsi_display_desc tdo_qhd0500d5_desc = {
	.mode = &tdo_qhd0500d5_mode,
	.lanes = 2,
	.flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
		 MIPI_DSI_MODE_LPM,
	.format = MIPI_DSI_FMT_RGB888,
	.init_sequence = tdo_qhd0500d5_init_sequence,
	.pwr_timing = &tdo_qhd0500d5_pwr_timing,
	.do_sw_reset = true
};

static int rpi_dsi_display_probe(struct mipi_dsi_device *dsi)
{
	struct rpi_dsi_display *rpi_dsi_display;
	const struct rpi_dsi_display_desc *desc;
	int ret;
	
	rpi_dsi_display = devm_kzalloc(&dsi->dev, sizeof(*rpi_dsi_display), GFP_KERNEL);
	if (!rpi_dsi_display)
		return -ENOMEM;

	desc = device_get_match_data(&dsi->dev);
	if (!desc) {
		dev_err(&dsi->dev, "No matching panel description found\n");
		return -ENODEV;
	}
	
	dsi->mode_flags = desc->flags;
	dsi->format = desc->format;
	dsi->lanes = desc->lanes;

	/* 修复：使用正确的成员名 prepare_upstream_first */
	rpi_dsi_display->panel.prepare_upstream_first = true;
	
	rpi_dsi_display->reset =
		devm_gpiod_get_optional(&dsi->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(rpi_dsi_display->reset)) {
		dev_err(&dsi->dev, "Failed to get reset GPIO\n");
		return PTR_ERR(rpi_dsi_display->reset);
	}

	ret = of_drm_get_panel_orientation(dsi->dev.of_node,
					   &rpi_dsi_display->orientation);
	if (ret < 0) {
		dev_warn(&dsi->dev,
			 "Failed to get orientation, default to normal");
		rpi_dsi_display->orientation =
			DRM_MODE_PANEL_ORIENTATION_NORMAL;
	}

	drm_panel_init(&rpi_dsi_display->panel, &dsi->dev,
		       &rpi_dsi_display_funcs, DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&rpi_dsi_display->panel);
	if (ret < 0) {
		dev_info(&dsi->dev,
			 "No backlight configured, using DCS backlight\n");
		struct backlight_device *bl;
		bl = devm_backlight_device_register(
			&dsi->dev, "rpi-dsi-display-bl", &dsi->dev,
			rpi_dsi_display, &rpi_dsi_display_bl_ops, NULL);
		if (IS_ERR(bl)) {
			dev_err(&dsi->dev,
				"Failed to register DCS backlight device\n");
			return PTR_ERR(bl);
		}
		bl->props.max_brightness = 0xFF;
		bl->props.brightness = 0x80;
		bl->props.power = FB_BLANK_POWERDOWN; /* 使用 FB_BLANK_POWERDOWN */
		rpi_dsi_display->panel.backlight = bl;
	}

	drm_panel_add(&rpi_dsi_display->panel);

	mipi_dsi_set_drvdata(dsi, rpi_dsi_display);
	rpi_dsi_display->dsi = dsi;
	rpi_dsi_display->desc = desc;
	
	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&rpi_dsi_display->panel);
		return ret;
	}
	
	return 0;
}

static void rpi_dsi_display_remove(struct mipi_dsi_device *dsi)
{
	struct rpi_dsi_display *rpi_dsi_display = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&rpi_dsi_display->panel);
}

static const struct of_device_id rpi_dsi_display_ids[] = {
	{ .compatible = "wlk,w280bf036i", .data = &w280bf036i_desc },
	{ .compatible = "sitronix,st7701s", .data = &st7701s_397_desc }, /* 3.97寸ST7701S屏幕 */
	{ .compatible = "truly,tdo-qhd0500d5", .data = &tdo_qhd0500d5_desc },
	{}
};

MODULE_DEVICE_TABLE(of, rpi_dsi_display_ids);

static struct mipi_dsi_driver rpi_dsi_display = {
    .probe = rpi_dsi_display_probe,
    .remove = rpi_dsi_display_remove,
    .driver =
        {
            .name = "rpi_dsi_display_driver",
            .of_match_table = rpi_dsi_display_ids,
        },
};

module_mipi_dsi_driver(rpi_dsi_display);
MODULE_AUTHOR("cnflysky@qq.com");
MODULE_DESCRIPTION("RPI DSI Display driver");
MODULE_LICENSE("GPL");