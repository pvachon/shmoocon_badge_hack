#include <driver/uart.h>

#include <osapi.h>
#include <gpio.h>
#include <os_type.h>
#include <user_interface.h>

#include "c99_fixups.h"

#include <stdint.h>

typedef void (*freedom_outside_cb_t)(uint8 status);
int wifi_register_send_pkt_freedom_cb(freedom_outside_cb_t cb);
void wifi_unregister_send_pkt_freedom_cb(void);
int wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq); 

static
uint8_t beacon_header[] = {
	0x80, 0x00, //frame control - indicating a beacon frame
	0x00, 0x00, //duration - will be overwritten by ESP8266
	/*4*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, //DA - destination address, broadcast in this case
	/*10*/ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, //SA - source address, will be overwritten later
	/*16*/ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, //BSSID - same as SA in this case, will be overwritten later
	/*22*/ 0xc0, 0x6c, //Seq-ctl
	//Frame body starts here
	/*24*/ 0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, //timestamp
	/*32*/ 0x64, 0x00, //beacon interval
	/*34*/ 0x01, 0x04, //capability info
};

static
uint8_t beacon_footer[] = {
	0x01, //ID meaning Supported rates
	0x08, //length
	0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, //Supported rates
	0x03, //ID meaning channel
	0x01, //length
	0x06	/* Channel 6 */ 
};

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

#define SSID(_n) { .ssid = _n }

static
struct base_stations bssids[] = {
    SSID("01. Never Gonna"),
    SSID("02. Give You Up"),
    SSID("03. Never Gonna"),
    SSID("04. Let You Down"),
    SSID("05. Never Gonna"),
    SSID("06. Run Around"),
    SSID("07. And Desert You"),
    SSID("08. Never Gonna"),
    SSID("09. Make You Cry"),
    SSID("10. Never Gonna"),
    SSID("11. Say Goodbye"),
    SSID("12. Never Gonna"),
    SSID("13. Tell a Lie"),
    SSID("14. And Hurt You"),
};

#define INTERVAL    16

/**
 * Setup the wifi interface parameters, and initiate a connection to the STA.
 */
static ICACHE_FLASH_ATTR
void setup_wifi_interface(void)
{
}

static ICACHE_FLASH_ATTR
void _on_beacon_frame_cb(uint8_t status)
{
    /*
    os_printf("Beacon frame sent, status: %u\n", (unsigned)status);
    */
}

/**
 * Check and update the status of the Wifi connection. If the status indicates we're not connected, attempt to force a reconnect.
 */
static ICACHE_FLASH_ATTR
void send_beacon_frame(size_t id)
{
    uint8_t mac_addr[6] = { 0xec, 0xfa, 0x3e, 0x5d, 0x7e, 0x0 };
    uint8_t beacon_frame[sizeof(beacon_header) + sizeof(beacon_footer) + 1 + 1 + 32];
    size_t offs = sizeof(beacon_header);

    memcpy(beacon_frame, beacon_header, sizeof(beacon_header));
    beacon_frame[offs++] = 0x0;        /* indicates SSID */

    size_t ssid_len = strlen(bssids[id].ssid);

    beacon_frame[offs++] = ssid_len;
    memcpy(&beacon_frame[offs], bssids[id].ssid, ssid_len);
    offs += ssid_len;

    memcpy(&beacon_frame[offs], beacon_footer, sizeof(beacon_footer));


    mac_addr[5] = id;
#if 0
    os_printf("MAC addr is %02x:%02x:%02x-%02x:%02x:%02x\n",
            mac_addr[0],
            mac_addr[1],
            mac_addr[2],
            mac_addr[3],
            mac_addr[4],
            mac_addr[5]);
#endif

    beacon_frame[10] = beacon_frame[16] = mac_addr[0];
    beacon_frame[11] = beacon_frame[17] = mac_addr[1];
    beacon_frame[12] = beacon_frame[18] = mac_addr[2];
    beacon_frame[13] = beacon_frame[19] = mac_addr[3];
    beacon_frame[14] = beacon_frame[20] = mac_addr[4];
    beacon_frame[15] = beacon_frame[21] = mac_addr[5];

    size_t len = offs + sizeof(beacon_footer);
#if 0
    os_printf("Length: %u\n", len);

    for (size_t i = 0; i < len; i++) {
        if (i % 16 == 0) {
            os_printf("\n%04x: ", i);
        }
        os_printf("%02x ", beacon_frame[i]);
    }

    os_printf("\n");
#endif

    int ret1 = wifi_send_pkt_freedom(beacon_frame, len, 0);

    if (ret1 != 0) {
        os_printf("returns: %d\n", ret1);
    }
}

static
int last_id = 0;

/**
 */
static ICACHE_FLASH_ATTR
void update_essids(void *arg)
{
    /* Check the status of Wifi before we move along */
    send_beacon_frame(last_id);

    last_id = (last_id + 1) % ARRAY_LEN(bssids);
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
    os_delay_us(500);
    system_phy_set_max_tpw(4 * 20);
    wifi_set_opmode(STATION_MODE);
    wifi_promiscuous_enable(1);
    wifi_set_channel(6);
    wifi_register_send_pkt_freedom_cb(_on_beacon_frame_cb);

    /* Arm event timer (500ms, repeating) to sample the temperature probe */
    os_timer_disarm((os_timer_t *)&temp_timer);
    os_timer_setfn((os_timer_t *)&temp_timer, (os_timer_func_t *)update_essids, NULL);
    os_timer_arm((os_timer_t *)&temp_timer, INTERVAL, 1);
}

