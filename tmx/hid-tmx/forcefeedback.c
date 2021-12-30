/** Callback to free urb when a ffb request is completed */
static void tmx_ff_free_urb(struct urb *urb) 
{
	//struct tmx *tmx = urb->context;
	kfree(urb->transfer_buffer);
	usb_free_urb(urb);
}

/**
 * Creates an usb URB to be sent to wheel for ffb operations
 * @param tmx the wheel
 * @param buffer_size how large alloc the urb
 * @returns a ptr to URB if no error, 0 otherwise
 */
static struct urb* tmx_ff_alloc_urb(struct tmx *tmx, const size_t buffer_size)
{
	struct urb *urb;

	void *buffer = kzalloc(buffer_size, GFP_KERNEL);
	if(!buffer)
		return 0;

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if(!urb) {
		kfree(buffer);
		return 0;
	}

	usb_fill_int_urb(
		urb,
		tmx->usb_device,
		tmx->pipe_out,
		buffer,
		buffer_size,
		donothing_callback,
		tmx,
		tmx->bInterval_out
	); 

	return urb;
}

/** 
 * This macro is called in the probe function when the wheel input
 * is beign setted up
 * @param tmx a pointer to the wheel
 * @returns 0 if no error, less than 0 if an error occured. 
 */
static inline int tmx_init_ffb(struct tmx *tmx)
{
	int errno, i;

	for (i = 0; i < tmx_ffb_effects_length; i++)
		set_bit(tmx_ffb_effects[i], tmx->joystick->ffbit);

	// input core will automatically free force feedback structures when device is destroyed.
	errno = input_ff_create(tmx->joystick, FF_MAX_EFFECTS);
	
	if(errno) {
		hid_err(tmx->hid_device, "error create ff :(. errno=%i\n", errno);
		return errno;
	}

	tmx->joystick->ff->upload = tmx_ff_upload;
	tmx->joystick->ff->erase = tmx_ff_erase;
	tmx->joystick->ff->playback = tmx_ff_play;
	tmx->joystick->ff->set_gain = tmx_ff_set_gain;

	return 0;
}

/**
 * macro to clean up the ffb stuff of a whell. It's to be called
 * when probe failed to init or when the wheel is disconnected
 * @param tmx a pointer to the wheel
 */
static inline void tmx_free_ffb(struct tmx *tmx)
{
	unsigned int i, j;

	for(i = 0; i < FF_MAX_EFFECTS; i++)
		for(j = 0; j < 3; j++) {
			if(! tmx->update_ffb_urbs[i][j])
				continue;

			usb_kill_urb(tmx->update_ffb_urbs[i][j]);
			kfree(tmx->update_ffb_urbs[i][j]->transfer_buffer);
			usb_free_urb(tmx->update_ffb_urbs[i][j]);
		}
}

static void tmx_ff_preapre_first(struct ff_first *ff_first, struct ff_effect *effect)
{
	struct ff_envelope *ff_envelope;

	switch (effect->type) {
	case FF_CONSTANT:
		ff_envelope = &effect->u.constant.envelope;
		ff_first->f0 = TMX_FF_FIRST_CODE_CONSTANT;
		break;
	case FF_PERIODIC:
		ff_envelope = &effect->u.periodic.envelope;
		ff_first->f0 = TMX_FF_FIRST_CODE_PERIODIC;
		break;
	case FF_DAMPER:
	case FF_SPRING:
		ff_envelope = 0;
		ff_first->f0 = TMX_FF_FIRST_CODE_CONDITION;
		break;
	default:
		ff_envelope = 0;
		break;
	}

	ff_first->pk_id0 = effect->id * 0x1c + 0x1c;
	ff_first->f1 = 0;
	ff_first->f2 = 0x46;
	ff_first->f3 = 0x54;

	/* Some effects do not use those fields */
	if(ff_envelope) {
		ff_first->attack_length = cpu_to_le16(ff_envelope->attack_length);
		// @FIXME the attack and fade levels are wrong !
		ff_first->attack_level  = ff_envelope->attack_level / 0x1fff;
		ff_first->fade_length = cpu_to_le16(ff_envelope->attack_length);
		ff_first->fade_level  = ff_envelope->fade_level / 0x1fff;
	}
}

