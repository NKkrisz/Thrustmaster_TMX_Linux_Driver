/**
 * This function initializes the input system for
 * @tmx pointer to our device
 */
static inline int tmx_init_input(struct tmx *tmx)
{
	struct hid_input *hidinput = list_entry(tmx->hid_device->inputs.next, struct hid_input, list);
	tmx->joystick = hidinput->input;
	
	input_set_drvdata(tmx->joystick, tmx);

	tmx->joystick->open = tmx_input_open;
	tmx->joystick->close = tmx_input_close;

	return 0;
}

static inline void tmx_free_input(struct tmx *tmx)
{
}

static int tmx_input_open(struct input_dev *dev)
{
	struct tmx *tmx = input_get_drvdata(dev);
	int boh, ret;

	ret = usb_interrupt_msg(
		tmx->usb_device,
		tmx->pipe_out,
		packet_input_open, 2, &boh,
		8
	);

	if(ret)
		return ret;

	ret = hid_hw_open(tmx->hid_device);

	return ret;
}

static void tmx_input_close(struct input_dev *dev)
{
	struct tmx *tmx = input_get_drvdata(dev);
	int boh, i;

	hid_hw_close(tmx->hid_device);

	// Send magic codes
	for(i = 0; i < 2; i++)
		usb_interrupt_msg(
			tmx->usb_device,
			tmx->pipe_out,
			packet_input_what, 2, &boh,
			8
		);

	usb_interrupt_msg(
		tmx->usb_device,
		tmx->pipe_out,
		packet_input_close, 2, &boh,
		8
	);
}

/**
 * This function updates the current input status of the joystick
 * @tmx target wheel
 * @ss   new status to register
 */
static int tmx_update_input(struct hid_device *hdev, struct hid_report *report, uint8_t *packet_raw, int size)
{	
	struct tmx_state_packet *packet = (struct tmx_state_packet*)packet_raw;

	if(packet->type != STATE_PACKET_INPUT)
	{
		hid_warn(hdev, "recived a packet that is not an input state :/\n");
		printP(packet_raw, size);
		return -1; // @TODO 
	}

	return 0;
}
