#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/battery.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>


#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) || !(IS_ENABLED(CONFIG_ZMK_SPLIT))
#include <zmk/keymap.h>
#include <zmk/ble.h>
#include <zmk/hid_indicators.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/hid_indicators_changed.h>
#define HAS_CENTRAL_STATE 1
#endif

#include "led_status.h"

LOG_MODULE_REGISTER(led_status, LOG_LEVEL_INF); // TODO: check logging options and necessity
// what is displayed on the leds
enum led_display {
    off, discharging, charging, low_battery_warn, bluetooth, indicators
};
static volatile enum led_display display_state = off;
// state to recover, after a temporary display was activated.
// This is usually off, but if we charge, we may want to go back to the charging animation
static volatile enum led_display base_state = off;
// to change the state, this function needs to be called.
// it turns off whatever there was previously, and turn on the new thing
// Note: changing from discharging to discharging (same state) will update the displayed charge
static void set_display_state(led_display new_state) {
    led_display old_state = display_state;
    // TODO: check if the current state is set to terminate
    // after x seconds, if yes, stop callback
    switch (old_state) {
        case off:
            break;
        case discharging:
            break;
        case charging:
            // this is an animation, which has to be stopped
            charge_anim_stop();
            break;
        case low_battery_warn:
            // TODO: once implemented, stop the low battery animation
            break;
        case bluetooth:
            break;
        case indicators:
            break;
        // omitting default case, hoping that the compiler warns, if not all enum cases match
    }
    
    display_state = new_state;
    switch (new_state) {
        case off:
            set_leds(false, false, false);
            break;
        case discharging:
            show_battery_discharging();
            break;
        case charging:
            // this is an animation, which has to be stopped
            charge_anim_start();
            break;
        case low_battery_warn:
            break;
        case bluetooth:
            // TODO: write bluetooth on function
            break;
        case indicators:
            // TODO: write indicators function
            break;
    }
}

// fetch led definitions from device tree file (on user side)
// expected order: led0: inner led, led1: middle led, led2: outer led
static const struct pwm_dt_spec leds[3] = {
    PWM_DT_SPEC_GET(DT_NODELABEL(status_led_0)),
    PWM_DT_SPEC_GET(DT_NODELABEL(status_led_1)),
    PWM_DT_SPEC_GET(DT_NODELABEL(status_led_2)),
};

// To save power, the led's should be off most of the time.
// this variable gets turned on, when the keyboard gets turned on
// and turns off after BATTERY_SHOW_MS

// Set the led at the given index (0 - 3) to the given brightness in permille 0 = off, 1000 = full brightness 
static void set_level(int led_idx, uint32_t permille) {
    // the second argument is a uint32 representing the pulse duration in nanoseconds.
    // the maximum value of a uint32 is 4'294'967'295, or 4.294'967'295 seconds.
    // note: if the pwm period is set to longer than 4'294'967 nanoseconds (4 milliseconds, ), this will result in a overflow.
    // clamp permille to max 1000
    uint32_t c_permille = (permille > 1000) ? 1000 : permille;
    pwm_set_pulse_dt(&leds[led_idx], leds[led_idx].period * c_permille / 1000);
}

// Turn the led's on or off
static void set_leds(bool led0, bool led1, bool led2) {
    pwm_set_pulse_dt(&leds[0], led0 ? leds[0].period : 0);
    pwm_set_pulse_dt(&leds[1], led0 ? leds[1].period : 0);
    pwm_set_pulse_dt(&leds[2], led0 ? leds[2].period : 0);
}
// reperesents the current charge level, as to be displayed on the led's
// the passed list led_levels must have length 3 (or longer)
// with a number between 0 and 1000. 0 = off, 1000 = fully on.
// this function does not write to the leds, to allow superimposing a charging animation.
static void led_charge_brightness(uint32_t led_levels[], uint8_t charge) {
    // using uint32_t, because at 100%, charge*3 = 300, which is bigger than uint8 max (255)
    uint32_t max_charge = 100;
    uint32_t charge_led1_full = 33;
    uint32_t charge_led2_full = 66;
    uint32_t permille_per_percent = 10; // to convert to the expected unit of set_level
    // note: this line relies on the clamping of the set_level function
    led_levels[0] = (uint32_t)charge*(max_charge/charge_led1_full)*permille_per_percent;
    // basically clamps charge to be between charge_led1_full and charge_led2_full, before doing the linear interpolation
    // to avoid over / underflows
    led_levels[1] = (charge > charge_led1_full) ? ((charge < charge_led2_full ? ( // clamping logic
        // linear interpolation (if both ternary statements are true -> no clamping)
        (charge - charge_led1_full) * (max_charge/((charge_led2_full - charge_led1_full))*permille_per_percent)
    ) : 1000) ) : 0; // clamping values

    led_levels[2] = (charge > charge_led2_full) ? ((charge < max_charge ? (
        (charge - charge_led2_full) * (max_charge/((max_charge - charge_led2_full))*permille_per_percent)
    ) : 1000) ) : 0; 
} 