/**
 * This function prepares an update packet to update an already uploaded effected
 * or when we're uploading a new effect
 * @param ff_update the usb packed data to prepare
 * @param effect the effect to be updated
 */
static void tmx_ff_prepare_update(struct ff_update *ff_update, struct ff_effect *effect)
{
	int32_t level = 0;

	ff_update->pk_id1 = effect->id * 0x1c + 0x0e;
	ff_update->f1 = 0x00;

	switch (effect->type) {
	case FF_PERIODIC:
	default:
		ff_update->effect_class = TMX_FF_UPDATE_CODE_PERIODIC;

		ff_update->effect.periodic.magnitude = word_high(effect->u.periodic.magnitude);
		ff_update->effect.periodic.offset = word_high(effect->u.periodic.offset);
		ff_update->effect.periodic.phase = effect->u.periodic.phase / ( (360*100) / 0xff) ; // Check if correct
		ff_update->effect.periodic.period = cpu_to_le16(effect->u.periodic.period);
		break;
	case FF_CONSTANT:
		ff_update->effect_class = TMX_FF_UPDATE_CODE_CONSTANT;

		/* Not sure if really necessary. Done only for the ffmvforce utility :P */
		level = effect->u.constant.level * fixp_sin16(effect->direction / ( 0xFFFF / 360 )) * +1;
		level >>= 15; // int only

		ff_update->effect.constant.level = (level / 0x01ff);
		break;
	case FF_SPRING:
		ff_update->effect_class = TMX_FF_UPDATE_CODE_CONDITION;

		ff_update->effect.condition.right_coeff = effect->u.condition[0].right_coeff / 0x147;
		ff_update->effect.condition.left_coeff = effect->u.condition[0].left_coeff / 0x147;

		ff_update->effect.condition.center = cpu_to_le16(
			effect->u.condition[0].center / (0x7fff / 0x01f4) 
		);
		ff_update->effect.condition.deadband = cpu_to_le16(
			effect->u.condition[0].deadband / (0xffff /0x03e8)
		);

		ff_update->effect.condition.right_sat = effect->u.condition[0].right_saturation / 0x030c;
		ff_update->effect.condition.left_sat = effect->u.condition[0].left_saturation / 0x030c;
		break;
	case FF_DAMPER:
		ff_update->effect_class = TMX_FF_UPDATE_CODE_CONDITION;

		ff_update->effect.condition.right_coeff = effect->u.condition[0].right_coeff / 0x147;
		ff_update->effect.condition.left_coeff = effect->u.condition[0].left_coeff / 0x147;

		ff_update->effect.condition.center = cpu_to_le16(
			effect->u.condition[0].center / (0x7fff / 0x01f4) 
		);
		ff_update->effect.condition.deadband = cpu_to_le16(
			effect->u.condition[0].deadband / (0xffff /0x03e8)
		);

		ff_update->effect.condition.right_sat = effect->u.condition[0].right_saturation / 0x028f;
		ff_update->effect.condition.left_sat = effect->u.condition[0].left_saturation / 0x028f;

		break;
	}
}

static void tmx_ff_prepare_commit(struct ff_commit *ff_commit, struct ff_effect *effect)
{
	ff_commit->f0 = 0x01;
	ff_commit->id = effect->id;
	if(effect->replay.length) // Ugly hack(?) per Assetto Corsa :P
		ff_commit->length = cpu_to_le16(effect->replay.length);
	else
		ff_commit->length = cpu_to_le16(0xffff);
	ff_commit->f1 = 0;
	ff_commit->f2 = 0;
	ff_commit->pk_id1 = effect->id * 0x1c + 0x0e;
	ff_commit->f3 = 0;
	ff_commit->pk_id0 = effect->id * 0x1c + 0x1c;
	ff_commit->f4 = 0;
	ff_commit->delay = word_high(effect->replay.delay);
	ff_commit->f5 = 0;

	switch (effect->type) {
	case FF_PERIODIC:
		switch (effect->u.periodic.waveform) {
		case FF_SINE:
		default:
			ff_commit->effect_type = cpu_to_le16(TMX_FF_COMMIT_CODE_SINE);
			break;
		case FF_SAW_UP:
			ff_commit->effect_type = cpu_to_le16(TMX_FF_COMMIT_CODE_SAW_UP);
			break;
		case FF_SAW_DOWN:
			ff_commit->effect_type = cpu_to_le16(TMX_FF_COMMIT_CODE_SAW_DOWN);
			break;
		}
		break;
	case FF_CONSTANT:
		ff_commit->effect_type = cpu_to_le16(TMX_FF_COMMIT_CODE_CONSTANT);
		break;
	case FF_SPRING:
		ff_commit->effect_type = cpu_to_le16(TMX_FF_COMMIT_CODE_SPRING);
		break;
	case FF_DAMPER:
		ff_commit->effect_type = cpu_to_le16(TMX_FF_COMMIT_CODE_DAMPER);
		break;
	default:
		printk(KERN_ERR "TMX: unknown effect type: %i\n", effect->type);
	}
}

