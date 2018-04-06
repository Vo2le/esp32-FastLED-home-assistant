/*
 * System
 */
const char* hostname     = "esp32";
const char* ota_password = "password";

/*
 * WLAN
 */
const char* ssid     = "";
const char* password = "";

/*
 * MQTT
 */
const char* mqtt_server = "192.168.4.1";
const char* mqtt_topics[2] = {
  "/esp32/ws281x/0/set",
  "/esp32/ws281x/1/set"};


/*
 * FastLED
 */

#define STRIP_COUNT 2
#define STRIP_MAX_SIZE 100

// Strip 0
#define NUM_LEDS_0 284
#define DATA_PIN_0  25
#define LED_TYPE_0  WS2811
//#define CLOCK_PIN_0 8
// Strip 1
#define NUM_LEDS_1 46
#define DATA_PIN_1 2
#define LED_TYPE_1 WS2812B
//#define CLOCK_PIN_1 8

/*
 * Performance
 */
#define FRAMES_PER_SECOND 60
