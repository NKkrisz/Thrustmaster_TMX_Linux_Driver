struct tmx_state_packet;

static inline int tmx_init_input(struct tmx *tmx);
static inline void tmx_free_input(struct tmx *tmx);
static int tmx_input_open(struct input_dev *dev);
static void tmx_input_close(struct input_dev *dev);
static int tmx_update_input(struct hid_device *hdev, struct hid_report *report, uint8_t *packet_raw, int size);

static uint16_t *packet_input_open = 0;
/** It seems it's used to purge all uploaded effects from the wheel, not sure */
static uint16_t *packet_input_what = 0;
static uint16_t *packet_input_close = 0; 
