#include <driver/uart.h>

#include <osapi.h>
#include <gpio.h>
#include <os_type.h>
#include <user_interface.h>

#include "c99_fixups.h"

#include <stdint.h>

#define ARRAY_LEN(x) (sizeof((x))/sizeof((x[0])))
#define ALIGN(x)        __attribute__((aligned((x))))

static
bool wifi_connected = false;

static
bool wifi_changed = false;

static
int wifi_last_status = STATION_IDLE;

static volatile
os_timer_t temp_timer;

struct base_stations {
    const char *ssid;
};

static
int id = 0;

#define SSID(_n) { .ssid = _n }

static
struct base_stations bssids[] = {
    SSID("1. Never Gonna"),
    SSID("2. Give You Up"),
    SSID("3. Never Gonna"),
    SSID("4. Let You Down"),
    SSID("5. Never Gonna"),
    SSID("6. Run Around"),
    SSID("7. And Desert You")
};

#define INTERVAL    10000

/**
 * Setup the wifi interface parameters, and initiate a connection to the STA.
 */
static ICACHE_FLASH_ATTR
void setup_wifi_interface(void)
{
}

/**
 * Check and update the status of the Wifi connection. If the status indicates we're not connected, attempt to force a reconnect.
 */
static ICACHE_FLASH_ATTR
void setup_wifi(void)
{
    struct softap_config cfg;
    uint8_t mac_addr[6];
    wifi_get_macaddr(SOFTAP_IF, mac_addr);
    mac_addr[5] = id;
    os_printf("MAC addr is %02x:%02x:%02x-%02x:%02x:%02x\n",
            mac_addr[0],
            mac_addr[1],
            mac_addr[2],
            mac_addr[3],
            mac_addr[4],
            mac_addr[5]);
    memset(&cfg, 0, sizeof(cfg));
    strcpy(cfg.ssid, bssids[id].ssid);
    cfg.ssid_len = strlen(bssids[id].ssid);
    os_printf("New ESSID: [%s] len=%d\n", cfg.ssid, cfg.ssid_len);
    strcpy(cfg.password, "lolhy");
    cfg.authmode = AUTH_WPA_PSK;
    cfg.channel = 6;
    ETS_UART_INTR_DISABLE();
    wifi_set_opmode(SOFTAP_MODE);
    wifi_set_macaddr(SOFTAP_IF, mac_addr);
    wifi_softap_set_config(&cfg);
    ETS_UART_INTR_ENABLE();
}

/**
 */
static ICACHE_FLASH_ATTR
void update_essids(void *arg)
{
    os_printf("Currently on ID %d\n", id);
    /* Check the status of Wifi before we move along */
    setup_wifi();

    id = (id + 1) % ARRAY_LEN(bssids);
    os_printf("ID will be now %d (%s)\n", id, bssids[id].ssid);
}

/**
 * Initialization entry point.
 */
ICACHE_FLASH_ATTR
void user_init(void)
{
    /* Initialize the UART */
    uart_init(BIT_RATE_115200, BIT_RATE_115200);
    os_delay_us(100);

    /*
     * Initialize the GPIO subsystem
     */
    gpio_init();

    /*
     * Print a welcome message
     */
    os_printf("ShmooCon is Starting...\r\n");

    /* Fire up the wifi interface */
    setup_wifi();

    /* Arm event timer (500ms, repeating) to sample the temperature probe */
    os_timer_disarm((os_timer_t *)&temp_timer);
    os_timer_setfn((os_timer_t *)&temp_timer, (os_timer_func_t *)update_essids, NULL);
    os_timer_arm((os_timer_t *)&temp_timer, INTERVAL, 1);
}

