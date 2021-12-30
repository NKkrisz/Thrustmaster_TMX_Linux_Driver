/**
 * @param tmx ptr to tmx
 * @param gain a value between 0x00 and 0x80 where 0x80 is 100% gain
 * @return 0 on success @see usb_interrupt_msg for return codes
 */
static int tmx_set_gain(struct tmx *tmx, uint8_t gain)
{
	int boh, errno;
	uint8_t *buffer = kzalloc(2, GFP_KERNEL);
	unsigned long flags;

	buffer[0] = 0x43;
	buffer[1] = gain;

	mutex_lock(&tmx->lock);

	// Send to the wheel desidered return force
	errno = usb_interrupt_msg(
		tmx->usb_device,
		tmx->pipe_out,
		buffer,
		2, &boh,
		SETTINGS_TIMEOUT
	);

	if(!errno) {
		spin_lock_irqsave(&tmx->settings.access_lock, flags);
		tmx->settings.gain = gain;
		spin_unlock_irqrestore(&tmx->settings.access_lock, flags);
	} else {
		hid_err(tmx->hid_device, "Operation set gain failed with code %d", errno);
	}

	mutex_unlock(&tmx->lock);

	kfree(buffer);

	return errno;
}

/**
 * @param autocenter_force a value between 0 and 100, is the strength of the autocenter effect
 */
static __always_inline int tmx_set_autocenter(struct tmx *tmx, uint8_t autocenter_force)
{
	uint8_t *buffer = kzalloc(4, GFP_KERNEL);
	int errno;
	unsigned long flags;

	mutex_lock(&tmx->lock);

	errno = tmx_settings_set40(tmx, SET40_RETURN_FORCE, autocenter_force, buffer);

	if(!errno) {
		spin_lock_irqsave(&tmx->settings.access_lock, flags);
		tmx->settings.autocenter_force = autocenter_force;
		spin_unlock_irqrestore(&tmx->settings.access_lock, flags);
	}

	mutex_unlock(&tmx->lock);

	kfree(buffer);
	return errno;
}

/**
 * @param enable true if the autocenter effect is to be kept enabled when the input
 * 	is opened. The autocentering effect is always active while no input are open
 */
static __always_inline int tmx_set_enable_autocenter(struct tmx *tmx, bool enable)
{
	uint8_t *buffer = kzalloc(4, GFP_KERNEL);
	int errno;
	unsigned long flags;

	mutex_lock(&tmx->lock);

	errno = tmx_settings_set40(tmx, SET40_USE_RETURN_FORCE, enable, buffer);

	if(!errno) {
		spin_lock_irqsave(&tmx->settings.access_lock, flags);
		tmx->settings.autocenter_enabled = enable;
		spin_unlock_irqrestore(&tmx->settings.access_lock, flags);
	}

	mutex_unlock(&tmx->lock);

	kfree(buffer);
	return errno;
}

/**
 * @param range a value between 0x0000 and 0xffff where 0xffff is 900°
 * 	wheel range
 */
static __always_inline int tmx_set_range(struct tmx *tmx, uint16_t range)
{
	uint8_t *buffer = kzalloc(4, GFP_KERNEL);
	int errno;
	unsigned long flags;

	mutex_lock(&tmx->lock);

	errno = tmx_settings_set40(tmx, SET40_RANGE, range, buffer);

	if(!errno) {
		spin_lock_irqsave(&tmx->settings.access_lock, flags);
		tmx->settings.range = range;
		spin_unlock_irqrestore(&tmx->settings.access_lock, flags);
	}

	mutex_unlock(&tmx->lock);

	kfree(buffer);
	return errno;
}


/**
 * @tmx pointer to tmx
 * @operation number of operation
 * @argument the argument to pass with the request
 * @buffer a buffer of at least 4 BYTES!
 * @return 0 on success @see usb_interrupt_msg for return codes
 */
static int tmx_settings_set40(
	struct tmx *tmx, operation_t operation, uint16_t argument, void *buffer_
)
{
	int boh, errno;
	struct operation40 *buffer = buffer_;
	buffer->code = 0x40;
	buffer->operation = operation;
	buffer->argument = cpu_to_le16(argument);

	// Send to the wheel desidered return force
	errno = usb_interrupt_msg(
		tmx->usb_device,
		tmx->pipe_out,
		buffer,
		sizeof(struct operation40), &boh,
		SETTINGS_TIMEOUT
	);

	if(errno)
		hid_err(tmx->hid_device, "errno %d during operation 0x40 0x%02hhX with argument (big endian) %04hhX",
			errno, operation, argument);

	return errno;
}

static int tmx_setup_task(struct tmx *tmx)
{
	int errno = 0;
	uint8_t *fw_version;

	fw_version = kzalloc(8, GFP_KERNEL);

	// Retrive current version
	mutex_lock(&tmx->lock);
	
	errno = usb_control_msg(
		tmx->usb_device,
		usb_rcvctrlpipe(tmx->usb_device, 0),
		86, 0xc1, 0, 0, fw_version, 8, SETTINGS_TIMEOUT
	);

	mutex_unlock(&tmx->lock);

	if(errno < 0)
		hid_err(tmx->hid_device, "Error %d while sending the control URB to retrive firmware version\n", errno);
	else
		tmx->settings.firmware_version = fw_version[1];

	errno = tmx_set_gain(tmx, 0x66); // ~80%
	if(errno)
		hid_err(tmx->hid_device, "Error %d while setting the tmx default gain\n", errno);

	errno = tmx_set_enable_autocenter(tmx, false);
	if(errno)
		hid_err(tmx->hid_device, "Error %d while setting the tmx default enable_autocenter\n", errno);

	errno = tmx_set_autocenter(tmx, 50);
	if(errno)
		hid_err(tmx->hid_device, "Error %d while setting the tmx default autocenter\n", errno);

	errno = tmx_set_range(tmx, 0xffff);
	if(errno)
		hid_err(tmx->hid_device, "Error %d while setting the tmx default range\n", errno);

	hid_info(tmx->hid_device,  "Setup completed! Firmware version is %d\n", tmx->settings.firmware_version);

	kfree(fw_version);
	return errno;
}
