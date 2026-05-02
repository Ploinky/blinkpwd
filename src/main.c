#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

// enable logging for this module using the default log level
LOG_MODULE_REGISTER(blinkpwd);

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)
#define SW2_NODE DT_ALIAS(sw2)
#define SW3_NODE DT_ALIAS(sw3)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);

static const struct gpio_dt_spec sw0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static const struct gpio_dt_spec sw1 = GPIO_DT_SPEC_GET(SW1_NODE, gpios);
static const struct gpio_dt_spec sw2 = GPIO_DT_SPEC_GET(SW2_NODE, gpios);
static const struct gpio_dt_spec sw3 = GPIO_DT_SPEC_GET(SW3_NODE, gpios);

static struct gpio_callback sw0_cb_data;
static struct gpio_callback sw1_cb_data;
static struct gpio_callback sw2_cb_data;
static struct gpio_callback sw3_cb_data;

static struct keypad {
        // true -> open, false -> locked
        bool lock;

        // current index into keypad_status while entering a keycode
        int current_digit;

        // records the entered digits as the buttons on the keypad are pressed
        // 0   ->   'empty' digit
        // 1-4 ->   digit was entered
        char input_status[4];

        // current password
        char pwd[4];
} keypad;


// stack area used by workqueue thread
static K_THREAD_STACK_DEFINE(my_stack_area, 512);

// Define queue structure
static struct k_work_q offload_work_q = {0};

struct work_info {
    struct k_work work;
    char name[5];
} my_work;

// sets up leds as output, off intially
// sets up switches as input, configures interrupts on edge to high
// sets up callbacks to button_pressed
bool setup_leds_and_switches(void);

// locks the lock, turns all leds off, resets input status
void lock_lock(void);

// unlocks the lock, turns all leds on, resets input status
void unlock_lock(void);

// switches all 4 leds on (true) or off (false)
void set_all_leds(bool on);

// blinks all leds on and off 3 times in 200ms intervals
// offloaded to a workqueue thread to avoid blocking in button press ISR
void blink_all_leds(struct k_work *work_term);

// reset input status of the keypad
void reset_keypad(void);

// main entry point to the application
int main(void)
{
        // set initial password
        keypad.pwd[0] = 1;
        keypad.pwd[1] = 3;
        keypad.pwd[2] = 2;
        keypad.pwd[3] = 4;

        // make sure lock is locked initially
        lock_lock();

        // set up workqueue used to blink leds asynchronously
        k_work_queue_start(&offload_work_q, my_stack_area, K_THREAD_STACK_SIZEOF(my_stack_area), 4, NULL);
        strcpy(my_work.name, "work");
        k_work_init(&my_work.work, blink_all_leds);

        if (!setup_leds_and_switches()) {
                return false;
        }

        return 0;
}

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
        char key_pressed = 0;
        if (pins & BIT(sw0.pin)) {
                key_pressed = 1;
        } else if (pins & BIT(sw1.pin)) {
                key_pressed = 2;
        } else if (pins & BIT(sw2.pin)) {
                key_pressed = 3;
        } else if (pins & BIT(sw3.pin)) {
                key_pressed = 4;
        }

        if (key_pressed == 0) {
                // invalid button press?!
                LOG_ERR("Invalid switch pressed: <%d>", key_pressed);
                return;
        }

        if (keypad.lock) {
                // lock is open, entering new pwd
                keypad.pwd[keypad.current_digit++] = key_pressed;

                if (keypad.current_digit > 3) {
                        // new pwd finished, lock the lock
                        lock_lock();
                        LOG_INF("new pwd: %d %d %d %d\r\n", keypad.pwd[0], keypad.pwd[1], keypad.pwd[2], keypad.pwd[3]);
                }
                return;
        }

        keypad.input_status[keypad.current_digit++] = key_pressed;

        if (keypad.current_digit < 4) {
                return;
        }

        // always a bit scary to initialize true, but fine for this project I think
        bool pwd_correct = true;
        for (int i = 0; i < 4; i++) {
                if (keypad.input_status[i] != keypad.pwd[i]) {
                        pwd_correct = false;
                }
        }

        if (!pwd_correct) {
                reset_keypad();
                // blink leds
                k_work_submit_to_queue(&offload_work_q, &my_work.work);
                return;
        }

        unlock_lock();
}

