#define BATTERY_SHOW_MS 5000
#define LED_FADE_STEP_MS 40 // time between led update during animations. 40 ms = 25 Hz

#define BLUETOOTH_LAYER 4
#define INDICATOR_LAYER 2
#define HOME_LAYER      0


// charge animation
uint32_t anim_start_time;
static void charge_anim_start();
static void charge_anim_stop();
static void charge_anim_callback(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(charge_anim, charge_anim_callback);

//discharging
static void show_battery_discharging();

// TODO: maybe restructure into different file / use header files
// event handling logic
// event callback setup
static void update_work_callback(struct k_work *work);
static void timeout_work_callback(struct k_work *work);
// work name, callback name
static K_WORK_DEFINE(update_work, update_work_callback);
static K_WORK_DELAYABLE_DEFINE(timeout_work, timeout_work_callback);