// changes the brightness of the three led's depending on the reported battery state.
// on full charge, all 3 led's are fully on
// on 66% charge, only led1 and 2 are on.
// on 50% charge, led1 is fully on and led2 is at 50% brightness
// etc.
// This function does not indicate if we are charging.
static void show_battery_discharging() {
    uint8_t charge = zmk_battery_state_of_charge();
    // TODO: low battery blink
    uint32_t led_levels[] = {0, 0, 0};
    led_charge_brightness(led_levels, charge);
    set_level(0, led_levels[0]);
    set_level(1, led_levels[1]);
    set_level(2, led_levels[2]);
}
// triangle function starting at zero, linearly increasing to amplitude, and then linearly decreasing to zero, clamped to zero before and after x = period
static uint32_t triangle(uint32_t x, uint32_t amplitude, uint32_t period) {
    uint32_t c_x = (x<0) ? 0 : ( (x>period) ? period : x); 
    return (c_x<period/2) ? c_x*amplitude*2/period : (period-c_x)*amplitude*2/period;
}
static uint32_t max(uint32_t a, uint32_t b) {
    return (a>b) ? a : b;
}
// sets led's to the "animation frame" at the given time
// basically plays / defines the animation, when called periodically
static void show_battery_charging(uint32_t anim_time_ms) {
    //uint8_t charge = zmk_battery_state_of_charge();
    uint32_t led_levels[] = {0, 0, 0};
    // commented out, to test the charging animation
    // (TODO: check if charge level is valid when charging)
    //led_charge_brightness(led_levels, charge);
    uint32_t fade_period_ms = 2000;
    uint32_t fade_amplitude = 1000; // led brightness in permille
    // the individual led triangle functions overlap 50%, after 2*fade_period_ms,
    // the third led will be off again. At that point, we want to restart the animation
    uint32_t anim_cycle_time = 2*fade_period_ms;
    // start with led0 fully off
    // offsets chosen to keep the total brightness 100% (ex. after fade_period_ms/2, led1 is at 50%, led2 at 50%)
    // note that the numbers will underflow in the first few seconds. We don't care about this here, because the triangle
    // function returns 0 for all values outside [0..fade_period].
    uint32_t x = anim_cycle_time % anim_cycle_time;
    uint32_t led0_brightness = triangle(x, fade_amplitude, fade_period_ms);
    uint32_t led1_brightness = triangle(x - fade_period_ms, fade_amplitude, fade_period_ms);
    uint32_t led2_brightness = triangle(x - 2*fade_period_ms, fade_amplitude, fade_period_ms);

    set_level(0, max(led_levels[0], led0_brightness));
    set_level(1, max(led_levels[1], led1_brightness));
    set_level(2, max(led_levels[2], led2_brightness));

}

//--------------------
// event callback definitions

static void charge_anim_start() {
    //show_charge_animation = true;
    //display_state = charging; // set to invoce this function, I don't want recursion here
    anim_start_time = k_uptime_get_32();
    k_work_reschedule(&charge_anim, K_NO_WAIT); // starts animation immediately
}
// NOTE: only stops the animation, without updating the led's
// if this function is called without calling update work callback, the led's stay on the last frame of the animation
// (this is done, because I probably want to switch to another presentation immediately, and therefore don't want to
// turn all (pwm's of the) led's off, just to re-enable them instantly)
static void charge_anim_stop() {
    //show_charge_animation = false;
    //display_state = off;
    set_leds(false, false, false);
    k_work_cancel_delayable(&charge_anim);
}
static void charge_anim_callback(struct k_work *work) {
    if (!display_state = charging) {
        return;
    }
    uint32_t anim_time = k_uptime_get_32()-anim_start_time;
    show_battery_charging(anim_time);
    k_work_reschedule(&charge_anim, K_MSEC(LED_FADE_STEP_MS));
}

// started through the zephyr work queue (update work)
// updates the led's according to global variables set via zmk subscriber callback
static void update_work_callback(struct k_work *work) {
    return; // I don't really need this...
}
// triggered through a timer, disabling the led's
static void timeout_work_callback(struct k_work *work) {
    //show_battery = false;
    //show_charge_animation = false;
    set_display_state(base_state);
}

// ZMK events

