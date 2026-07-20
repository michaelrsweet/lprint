#include "lprint-brother.h"

static lprint_brother_driver_data_t lprint_brother_PT_E550W_P750W_P710BT = {
	.dpi = 180,
	.head_width = 128,
	.min_feed = 200,
	.min_feed_noprecut = 2430,
	.max_feed = 12700,
	// For double DPI, this is really 420, but nobody will mind
	.min_len = 440,
	.max_len = 100000,
	.flags = BROTHER_FLAG_IS_PT | BROTHER_FLAG_DOUBLE_DPI,
};

static lprint_brother_driver_data_t lprint_brother_PT_P900_P900W_P950NW_P910BT = {

	.dpi = 360,
	.head_width = 560,
	.min_feed = 100,
	.min_feed_noprecut = 2700,
	.max_feed = 12700,
	.min_len = 400,
	.max_len = 100000,
	.flags = BROTHER_FLAG_IS_PT | BROTHER_FLAG_DOUBLE_DPI,
};

static lprint_brother_driver_data_t lprint_brother_QL_500_550_560_570_580N_650TD_700 = {

	.dpi = 300,
	.head_width = 720,
	.min_feed = 300,
	.min_feed_noprecut = 300, // Not specified for QL printers, use normal min_feed
	.max_feed = 12700,
	.min_len = 1270,
	.max_len = 300000,
	.flags = BROTHER_FLAG_DOUBLE_DPI,
};

static lprint_brother_driver_data_t lprint_brother_QL_810W_820NWB = {

	.dpi = 300,
	.head_width = 720,
	.min_feed = 300,
	.min_feed_noprecut = 300, // Not specified for QL printers, use normal min_feed
	.max_feed = 12700,
	.min_len = 1270,
	.max_len = 100000,
	.flags = BROTHER_FLAG_DOUBLE_DPI,
};

static lprint_brother_driver_data_t lprint_brother_QL_800 = {

	.dpi = 300,
	.head_width = 720,
	.min_feed = 300,
	.min_feed_noprecut = 300, // Not specified for QL printers, use normal min_feed
	.max_feed = 12700,
	.min_len = 1270,
	.max_len = 100000,
	.flags = BROTHER_FLAG_DOUBLE_DPI | BROTHER_FLAG_RASTER_NO_Z_CMD,
};
