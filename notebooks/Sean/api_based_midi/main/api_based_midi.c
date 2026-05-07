#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "driver/mcpwm_prelude.h"

static const char *TAG = "MOTOR_API_PRO";

#define WIFI_SSID           "Motor_Controller_AP"
#define WIFI_PASS           "password123"
#define SCNT 11 // Servo count

// NOTE: Deals with board revision pinout changes :D
#define R4
#if defined(R3)
// Rev 3 pinout {{{ 
#define AUX_PIN              2
#define MOTOR_PIN_A          1
#define MOTOR_PIN_B          3
#define SERVO_PIN            9       // GPIO for Servo Signal
const int solenoid_pins[SCNT] = {4, 5, 6, 7, 15, 16, 17, 18, 8, 10, 14};
// }}}
#elif defined(R4)
// Rev 4 pinout {{{
#define AUX_PIN              1
#define MOTOR_PIN_A          4
#define MOTOR_PIN_B          5
#define SERVO_PIN            9       // GPIO for Servo Signal
#define SOLF                 10
#define SOLR                 0
const int solenoid_pins[SCNT] = {21, 47, 6, 7, 15, 16, 17, 18, 8, 10};
// }}}
#else
#error "Board revision not defined"
#endif

#define PWM_RES_HZ           1000000 // 1MHz
#define PWM_PERIOD           1000    // 1kHz Frequency

#define SERVO_TIMEBASE_RES   1000000 // 1MHz (1 tick = 1 microsecond)
#define SERVO_TIMEBASE_PERIOD 20000   // 20000 ticks = 20ms (50Hz)
#define SERVO_MIN_PULSE      500     // 0.5ms (0 degrees)
#define SERVO_MAX_PULSE      2500    // 2.5ms (180 degrees)

static mcpwm_cmpr_handle_t servo_comparator;

static mcpwm_cmpr_handle_t comp_a;
static mcpwm_cmpr_handle_t comp_b;
static bool aux_state = false;
static bool solenoid_state[SCNT] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/**
 * Set servo angle
 * @param angle: 0 to 180
 */
void set_servo_angle(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    // Formula: pulse = ((angle / 180) * (max - min)) + min
    uint32_t pulse = (angle * (SERVO_MAX_PULSE - SERVO_MIN_PULSE) / 180) + SERVO_MIN_PULSE;
    mcpwm_comparator_set_compare_value(servo_comparator, pulse);
}

/**
 * URL: 192.168.4.1/play?speed=VALUE
 * Controls direction and duty cycle
 */
esp_err_t play_api_handler(httpd_req_t *req) {
    char buf[64];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char buf2[10];
        char buf3[10];
        if (httpd_query_key_value(buf, "a", buf2, sizeof(buf2)) == ESP_OK) {
            int speed = atoi(buf2);
            
            // Clamp speed between -100 and 100
            if (speed > 100) speed = 100;
            if (speed < -100) speed = -100;

            uint32_t duty = (abs(speed) * PWM_PERIOD) / 100;

            if (speed > 0) {
                // Forward
                solenoid_state[SOLF] = 1;
                solenoid_state[SOLR] = 0;
                mcpwm_comparator_set_compare_value(comp_a, duty);
                mcpwm_comparator_set_compare_value(comp_b, 0);
            } else if (speed < 0) {
                // Reverse
                solenoid_state[SOLR] = 1;
                solenoid_state[SOLF] = 0;
                mcpwm_comparator_set_compare_value(comp_a, 0);
                mcpwm_comparator_set_compare_value(comp_b, duty);
            } else {
                // Stop
                solenoid_state[SOLF] = 0;
                solenoid_state[SOLR] = 0;
                mcpwm_comparator_set_compare_value(comp_a, 0);
                mcpwm_comparator_set_compare_value(comp_b, 0);
            }
            
            ESP_LOGI(TAG, "Motor Speed: %d", speed);
            char resp[64];
            snprintf(resp, sizeof(resp), "{\"status\":\"ok\",\"val\":%d}", speed);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        }
        if (httpd_query_key_value(buf, "s", buf2, sizeof(buf2)) == ESP_OK) {
            int speed = atoi(buf2);
            aux_state = speed;
            gpio_set_level(AUX_PIN, aux_state);

            char resp[64];
            snprintf(resp, sizeof(resp), "{\"status\":\"ok\",\"pin_state\":%d}", aux_state);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        }
        if (httpd_query_key_value(buf, "h", buf2, sizeof(buf2)) == ESP_OK && httpd_query_key_value(buf, "a", buf3, sizeof(buf3)) == ESP_OK) {
            int whole_note = atoi(buf2);
            int active = atoi(buf3);
            ESP_LOGI(TAG, "%d is set: %d", whole_note, active);
            if(whole_note > 0 && whole_note <= SCNT) {
                solenoid_state[whole_note - 1] = active;
            }
            for(int i = 0; i < SCNT; i++) {
                gpio_set_level(solenoid_pins[i], solenoid_state[i]);
                ESP_LOGI(TAG, "%d is pin: %d and is set: %d", i, solenoid_pins[i], solenoid_state[i]);
            }

            char resp[64];
            snprintf(resp, sizeof(resp), "{\"status\":\"ok\",\"pin_state\":%d}", aux_state);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        }
        if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
            char ang_str[10];
            if (httpd_query_key_value(buf, "s", ang_str, sizeof(ang_str)) == ESP_OK) {
                int angle = atoi(ang_str);
                if(angle > 0) {
                  set_servo_angle(70);
                  ESP_LOGI(TAG, "Servo Angle set to: %d", 70);
                } else {
                  set_servo_angle(90);
                  ESP_LOGI(TAG, "Servo Angle set to: %d", 90);
                }
                httpd_resp_send(req, "{\"status\":\"ok\",\"type\":\"servo\"}", HTTPD_RESP_USE_STRLEN);
            }
        }
    }
    // httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Speed parameter missing");
    return ESP_FAIL;
}