/**
 * Function called to upload an effect to the wheel.
 * An effect has to be sent to the wheel fragmented in 3 usb request.
 * @param dev the input_dev
 * @param effect the effect to upload
 * @param old If I have to update an already uploaded effect this is not 0
 * 
 * @return 0 if no errors occured
 */
static int tmx_ff_upload(struct input_dev *dev, struct ff_effect *effect, struct ff_effect *old)
{
	struct tmx *tmx = input_get_drvdata(dev);
	int errno = 0;

	struct ff_first ff_first_old, ff_first_new;
	struct ff_update ff_update_old, ff_update_new;
	struct ff_commit ff_commit_old, ff_commit_new;

	// No need to re-upload the same effect....
	if(!TMX_FF_BLIND_COMPUTE_EFFECT && old && memcmp(effect, old, sizeof(struct ff_effect)) == 0)
		return 0;

	// If URBs were already allocated we can re-use them....
	// Alloc first urb
	if(! tmx->update_ffb_urbs[effect->id][0])
		tmx->update_ffb_urbs[effect->id][0] = tmx_ff_alloc_urb(tmx, sizeof(struct ff_first));

	if(! tmx->update_ffb_urbs[effect->id][0])
		return -ENOMEM;

	// Alloc second urb
	if(! tmx->update_ffb_urbs[effect->id][1])
		tmx->update_ffb_urbs[effect->id][1] = tmx_ff_alloc_urb(tmx, sizeof(struct ff_update));

	if(! tmx->update_ffb_urbs[effect->id][1])
		goto free0;

	// Alloc third urb
	if(! tmx->update_ffb_urbs[effect->id][2])
		tmx->update_ffb_urbs[effect->id][2] = tmx_ff_alloc_urb(tmx, sizeof(struct ff_commit));

	if(! tmx->update_ffb_urbs[effect->id][2])
		goto free1;	

	/** Preparing effect */
	tmx_ff_preapre_first(&ff_first_new, effect);
	tmx_ff_prepare_update(&ff_update_new, effect);
	tmx_ff_prepare_commit(&ff_commit_new, effect);

	if(old) {
		tmx_ff_preapre_first(&ff_first_old, old);
		tmx_ff_prepare_update(&ff_update_old, old);
		tmx_ff_prepare_commit(&ff_commit_old, old);
	}
	
	/** Submiting the effect to the wheel 
	 * If an old is present and the result packet are the same we skip an URB
	 * unless you define TMX_FF_BLIND_UPLOAD as true
	 */
	if(TMX_FF_BLIND_UPLOAD || !old || memcmp(&ff_first_old, &ff_first_new, sizeof(struct ff_first))){
		usb_kill_urb(tmx->update_ffb_urbs[effect->id][0]);

		memcpy(tmx->update_ffb_urbs[effect->id][0]->transfer_buffer, &ff_first_new, sizeof(struct ff_first));
		errno = usb_submit_urb(tmx->update_ffb_urbs[effect->id][0], GFP_ATOMIC);
		if(errno) {
			hid_err(tmx->hid_device, "submitting ffb 0 urb of effect %d, error %d\n", effect->id ,errno);
			return errno;
		}
	}

	if(TMX_FF_BLIND_UPLOAD || !old || memcmp(&ff_update_old, &ff_update_new, sizeof(struct ff_update))){
		usb_kill_urb(tmx->update_ffb_urbs[effect->id][1]);

		memcpy(tmx->update_ffb_urbs[effect->id][1]->transfer_buffer, &ff_update_new, sizeof(struct ff_update));
		errno = usb_submit_urb(tmx->update_ffb_urbs[effect->id][1], GFP_ATOMIC);
		if(errno) {
			hid_err(tmx->hid_device, "submitting ffb 1 urb of effect %d, error %d\n", effect->id ,errno);
			return errno;
		}
	}

	if(TMX_FF_BLIND_UPLOAD || !old || memcmp(&ff_commit_old, &ff_commit_new, sizeof(struct ff_commit))){
		usb_kill_urb(tmx->update_ffb_urbs[effect->id][2]);

		memcpy(tmx->update_ffb_urbs[effect->id][2]->transfer_buffer, &ff_commit_new, sizeof(struct ff_commit));
		errno = usb_submit_urb(tmx->update_ffb_urbs[effect->id][2], GFP_ATOMIC);
		if(errno) {
			hid_err(tmx->hid_device, "submitting ffb 2 urb of effect %d, error %d\n", effect->id ,errno);
			return errno;
		}
	}

	return 0;

free1:	tmx_ff_free_urb(tmx->update_ffb_urbs[effect->id][1]);
	tmx->update_ffb_urbs[effect->id][1] = 0;
free0:	tmx_ff_free_urb(tmx->update_ffb_urbs[effect->id][0]);
	tmx->update_ffb_urbs[effect->id][0] = 0;
	return -ENOMEM;
}

