#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/completion.h>
#include <linux/input.h>
#include <linux/usb/input.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/fixp-arith.h>
#include <linux/spinlock.h>
#include <linux/hid.h>

#include "hid-tmx.h"
#include "input.h"
#include "attributes.h"
#include "settings.h"
#include "forcefeedback.h"
#include "packet.h"

static void donothing_callback(struct urb *urb) {}

/** Init for a tmx data struct
 * @param tmx pointer to the tmx structor to init
 * @param interface pointer to usb interface which the wheel is connected to
 */
static inline int tmx_constructor(struct tmx *tmx,struct hid_device *hid_device)
{
	int i, error_code = 0;
	struct usb_endpoint_descriptor *ep, *ep_irq_in = 0, *ep_irq_out = 0;
	struct usb_interface *interface = to_usb_interface(hid_device->dev.parent);

	tmx->usb_device = interface_to_usbdev(interface);
	tmx->hid_device = hid_device;

	// Saving ref to tmx
	dev_set_drvdata(&tmx->usb_device->dev, tmx);
	hid_set_drvdata(hid_device, tmx);

	error_code = hid_parse(hid_device);
	if (error_code) {
		hid_err(hid_device, "hid_parse() failed\n");
		return error_code;
	}

	error_code = hid_hw_start(hid_device, HID_CONNECT_DEFAULT & ~HID_CONNECT_FF);
	if (error_code) {
		hid_err(hid_device, "hid_hw_start() failed\n");
		return error_code;
	}

	mutex_init(&tmx->lock);
	spin_lock_init(&tmx->settings.access_lock);

	// Path used for the input subsystem
	usb_make_path(tmx->usb_device, tmx->dev_path, sizeof(tmx->dev_path));
	strlcat(tmx->dev_path, "/input0", sizeof(tmx->dev_path));

	// From xpad.c
	for (i = 0; i < 2; i++) {
		ep = &interface->cur_altsetting->endpoint[i].desc;

		if (usb_endpoint_xfer_int(ep))
			if (usb_endpoint_dir_in(ep))
				ep_irq_in = ep;
			else
				ep_irq_out = ep;
	}

	if (!ep_irq_in || !ep_irq_out) {
		error_code = -ENODEV;
		goto error3;
	}

	tmx->pipe_in = usb_rcvintpipe(tmx->usb_device, ep_irq_in->bEndpointAddress);
	tmx->pipe_out= usb_sndintpipe(tmx->usb_device, ep_irq_out->bEndpointAddress);

	tmx->bInterval_in = ep_irq_in->bInterval;
	tmx->bInterval_out = ep_irq_out->bInterval;

	error_code = tmx_init_input(tmx);
	if(error_code)
		goto error4;

	error_code = tmx_init_ffb(tmx);
	if(error_code)
		goto error5;
	
	error_code = tmx_init_attributes(tmx);
	if(error_code)
		goto error6;

	return 0;

error6: tmx_free_ffb(tmx);
error5: tmx_free_input(tmx);
error4:	;
error3: hid_hw_stop(hid_device);
	return error_code;
}

static int tmx_probe(struct hid_device *hid_device, const struct hid_device_id *id)
{
	int error_code = 0;
	struct tmx *tmx;

	// Create new tmx struct
	tmx = kzalloc(sizeof(struct tmx), GFP_KERNEL);
	if(!tmx)
		return -ENOMEM;

	error_code = tmx_constructor(tmx, hid_device);
	if(error_code)
		goto error0;

	return 0;

error0: kfree(tmx);
	return error_code;
}

static void tmx_remove(struct hid_device *hid_device)
{
	struct tmx *tmx = hid_get_drvdata(hid_device);;

	hid_info(tmx->hid_device, "TMX Wheel removed. Bye\n");

	// Force feedback 
	tmx_free_ffb(tmx);

	// input deregister
	tmx_free_input(tmx);

	// sysf free
	tmx_free_attributes(tmx);

	// Stop hid
	hid_hw_close(hid_device);
	hid_hw_stop(hid_device);

	// tmx free
	kfree(tmx);
}

#include "attributes.c"
#include "input.c"
#include "settings.c"
#include "forcefeedback.c"


/********************************************************************
 *			MODULE STUFF
 *
 *******************************************************************/

static struct hid_device_id tmx_table[] =
{
	{ HID_USB_DEVICE(USB_THRUSTMASTER_VENDOR_ID, USB_TMX_PRODUCT_ID) },
	{} /* Terminating entry */
};
MODULE_DEVICE_TABLE (hid, tmx_table);

static struct hid_driver tmx_driver =
{
	.name = "hid-tmx",
	.id_table = tmx_table,
	.probe = tmx_probe,
	.remove = tmx_remove,
	.raw_event = tmx_update_input
};

static int __init tmx_init(void)
{
	int errno = -ENOMEM;
	packet_input_open = kzalloc(sizeof(uint16_t), GFP_KERNEL);
	if(!packet_input_open)
		goto err0;

	packet_input_what = kzalloc(sizeof(uint16_t), GFP_KERNEL);
	if(!packet_input_what)
		goto err1;

	packet_input_close = kzalloc(sizeof(uint16_t), GFP_KERNEL);
	if(!packet_input_close)
		goto err2;

	*packet_input_open = cpu_to_le16(0x0442);
	*packet_input_what = cpu_to_le16(0x0542);
	*packet_input_close = cpu_to_le16(0x0042);

	errno = hid_register_driver(&tmx_driver);
	if(errno)
		goto err3;
	else
		return 0;

err3:	kfree(packet_input_close);
err2:	kfree(packet_input_what);
err1:	kfree(packet_input_open);
err0:	return errno;
}

static void __exit tmx_exit(void)
{
	kfree(packet_input_open);
	kfree(packet_input_what);
	kfree(packet_input_close);

	hid_unregister_driver(&tmx_driver);
}

module_init(tmx_init);
module_exit(tmx_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dario Pagani, Cat2048");
MODULE_DESCRIPTION("ThrustMaster TMX steering wheel driver");