void init_hardware() {
    // Setup Aux Toggle Pin
    gpio_reset_pin(AUX_PIN);
    gpio_set_direction(AUX_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(AUX_PIN, 0);
    for (int i = 0; i < SCNT; i++) {
      gpio_reset_pin(solenoid_pins[i]);
      gpio_set_direction(solenoid_pins[i], GPIO_MODE_OUTPUT);
      gpio_set_level(solenoid_pins[i], 0);
    }

    // Setup MCPWM for Motor
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_cfg = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = PWM_RES_HZ,
        .period_ticks = PWM_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_cfg, &timer));

    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t oper_cfg = { .group_id = 0 };
    ESP_ERROR_CHECK(mcpwm_new_operator(&oper_cfg, &oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    mcpwm_comparator_config_t comp_cfg = { .flags.update_cmp_on_tez = true };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comp_cfg, &comp_a));
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comp_cfg, &comp_b));

    mcpwm_gen_handle_t gen_a, gen_b;
    mcpwm_generator_config_t gen_cfg_a = { .gen_gpio_num = MOTOR_PIN_A };
    mcpwm_generator_config_t gen_cfg_b = { .gen_gpio_num = MOTOR_PIN_B };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &gen_cfg_a, &gen_a));
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &gen_cfg_b, &gen_b));

    // Define PWM wave: HIGH at start (0), LOW at compare match
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen_a, 
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_a, 
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comp_a, MCPWM_GEN_ACTION_LOW)));

    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(gen_b, 
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(gen_b, 
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comp_b, MCPWM_GEN_ACTION_LOW)));

    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    // Servo init
    mcpwm_timer_handle_t s_timer = NULL;
    mcpwm_timer_config_t s_timer_cfg = {
        .group_id = 1,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RES,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&s_timer_cfg, &s_timer));

    mcpwm_oper_handle_t s_oper = NULL;
    mcpwm_operator_config_t s_oper_cfg = { .group_id = 1 };
    ESP_ERROR_CHECK(mcpwm_new_operator(&s_oper_cfg, &s_oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(s_oper, s_timer));

    mcpwm_comparator_config_t s_comp_cfg = { .flags.update_cmp_on_tez = true };
    ESP_ERROR_CHECK(mcpwm_new_comparator(s_oper, &s_comp_cfg, &servo_comparator));

    mcpwm_gen_handle_t s_gen = NULL;
    mcpwm_generator_config_t s_gen_cfg = { .gen_gpio_num = SERVO_PIN };
    ESP_ERROR_CHECK(mcpwm_new_generator(s_oper, &s_gen_cfg, &s_gen));

    mcpwm_generator_set_action_on_timer_event(s_gen, 
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH));
    mcpwm_generator_set_action_on_compare_event(s_gen, 
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, servo_comparator, MCPWM_GEN_ACTION_LOW));

    ESP_ERROR_CHECK(mcpwm_timer_enable(s_timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(s_timer, MCPWM_TIMER_START_NO_STOP));
}

// WiFi AP Setup 
void init_wifi() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi AP Started. SSID:%s", WIFI_SSID);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_hardware();
    init_wifi();

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t play_uri = {
            .uri      = "/play",
            .method   = HTTP_GET,
            .handler  = play_api_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &play_uri);

        ESP_LOGI(TAG, "Server started on port: '%d'", config.server_port);
    } else {
        ESP_LOGE(TAG, "Failed to start server!");
    }
}