// check if the led is ready and configure it for output
// returns true if the led is ready and configured for output, false otherwise
bool setup_led(const struct gpio_dt_spec gpio)
{
        if (!device_is_ready(gpio.port)) {
                return false;
        }

        if (gpio_pin_configure_dt(&gpio, GPIO_OUTPUT) < 0) {
                return false;
        }
        
        if (gpio_pin_set_dt(&gpio, 0) < 0) {
                return false;
        }

        return true;
}

// check if the switch is ready and configure it for input
// returns true if the switch is ready and configured for input, false otherwise
bool setup_switch(const struct gpio_dt_spec gpio)
{
        if (!device_is_ready(gpio.port)) {
                return false;
        }

        if (gpio_pin_configure_dt(&gpio, GPIO_INPUT) < 0) {
                return false;
        }
        
        if (gpio_pin_interrupt_configure_dt(&gpio, GPIO_INT_EDGE_TO_ACTIVE) < 0) {
                return false;
        }

        return true;
}

bool setup_leds_and_switches(void)
{
        if (!setup_led(led0)) {
                LOG_ERR("failed to set up led0");
                return false;
        }

        if (!setup_led(led1)) {
                LOG_ERR("failed to set up led1");
                return false;
        }

        if (!setup_led(led2)) {
                LOG_ERR("failed to set up led2");
                return false;
        }

        if (!setup_led(led3)) {
                LOG_ERR("failed to set up led3");
                return false;
        }

        if (!setup_switch(sw0)) {
                LOG_ERR("failed to set up sw0");
                return false;
        }

        if (!setup_switch(sw1)) {
                LOG_ERR("failed to set up sw1");
                return false;
        }

        if (!setup_switch(sw2)) {
                LOG_ERR("failed to set up sw2");
                return false;
        }

        if (!setup_switch(sw3)) {
                LOG_ERR("failed to set up sw3");
                return false;
        }

        gpio_init_callback(&sw0_cb_data, button_pressed, BIT(sw0.pin));
        gpio_init_callback(&sw1_cb_data, button_pressed, BIT(sw1.pin));
        gpio_init_callback(&sw2_cb_data, button_pressed, BIT(sw2.pin));
        gpio_init_callback(&sw3_cb_data, button_pressed, BIT(sw3.pin));

        if (gpio_add_callback(sw0.port, &sw0_cb_data) < 0) {
                return false;
        }
        gpio_add_callback(sw1.port, &sw1_cb_data);
        gpio_add_callback(sw2.port, &sw2_cb_data);
        gpio_add_callback(sw3.port, &sw3_cb_data);

        LOG_INF("led and switch setup completed");

        return true;
}

void lock_lock(void)
{
        set_all_leds(false);
        keypad.lock = false;
        reset_keypad();
}

void unlock_lock(void)
{
        set_all_leds(true);
        keypad.lock = true;
        reset_keypad();
}

void set_all_leds(bool on)
{
        gpio_pin_set_dt(&led0, on ? 1 : 0);
        gpio_pin_set_dt(&led1, on ? 1 : 0);
        gpio_pin_set_dt(&led2, on ? 1 : 0);
        gpio_pin_set_dt(&led3, on ? 1 : 0);
}

void blink_all_leds(struct k_work *work_term)
{
        set_all_leds(true);
        k_msleep(200);
        set_all_leds(false);
        k_msleep(200);
        set_all_leds(true);
        k_msleep(200);
        set_all_leds(false);
        k_msleep(200);
        set_all_leds(true);
        k_msleep(200);
        set_all_leds(false);
}

void reset_keypad(void)
{
        keypad.input_status[0] = 0;
        keypad.input_status[1] = 0;
        keypad.input_status[2] = 0;
        keypad.input_status[3] = 0;
        keypad.current_digit = 0;
}
