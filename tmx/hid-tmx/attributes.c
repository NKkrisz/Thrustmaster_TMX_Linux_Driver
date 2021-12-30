static inline int tmx_init_attributes(struct tmx *tmx)
{
	int errno;

	// Before making avaible the syfs attrbiutes we try to set them to some default
	// @FIXME I do not know if it is desiradable to wait for URBs in probe() method...
	tmx_setup_task(tmx);

	errno = device_create_file(&tmx->usb_device->dev, &dev_attr_autocenter);
	if(errno)
		return errno;

	errno = device_create_file(&tmx->usb_device->dev, &dev_attr_enable_autocenter);
	if(errno)
		goto err1;

	errno = device_create_file(&tmx->usb_device->dev, &dev_attr_range);
	if(errno)
		goto err2;

	errno = device_create_file(&tmx->usb_device->dev, &dev_attr_gain);
	if(errno)
		goto err3;

	errno = device_create_file(&tmx->usb_device->dev, &dev_attr_firmware_version);
	if(errno)
		goto err4;

	return 0;

err4:	device_remove_file(&tmx->usb_device->dev, &dev_attr_firmware_version);
err3:	device_remove_file(&tmx->usb_device->dev, &dev_attr_range);
err2:	device_remove_file(&tmx->usb_device->dev, &dev_attr_enable_autocenter);
err1:	device_remove_file(&tmx->usb_device->dev, &dev_attr_autocenter);
	return errno;
}

static inline void tmx_free_attributes(struct tmx *tmx)
{
	device_remove_file(&tmx->usb_device->dev, &dev_attr_autocenter);
	device_remove_file(&tmx->usb_device->dev, &dev_attr_enable_autocenter);
	device_remove_file(&tmx->usb_device->dev, &dev_attr_range);
	device_remove_file(&tmx->usb_device->dev, &dev_attr_gain);
	device_remove_file(&tmx->usb_device->dev, &dev_attr_firmware_version);
}

/**/
static ssize_t tmx_store_return_force(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	uint8_t nforce;
	struct tmx *tmx = dev_get_drvdata(dev);

	// If mallformed input leave...
	if(kstrtou8(buf, 10, &nforce))
		return count;

	if(nforce > 100)
		nforce = 100;

	tmx_set_autocenter(tmx, nforce);

	return count;
}

static ssize_t tmx_show_return_force(struct device *dev, struct device_attribute *attr,char * buf )
{
	int len;
	struct tmx *tmx = dev_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&tmx->settings.access_lock, flags);
	len = sprintf(buf, "%d\n", tmx->settings.autocenter_force);
	spin_unlock_irqrestore(&tmx->settings.access_lock, flags);

	return len;
}

static ssize_t tmx_store_simulate_return_force(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	bool use;
	struct tmx *tmx = dev_get_drvdata(dev);

	// If mallformed input leave...
	if(!kstrtobool(buf, &use))
		tmx_set_enable_autocenter(tmx, use);	

	return count;
}

static ssize_t tmx_show_simulate_return_force(struct device *dev, struct device_attribute *attr,char * buf)
{
	int len;
	struct tmx *tmx = dev_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&tmx->settings.access_lock, flags);
	len = sprintf(buf, "%c\n", tmx->settings.autocenter_enabled ? 'y' : 'n');
	spin_unlock_irqrestore(&tmx->settings.access_lock, flags);

	return len;
}

static ssize_t tmx_store_range(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	uint16_t range;
	struct tmx *tmx = dev_get_drvdata(dev);

	// If mallformed input leave...
	if(kstrtou16(buf, 10, &range))
		return count;

	if(range < 270)
		range = 270;
	else if (range > 900)
		range = 900;

	range = DIV_ROUND_CLOSEST((range * 0xffff), 900);

	tmx_set_range(tmx, range);

	return count;
}

static ssize_t tmx_show_range(struct device *dev, struct device_attribute *attr,char * buf )
{
	int len;
	struct tmx *tmx = dev_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&tmx->settings.access_lock, flags);
	len = sprintf(buf, "%d\n", DIV_ROUND_CLOSEST(tmx->settings.range * 900, 0xffff));
	spin_unlock_irqrestore(&tmx->settings.access_lock, flags);

	return len;
}

static ssize_t tmx_store_ffb_intensity(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	uint8_t nforce;
	struct tmx *tmx = dev_get_drvdata(dev);

	// If mallformed input leave...
	if(kstrtou8(buf, 10, &nforce))
		return count;

	if(nforce > 100)
		nforce = 100;

	nforce = DIV_ROUND_CLOSEST(nforce * 0x80, 100);

	tmx_set_gain(tmx, nforce);
	return count;
}

static ssize_t tmx_show_ffb_intensity(struct device *dev, struct device_attribute *attr,char * buf )
{
	int len;
	struct tmx *tmx = dev_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&tmx->settings.access_lock, flags);
	len = sprintf(buf, "%d\n", DIV_ROUND_CLOSEST(tmx->settings.gain * 100, 0x80));
	spin_unlock_irqrestore(&tmx->settings.access_lock, flags);

	return len;
}

ssize_t tmx_show_fw_version(struct device *dev, struct device_attribute *attr,char * buf )
{
	int len;
	struct tmx *tmx = dev_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&tmx->settings.access_lock, flags);
	len = sprintf(buf, "%d\n", tmx->settings.firmware_version);
	spin_unlock_irqrestore(&tmx->settings.access_lock, flags);

	return len;
}
