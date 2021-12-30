/********************************************************************
 *			             SYSF STUFF
 *
 *        This section contains the handlers for the
 *          attributes created in the sysfs when a
 *             wheel is plugged into the host
 *******************************************************************/
/***/

static inline int tmx_init_attributes(struct tmx *tmx);
static inline void tmx_free_attributes(struct tmx *tmx);

static ssize_t tmx_store_return_force(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count);
static ssize_t tmx_show_return_force(struct device *dev, struct device_attribute *attr,char * buf );
static ssize_t tmx_store_simulate_return_force(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count);
static ssize_t tmx_show_simulate_return_force(struct device *dev, struct device_attribute *attr,char * buf );
static ssize_t tmx_store_range(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count);
static ssize_t tmx_show_range(struct device *dev, struct device_attribute *attr,char * buf );
static ssize_t tmx_store_ffb_intensity(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count);
static ssize_t tmx_show_ffb_intensity(struct device *dev, struct device_attribute *attr,char * buf );
static ssize_t tmx_show_fw_version(struct device *dev, struct device_attribute *attr,char * buf );


/** Attribute used to set how much strong is the simulated "spring" that makes
 * the wheel center back when steered.
 * Input is a decimal value between 0 and 100*/
static DEVICE_ATTR(autocenter, 0664, tmx_show_return_force, tmx_store_return_force);

/** Attribute used to set if the wheel has to activate the auto-centering of the
 *  wheel. If setted to true ffb to center will be ignored
 * Input is a boolean value*/
static DEVICE_ATTR(enable_autocenter, 0664, tmx_show_simulate_return_force, tmx_store_simulate_return_force);

/** Attribute used to set the wheel max rotation in degrees.
 * Input is a decimal value between between 270 and 900*/
static DEVICE_ATTR(range, 0664, tmx_show_range, tmx_store_range);

/** 
 * How strong the ffb effects are reproduced on the wheel
 * Input is a decimal value between between 0 and 100*/
static DEVICE_ATTR(gain, 0664, tmx_show_ffb_intensity, tmx_store_ffb_intensity);

/**
 * Read-only, returns the current firmware version of the Wheel*/
static DEVICE_ATTR(firmware_version, 0444, tmx_show_fw_version, 0);