// the callback, which controls the led's
static int led_status_listener(const zmk_event_t *eh);

ZMK_LISTENER(led_status, led_status_listener);

ZMK_SUBSCRIPTION(led_status, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(led_status, zmk_activity_state_changed);
ZMK_SUBSCRIPTION(led_status, zmk_usb_conn_state_changed);
#ifdef HAS_CENTRAL_STATE
ZMK_SUBSCRIPTION(led_status, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(led_status, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(led_status, zmk_hid_indicators_changed);
#endif
// triggers when a subscribed event happens.
// handles what is going to happen
static int led_status_listener(const zmk_event_t *eh) {
    if (as_zmk_battery_state_changed(eh) != NULL) {
        uint8_t state_of_charge = as_zmk_battery_state_changed(eh)->state_of_charge;
        // if state_of_charge is low, blink the led
        //TODO
    }
    struct zmk_activity_state_changed *ev_act = as_zmk_activity_state_changed(eh);
    if (ev_act != NULL) {
        switch (ev_act->state) {
            case ZMK_ACTIVITY_ACTIVE:
                //show_battery = true;
                // sets show_battery to false and updates led's after some time
                //k_work_submit(&update_work);
                // show battery for BATTERY_SHOW_MS (milliseconds)
                set_display_state(discharging);
                // this will turn the display off
                k_work_reschedule(&timeout_work, K_MSEC(BATTERY_SHOW_MS));
                break;
            case ZMK_ACTIVITY_IDLE:
                //show_battery = false;
                //k_work_submit(&update_work);
                k_work_cancel_delayable(&timeout_work);
                //set_display_state(off);
                break;
            case ZMK_ACTIVITY_SLEEP:
                //show_battery = false;
                //k_work_submit(&update_work);
                k_work_cancel_delayable(&timeout_work);
                //set_display_state(off);
                break;
            default:
                break;
        }
    }
#ifdef HAS_CENTRAL_STATE
    struct zmk_layer_state_changed *ev_lay = as_zmk_layer_state_changed(eh);
    if (ev_lay != NULL) {
        bool display_bluetooth = zmk_keymap_layer_active(BLUETOOTH_LAYER);
        bool display_indicators = zmk_keymap_layer_active(INDICATOR_LAYER);
        bool home = zmk_keymap_layer_active(HOME_LAYER);
        //bluetooth
        // TODO: find better way to check if display bluetooth is the highest active layer with behaviour
        // because I may want another layer to force-show the battery state
        // NOTE: this would be easy, if we knew if bluetooth is actually the higher layer
        if ((display_bluetooth && !display_indicators) || ((display_bluetooth && display_indicators) && (BLUETOOTH_LAYER > INDICATOR_LAYER))) {
            set_display_state(bluetooth);
        } else if ((display_indicators && !display_bluetooth) || ((display_indicators && display_bluetooth) && INDICATOR_LAYER > BLUETOOTH_LAYER)){
            set_display_state(indicators);
        } else if (home) {
            set_dispay_state(base_state);
        }
        //k_work_submit(&update_work);
    }
#endif
    struct zmk_usb_conn_state_changed *ev_conn = as_zmk_usb_conn_state_changed(eh);
    if (ev_conn != NULL) {
        switch (ev_conn->conn_state) {
            case ZMK_USB_CONN_NONE:
                // show battery when unplugging
                base_state = off; // after the timeout, make the displays off
                set_display_state(discharging)
                k_work_reschedule(&timeout_work, K_MSEC(BATTERY_SHOW_MS));
                break;
            case ZMK_USB_CONN_POWERED:
                set_display_state(charging);
                base_state = charging;
                //k_work_submit(&update_work);
                break;
            case ZMK_USB_CONN_HID:
                // do not restart the charge animation, if we entered this mode from ZMK_USB_CONN_POWERED
                // (I don't care much about the other way around, because if a usb host suddenly stops being
                // a usb host and only powers... I feel like that deserves restarting the charging animation)
                if (display_state != charging) {
                    set_display_state(charging);
                    base_state = charging;
                }
                break;
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}


static int led_status_init(void) {
    for (int i = 0; i<3; i++) {
        if (!pwm_is_ready_dt(&leds[i])) {
            return -ENODEV;
        }
    }
    // debug charging animation
    set_display_state(charging);
    k_work_reschedule(&timeout_work, K_MSEC(BATTERY_SHOW_MS));
    return 0;
}

//SYS_INIT(led_status_init, APPLICATION, 90);
//              name,         stack_size, entrry, p1, p2, p3, Thread priority, Thread options, delay
K_THREAD_DEFINE(led_init_tid, 1024, led_status_init, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, 200);