/**
 * Function used to erase an effect already uploaded to the Wheel
 * @param dev 
 * @param effect_id the ID of the effect to be erased
 * 
 * @return 0 if no errors occured
 */
static int tmx_ff_erase(struct input_dev *dev, int effect_id)
{
	/** When an effect is destroyed also a request to stop it is sent to 
	 * tmx_ff_play. Observing the Windows's driver seems there isn't any
	 * specific packet to explicity destory the effect, so we return success (0)
	 * to notify the Kernel that the id can be freed and re-used for another
	 * effect. 
	 */
	return 0;
}

/**
 * Function used to play an effect already uploaded to the Wheel
 * If times==0 then the function will send to the wheel a request
 * to stop playing the effect.
 * @param dev 
 * @param effect_id the ID of the effect to be played
 * @param times how many times the effect should be played. If the effect
 * 	is beign erased a play request in times=0 is also sent.
 * 
 * @return 0 if no errors occured
 */
static int tmx_ff_play(struct input_dev *dev, int effect_id, int times)
{
	struct tmx *tmx = input_get_drvdata(dev);
	struct urb *urb;
	struct ff_change_effect_status *ff_change;
	int errno;

	// Alloc urb
	urb = tmx_ff_alloc_urb(tmx, sizeof(struct ff_change_effect_status));
	if(!urb)
		return -ENOMEM;
	ff_change = urb->transfer_buffer;

	ff_change->f0 = 0x41;
	ff_change->id = effect_id;
	ff_change->mode = times ? 0x41 : 0x00; // Play or stop ?
	ff_change->times = times ? times : 0x01;

	urb->complete = tmx_ff_free_urb;
	errno = usb_submit_urb(urb, GFP_KERNEL);
	if(errno)
		hid_err(tmx->hid_device, "unable to send URB to play effect n %d, errno %d\n", effect_id ,errno);

	return errno;
}

/**
 * @param dev
 * @param gain 0xFFFF = 100% of gain 
 */
static void tmx_ff_set_gain(struct input_dev *dev, uint16_t gain)
{
	struct tmx *tmx = input_get_drvdata(dev);
	int errno;
	struct urb *urb;
	struct ff_change_gain *ff_change;
	unsigned long flags;

	urb = tmx_ff_alloc_urb(tmx, sizeof(struct ff_change_gain));
	if(!urb)
		return; // -NOMEM

	ff_change = urb->transfer_buffer;
	
	ff_change->f0 = 0x43;
	ff_change->gain = DIV_ROUND_CLOSEST(gain, 0x1ff);

	spin_lock_irqsave(&tmx->settings.access_lock, flags);
	tmx->settings.gain = ff_change->gain;
	spin_unlock_irqrestore(&tmx->settings.access_lock, flags);

	urb->complete = tmx_ff_free_urb;
	errno = usb_submit_urb(urb, GFP_KERNEL);
	if(errno)
		hid_err(tmx->hid_device, "unable to send URB to set gain, errno %i\n", errno);
}
