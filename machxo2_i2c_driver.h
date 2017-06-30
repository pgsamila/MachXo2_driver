
#include <linux/kernel.h>
/*
 * This driver handles the LM75 and compatible digital temperature sensors.
 */

enum lm75_type {		/* keep sorted in alphabetical order */
	adt75,
	ds1775,
	ds75,
	ds7505,
	g751,
	lm75,
	lm75a,
	max6625,
	max6626,
	mcp980x,
	stds75,
	tcn75,
	tmp100,
	tmp101,
	tmp105,
	tmp175,
	tmp275,
	tmp75,
};

/* The I2C driver structure */ 
static const struct i2c_device_id machxo2_idtable[] = {
	{ "adt75", adt75, },
	{ "ds1775", ds1775, },
	{ "ds75", ds75, },
	{ "ds7505", ds7505, },
	{ "g751", g751, },
	{ "lm75", lm75, },
	{ "lm75a", lm75a, },
	{ "max6625", max6625, },
	{ "max6626", max6626, },
	{ "mcp980x", mcp980x, },
	{ "stds75", stds75, },
	{ "tcn75", tcn75, },
	{ "tmp100", tmp100, },
	{ "tmp101", tmp101, },
	{ "tmp105", tmp105, },
	{ "tmp175", tmp175, },
	{ "tmp275", tmp275, },
	{ "tmp75", tmp75, },
	{ /* LIST END */ }
};

