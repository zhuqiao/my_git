/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <aos/aos.h>
#include <aos/yloop.h>
#include "netmgr.h"
#include "iot_export.h"
#include "iot_import.h"
#include "app_entry.h"
#include "aos/kv.h"
#include "vendor.h"
#include "device_state_manger.h"
#include "smart_outlet.h"
#include "msg_process_center.h"
#include "property_report.h"

#ifdef AOS_TIMER_SERVICE
#include "iot_export_timer.h"
#endif
#ifdef CSP_LINUXHOST
#include <signal.h>
#endif

#include <k_api.h>

#if defined(OTA_ENABLED) && defined(BUILD_AOS)
#include "ota_service.h"
#endif

#ifdef EN_COMBO_NET
#include "combo_net.h"
#endif

static aos_task_t task_key_detect;
static aos_task_t task_msg_process;
static aos_task_t task_property_report;
static aos_task_t task_linkkit_reset;
static aos_task_t task_reboot_device;
static aos_task_t task_userapp_process;

static char linkkit_started = 0;

#ifdef EN_COMBO_NET
char awss_running = 0;
char awss_success_flag = 0;
#else
static char awss_running = 0;
#endif
uint8_t g_ap_state = 0;
extern int init_awss_flag(void);

void do_awss_active(void);

#ifdef CONFIG_PRINT_HEAP
void print_heap()
{
    extern k_mm_head *g_kmm_head;
    int               free = g_kmm_head->free_size;
    LOG("============free heap size =%d==========", free);
}
#endif

static void wifi_service_event(input_event_t *event, void *priv_data)
{
    if (event->type != EV_WIFI) {
        return;
    }

    LOG("wifi_service_event(), event->code=%d.", event->code);
    if (event->code == CODE_WIFI_ON_CONNECTED) {
        LOG("CODE_WIFI_ON_CONNECTED");
    } else if (event->code == CODE_WIFI_ON_DISCONNECT) {
        LOG("CODE_WIFI_ON_DISCONNECT");
#ifdef EN_COMBO_NET
        g_ap_state = 0;
        if (!combo_ble_conn_state()) {
            aos_post_event(EV_BZ_COMBO, COMBO_EVT_CODE_RESTART_ADV, 0);
        }
#endif
    } else if (event->code == CODE_WIFI_ON_CONNECT_FAILED) {
        LOG("CODE_WIFI_ON_CONNECT_FAILED");
    } else if (event->code == CODE_WIFI_ON_GOT_IP) {
#ifdef EN_COMBO_NET
        g_ap_state = 1;
        if (!combo_ble_conn_state()) {
            aos_post_event(EV_BZ_COMBO, COMBO_EVT_CODE_RESTART_ADV, 0);
        }
#endif
    }

    if (event->code != CODE_WIFI_ON_GOT_IP) {
        LOG("not CODE_WIFI_ON_GOT_IP");
        return;
    }

    netmgr_ap_config_t config;
    memset(&config, 0, sizeof(netmgr_ap_config_t));
    netmgr_get_ap_config(&config);
    LOG("wifi_service_event config.ssid %s", config.ssid);
    if (strcmp(config.ssid, "adha") == 0 || strcmp(config.ssid, "aha") == 0) {
        // clear_wifi_ssid();
        return;
    }
#ifdef EN_COMBO_NET
    awss_success_flag = 1;
    uint8_t rsp[] = {0x01, 0x01, 0x01};
    if (combo_ble_conn_state()) {
        breeze_post(rsp, sizeof(rsp));
    }
#endif

    if (!linkkit_started) {
#ifdef CONFIG_PRINT_HEAP
        print_heap();
#endif
        aos_task_new("linkkit", (void (*)(void *))linkkit_main, NULL, 1024 * 8);
        linkkit_started = 1;
    }
}

static void cloud_service_event(input_event_t * event, void *priv_data)
{
    if (event->type != EV_YUNIO) {
        return;
    }

    LOG("cloud_service_event %d", event->code);

    if (event->code == CODE_YUNIO_ON_CONNECTED) {
        LOG("user sub and pub here");
        return;
    }

    if (event->code == CODE_YUNIO_ON_DISCONNECTED) {
    }
}

/*
 * Note:
 * the linkkit_event_monitor must not block and should run to complete fast
 * if user wants to do complex operation with much time,
 * user should post one task to do this, not implement complex operation in
 * linkkit_event_monitor
 */

static void linkkit_event_monitor(int event)
{
    switch (event) {
        case IOTX_AWSS_START: // AWSS start without enbale, just supports device discover
            // operate led to indicate user
            LOG("IOTX_AWSS_START");
            break;
        case IOTX_AWSS_ENABLE: // AWSS enable, AWSS doesn't parse awss packet until AWSS is enabled.
            LOG("IOTX_AWSS_ENABLE");
            // operate led to indicate user
            break;
        case IOTX_AWSS_LOCK_CHAN: // AWSS lock channel(Got AWSS sync packet)
            LOG("IOTX_AWSS_LOCK_CHAN");
            // operate led to indicate user
            break;
        case IOTX_AWSS_PASSWD_ERR: // AWSS decrypt passwd error
            LOG("IOTX_AWSS_PASSWD_ERR");
            // operate led to indicate user
            break;
        case IOTX_AWSS_GOT_SSID_PASSWD:
            LOG("IOTX_AWSS_GOT_SSID_PASSWD");
            // operate led to indicate user
            set_net_state(GOT_AP_SSID);
            break;
        case IOTX_AWSS_CONNECT_ADHA: // AWSS try to connnect adha (device
            // discover, router solution)
            LOG("IOTX_AWSS_CONNECT_ADHA");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_ADHA_FAIL: // AWSS fails to connect adha
            LOG("IOTX_AWSS_CONNECT_ADHA_FAIL");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_AHA: // AWSS try to connect aha (AP solution)
            LOG("IOTX_AWSS_CONNECT_AHA");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_AHA_FAIL: // AWSS fails to connect aha
            LOG("IOTX_AWSS_CONNECT_AHA_FAIL");
            // operate led to indicate user
            break;
        case IOTX_AWSS_SETUP_NOTIFY: // AWSS sends out device setup information
            // (AP and router solution)
            LOG("IOTX_AWSS_SETUP_NOTIFY");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_ROUTER: // AWSS try to connect destination router
            LOG("IOTX_AWSS_CONNECT_ROUTER");
            // operate led to indicate user
            break;
        case IOTX_AWSS_CONNECT_ROUTER_FAIL: // AWSS fails to connect destination
            // router.
            LOG("IOTX_AWSS_CONNECT_ROUTER_FAIL");
            set_net_state(CONNECT_AP_FAILED);
            // operate led to indicate user
            break;
        case IOTX_AWSS_GOT_IP: // AWSS connects destination successfully and got
            // ip address
            LOG("IOTX_AWSS_GOT_IP");
            // operate led to indicate user
            break;
        case IOTX_AWSS_SUC_NOTIFY: // AWSS sends out success notify (AWSS
            // sucess)
            LOG("IOTX_AWSS_SUC_NOTIFY");
            // operate led to indicate user
            break;
        case IOTX_AWSS_BIND_NOTIFY: // AWSS sends out bind notify information to
            // support bind between user and device
            LOG("IOTX_AWSS_BIND_NOTIFY");
            // operate led to indicate user
            user_example_ctx_t *user_example_ctx = user_example_get_ctx();
            user_example_ctx->bind_notified = 1;
            break;
        case IOTX_AWSS_ENABLE_TIMEOUT: // AWSS enable timeout
            // user needs to enable awss again to support get ssid & passwd of router
            LOG("IOTX_AWSS_ENALBE_TIMEOUT");
            // operate led to indicate user
            break;
        case IOTX_CONN_CLOUD: // Device try to connect cloud
            LOG("IOTX_CONN_CLOUD");
            // operate led to indicate user
            break;
        case IOTX_CONN_CLOUD_FAIL: // Device fails to connect cloud, refer to
            // net_sockets.h for error code
            LOG("IOTX_CONN_CLOUD_FAIL");
            set_net_state(CONNECT_CLOUD_FAILED);
            // operate led to indicate user
            break;
        case IOTX_CONN_CLOUD_SUC: // Device connects cloud successfully
            set_net_state(CONNECT_CLOUD_SUCCESS);
            LOG("IOTX_CONN_CLOUD_SUC");
            // operate led to indicate user
            break;
        case IOTX_RESET: // Linkkit reset success (just got reset response from
            // cloud without any other operation)
            LOG("IOTX_RESET");
            break;
        case IOTX_CONN_REPORT_TOKEN_SUC:
#ifdef EN_COMBO_NET
            {
                uint8_t rsp[] = { 0x01, 0x01, 0x03 };
                if (combo_ble_conn_state()) {
                    breeze_post(rsp, sizeof(rsp));
                }
            }
#endif
            LOG("---- report token success ----");
            break;
        default:
            break;
    }
}

#ifdef AWSS_BATCH_DEVAP_ENABLE
#define DEV_AP_ZCONFIG_TIMEOUT_MS  120000 // (ms)
extern void awss_set_config_press(uint8_t press);
extern uint8_t awss_get_config_press(void);
extern void zconfig_80211_frame_filter_set(uint8_t filter, uint8_t fix_channel);
void do_awss_dev_ap();

static aos_timer_t dev_ap_zconfig_timeout_timer;
static uint8_t g_dev_ap_zconfig_timer = 0; // this timer create once and can restart
static uint8_t g_dev_ap_zconfig_run = 0;

static void timer_func_devap_zconfig_timeout(void *arg1, void *arg2)
{
    LOG("%s run\n", __func__);

    if (awss_get_config_press()) {
        // still in zero wifi provision stage, should stop and switch to dev ap
        do_awss_dev_ap();
    } else {
        // zero wifi provision finished
    }

    awss_set_config_press(0);
    zconfig_80211_frame_filter_set(0xFF, 0xFF);
    g_dev_ap_zconfig_run = 0;
    aos_timer_stop(&dev_ap_zconfig_timeout_timer);
}

static void awss_dev_ap_switch_to_zeroconfig(void *p)
{
    LOG("%s run\n", __func__);
    // Stop dev ap wifi provision
    awss_dev_ap_stop();
    // Start and enable zero wifi provision
    iotx_event_regist_cb(linkkit_event_monitor);
    awss_set_config_press(1);

    // Start timer to count duration time of zero provision timeout
    if (!g_dev_ap_zconfig_timer) {
        aos_timer_new(&dev_ap_zconfig_timeout_timer, timer_func_devap_zconfig_timeout, NULL, DEV_AP_ZCONFIG_TIMEOUT_MS, 0);
        g_dev_ap_zconfig_timer = 1;
    }
    aos_timer_start(&dev_ap_zconfig_timeout_timer);

    // This will hold thread, when awss is going
    netmgr_start(true);

    LOG("%s exit\n", __func__);
    aos_task_exit(0);
}

int awss_dev_ap_modeswitch_cb(uint8_t awss_new_mode, uint8_t new_mode_timeout, uint8_t fix_channel)
{
    if ((awss_new_mode == 0) && !g_dev_ap_zconfig_run) {
        g_dev_ap_zconfig_run = 1;
        // Only receive zero provision packets
        zconfig_80211_frame_filter_set(0x00, fix_channel);
        LOG("switch to awssmode %d, mode_timeout %d, chan %d\n", 0x00, new_mode_timeout, fix_channel);
        // switch to zero config
        aos_task_new("devap_to_zeroconfig", awss_dev_ap_switch_to_zeroconfig, NULL, 2048);
    }
}
#endif

static void awss_close_dev_ap(void *p)
{
    awss_dev_ap_stop();
    aos_task_exit(0);
}

void awss_open_dev_ap(void *p)
{
    iotx_event_regist_cb(linkkit_event_monitor);
    /*if (netmgr_start(false) != 0) */{
        //aos_msleep(2000);
#ifdef AWSS_BATCH_DEVAP_ENABLE
        awss_dev_ap_reg_modeswit_cb(awss_dev_ap_modeswitch_cb);
#endif
        awss_dev_ap_start();
    }
    aos_task_exit(0);
}

void stop_netmgr(void *p)
{
    awss_stop();
    aos_task_exit(0);
}

void start_netmgr(void *p)
{
    iotx_event_regist_cb(linkkit_event_monitor);
    netmgr_start(true);
    aos_task_exit(0);
}

void do_awss_active(void)
{
    LOG("do_awss_active %d\n", awss_running);
    awss_running = 1;
#ifdef WIFI_PROVISION_ENABLED
    extern int awss_config_press();
    awss_config_press();
#endif
}

#ifdef EN_COMBO_NET
void ble_awss_open(void *p)
{
    iotx_event_regist_cb(linkkit_event_monitor);
    /*if (netmgr_start(false) != 0)*/ {
        //aos_msleep(2000);
        combo_net_init();
    }
    aos_task_exit(0);
}

void do_ble_awss()
{
    aos_task_new("netmgr_stop", stop_netmgr, NULL, 4096);
    //aos_msleep(2000);
    aos_task_new("ble_awss_open", ble_awss_open, NULL, 4096);
}
#endif

void do_awss_dev_ap()
{
    aos_task_new("netmgr_stop", stop_netmgr, NULL, 4096);
    aos_task_new("dap_open", awss_open_dev_ap, NULL, 4096);
}

void do_awss()
{
    aos_task_new("dap_close", awss_close_dev_ap, NULL, 2048);
    aos_task_new("netmgr_start", start_netmgr, NULL, 5120);
}

void linkkit_reset(void *p)
{
    aos_msleep(2000);
#ifdef AOS_TIMER_SERVICE
    timer_service_clear();
#endif
    iotx_sdk_reset_local();
#ifdef EN_COMBO_NET
    breeze_clear_bind_info();
#endif
    HAL_Reboot();
    aos_task_exit(0);
}

extern int  iotx_sdk_reset(iotx_vendor_dev_reset_type_t *reset_type);
iotx_vendor_dev_reset_type_t reset_type = IOTX_VENDOR_DEV_RESET_TYPE_UNBIND_ONLY;
void do_awss_reset(void)
{
#ifdef WIFI_PROVISION_ENABLED
    aos_task_new("reset", (void (*)(void *))iotx_sdk_reset, &reset_type, 6144);  // stack taken by iTLS is more than taken by TLS.
#endif
    aos_task_new_ext(&task_linkkit_reset, "reset task", linkkit_reset, NULL, 1024, 0);
    //aos_post_delayed_action(3000, linkkit_reset, NULL);
}

void reboot_device(void *p)
{
    aos_msleep(500);
    HAL_Reboot();
    aos_task_exit(0);
}

void do_awss_reboot(void)
{
    int ret;
    unsigned char awss_flag = 1;
    int len = sizeof(awss_flag);

    ret = aos_kv_set("awss_flag", &awss_flag, len, 1);
    if (ret != 0)
        LOG("KV Setting failed");

    aos_task_new_ext(&task_reboot_device, "reboot task", reboot_device, NULL, 1024, 0);
    //aos_post_delayed_action(500, reboot_device, NULL);
}

void linkkit_key_process(input_event_t *eventinfo, void *priv_data)
{
    if (eventinfo->type != EV_KEY) {
        return;
    }
    LOG("awss config press %d\n", eventinfo->value);

    if (eventinfo->code == CODE_BOOT) {
        if (eventinfo->value == VALUE_KEY_CLICK) {

            do_awss_active();
        } else if (eventinfo->value == VALUE_KEY_LTCLICK) {
            do_awss_reset();
        }
    }
}

#ifdef MANUFACT_AP_FIND_ENABLE
void manufact_ap_find_process(int result)
{
    // Informed manufact ap found or not.
    // If manufact ap found, lower layer will auto connect the manufact ap
    // IF manufact ap not found, lower layer will enter normal awss state
    if (result == 0) {
        LOG("%s ap found.\n", __func__);
    } else {
        LOG("%s ap not found.\n", __func__);
    }
}
#endif

#ifdef CONFIG_AOS_CLI
static void handle_reset_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    aos_schedule_call((aos_call_t)do_awss_reset, NULL);
}

static void handle_active_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    aos_schedule_call((aos_call_t)do_awss_active, NULL);
}

static void handle_dev_ap_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    aos_schedule_call((aos_call_t)do_awss_dev_ap, NULL);
}

#ifdef EN_COMBO_NET
static void handle_ble_awss_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    aos_schedule_call((aos_call_t)do_ble_awss, NULL);
}
#endif

static void handle_linkkey_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    if (argc == 1) {
        int len = 0;
        char product_key[PRODUCT_KEY_LEN + 1] = { 0 };
        char product_secret[PRODUCT_SECRET_LEN + 1] = { 0 };
        char device_name[DEVICE_NAME_LEN + 1] = { 0 };
        char device_secret[DEVICE_SECRET_LEN + 1] = { 0 };
        char pidStr[9] = { 0 };

        len = PRODUCT_KEY_LEN + 1;
        aos_kv_get("linkkit_product_key", product_key, &len);

        len = PRODUCT_SECRET_LEN + 1;
        aos_kv_get("linkkit_product_secret", product_secret, &len);

        len = DEVICE_NAME_LEN + 1;
        aos_kv_get("linkkit_device_name", device_name, &len);

        len = DEVICE_SECRET_LEN + 1;
        aos_kv_get("linkkit_device_secret", device_secret, &len);

        aos_cli_printf("Product Key=%s.\r\n", product_key);
        aos_cli_printf("Device Name=%s.\r\n", device_name);
        aos_cli_printf("Device Secret=%s.\r\n", device_secret);
        aos_cli_printf("Product Secret=%s.\r\n", product_secret);
        len = sizeof(pidStr);
        if (aos_kv_get("linkkit_product_id", pidStr, &len) == 0) {
            aos_cli_printf("Product Id=%d.\r\n", atoi(pidStr));
        }
    } else if (argc == 5 || argc == 6) {
        aos_kv_set("linkkit_product_key", argv[1], strlen(argv[1]) + 1, 1);
        aos_kv_set("linkkit_device_name", argv[2], strlen(argv[2]) + 1, 1);
        aos_kv_set("linkkit_device_secret", argv[3], strlen(argv[3]) + 1, 1);
        //aos_cli_printf("Device Secret is null.\n");
        aos_kv_set("linkkit_product_secret", argv[4], strlen(argv[4]) + 1, 1);
        if (argc == 6)
            aos_kv_set("linkkit_product_id", argv[5], strlen(argv[5]) + 1, 1);
        aos_cli_printf("Done");
    } else {
        aos_cli_printf("Error: %d\r\n", __LINE__);
        return;
    }
}

static void handle_awss_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    aos_schedule_call(do_awss, NULL);
}

static struct cli_command resetcmd = {
    .name = "reset",
    .help = "factory reset",
    .function = handle_reset_cmd
};

static struct cli_command awss_enable_cmd = {
    .name = "active_awss",
    .help = "active_awss [start]",
    .function = handle_active_cmd
};

static struct cli_command awss_dev_ap_cmd = {
    .name = "dev_ap",
    .help = "awss_dev_ap [start]",
    .function = handle_dev_ap_cmd
};

static struct cli_command awss_cmd = {
    .name = "awss",
    .help = "awss [start]",
    .function = handle_awss_cmd
};

#ifdef EN_COMBO_NET
static struct cli_command awss_ble_cmd = {
    .name = "ble_awss",
    .help = "ble_awss [start]",
    .function = handle_ble_awss_cmd
};
#endif

static struct cli_command linkkeycmd = {
    .name = "linkkey",
    .help = "set/get linkkit keys. linkkey [<Product Key> <Device Name> <Device Secret> <Product Secret>]",
    .function = handle_linkkey_cmd
};

#endif

#ifdef CONFIG_PRINT_HEAP
static void duration_work(void *p)
{
    print_heap();
    aos_post_delayed_action(5000, duration_work, NULL);
}
#endif

#if defined(OTA_ENABLED) && defined(BUILD_AOS)
static int ota_init(void);
static ota_service_t ctx = {0};
#endif
static int mqtt_connected_event_handler(void)
{
#if defined(OTA_ENABLED) && defined(BUILD_AOS)
    static bool ota_service_inited = false;

    if (ota_service_inited == true) {
        int ret = 0;

        LOG("MQTT reconnected, let's redo OTA upgrade");
        if ((ctx.h_tr) && (ctx.h_tr->upgrade)) {
            LOG("Redoing OTA upgrade");
            ret = ctx.h_tr->upgrade(&ctx);
            if (ret < 0) LOG("Failed to do OTA upgrade");
        }

        return ret;
    }

    LOG("MQTT Construct  OTA start to inform");
#ifdef DEV_OFFLINE_OTA_ENABLE
    ota_service_inform(&ctx);
#else
    ota_init();
#endif
    ota_service_inited = true;
#endif
    return 0;
}

static int ota_init(void)
{
#if defined(OTA_ENABLED) && defined(BUILD_AOS)
    char product_key[PRODUCT_KEY_LEN + 1] = {0};
    char device_name[DEVICE_NAME_LEN + 1] = {0};
    char device_secret[DEVICE_SECRET_LEN + 1] = {0};
    HAL_GetProductKey(product_key);
    HAL_GetDeviceName(device_name);
    HAL_GetDeviceSecret(device_secret);
    memset(&ctx, 0, sizeof(ota_service_t));
    strncpy(ctx.pk, product_key, sizeof(ctx.pk)-1);
    strncpy(ctx.dn, device_name, sizeof(ctx.dn)-1);
    strncpy(ctx.ds, device_secret, sizeof(ctx.ds)-1);
    ctx.trans_protcol = 0;
    ctx.dl_protcol = 3;
    ota_service_init(&ctx);
#endif
    return 0;
}

static void show_firmware_version(void)
{
    printf("\r\n--------Firmware info--------");
    printf("\r\nHost: %s", COMPILE_HOST);
    printf("\r\nBranch: %s", GIT_BRANCH);
    printf("\r\nHash: %s", GIT_HASH);
    printf("\r\nDate: %s %s", __DATE__, __TIME__);
    printf("\r\nKernel: %s", aos_get_kernel_version());
    printf("\r\nLinkKit: %s", LINKKIT_VERSION);
    printf("\r\nAPP: %s", aos_get_app_version());

    printf("\r\nRegion env: %s\r\n\r\n", REGION_ENV_STRING);
}

void write_mac_address(uint8_t * mac);
uint8_t * read_mac_address();
int application_start(int argc, char **argv)
{
    // uint8_t mac[6];  //测试用
    // mac[0] = 0x86;
    // mac[1] = 0x61;
    // mac[2] = 0xAB;
    // mac[3] = 0x00;
    // mac[4] = 0x00;
    // mac[5] = 0x02;
    // write_mac_address(mac);

#ifdef CONFIG_PRINT_HEAP
    print_heap();
    aos_post_delayed_action(5000, duration_work, NULL);
#endif

#ifdef CSP_LINUXHOST
    signal(SIGPIPE, SIG_IGN);
#endif

#ifdef WITH_SAL
    sal_init();
#endif

#ifdef MDAL_MAL_ICA_TEST
    HAL_MDAL_MAL_Init();
#endif

#ifdef DEFAULT_LOG_LEVEL_DEBUG
    IOT_SetLogLevel(IOT_LOG_DEBUG);
#else
    IOT_SetLogLevel(IOT_LOG_WARNING);
#endif

    show_firmware_version();
    /* Must call set_device_meta_info before netmgr_init */
    set_device_meta_info();
    netmgr_init();
    vendor_product_init();
    dev_diagnosis_module_init();
#ifdef DEV_OFFLINE_OTA_ENABLE
    ota_init();
#endif

#ifdef MANUFACT_AP_FIND_ENABLE
    netmgr_manuap_info_set("TEST_AP", "TEST_PASSWORD", manufact_ap_find_process);
#endif

    aos_register_event_filter(EV_KEY, linkkit_key_process, NULL);
    aos_register_event_filter(EV_WIFI, wifi_service_event, NULL);
    aos_register_event_filter(EV_YUNIO, cloud_service_event, NULL);
    IOT_RegisterCallback(ITE_MQTT_CONNECT_SUCC,mqtt_connected_event_handler);

#ifdef CONFIG_AOS_CLI
    aos_cli_register_command(&resetcmd);
    aos_cli_register_command(&awss_enable_cmd);
    aos_cli_register_command(&awss_dev_ap_cmd);
    aos_cli_register_command(&linkkeycmd);
    aos_cli_register_command(&awss_cmd);
#ifdef EN_COMBO_NET
    aos_cli_register_command(&awss_ble_cmd);
#endif
#endif
    init_awss_flag();
//    aos_task_new_ext(&task_key_detect, "detect key pressed", key_detect_event_task, NULL, 1024, AOS_DEFAULT_APP_PRI);

    init_msg_queue();
    aos_task_new_ext(&task_msg_process, "cmd msg process", msg_process_task, NULL, 2048, AOS_DEFAULT_APP_PRI - 1);
#ifdef REPORT_MULTHREAD
    aos_task_new_ext(&task_property_report, "property report", process_property_report_task, NULL, 2048, AOS_DEFAULT_APP_PRI);
#endif

//创建一个用户函数
    extern void userapp(void *argv);
    aos_task_new_ext(&task_userapp_process, "task_userapp_process", userapp, NULL, 4096, AOS_DEFAULT_APP_PRI);

    check_factory_mode();

    aos_loop_run();

    return 0;
}

//写入mac地址
//86:61:AB:00:00:02
void write_mac_address(uint8_t * mac)
{
    char str1[12];
    char str2[32] = {0};
    uint8_t temp[12];

    temp[0] = (mac[0] & 0xf0) >> 4;
    temp[1] = (mac[0] & 0x0f);
    temp[2] = (mac[1] & 0xf0) >> 4;
    temp[3] = (mac[1] & 0x0f);
    temp[4] = (mac[2] & 0xf0) >> 4;
    temp[5] = (mac[2] & 0x0f);
    temp[6] = (mac[3] & 0xf0) >> 4;
    temp[7] = (mac[3] & 0x0f);
    temp[8] = (mac[4] & 0xf0) >> 4;
    temp[9] = (mac[4] & 0x0f);
    temp[10] = (mac[5] & 0xf0) >> 4;
    temp[11] = (mac[5] & 0x0f);


    for(int i = 0 ; i < 12 ; i++)
    {
        if(temp[i] >= 0x0a)
        {
            str1[i] = temp[i] - 0x0a + 'A';
        }
        else
        {
            str1[i] = temp[i] + '0';
        }
    }

    str2[0] = str1[0];
    str2[1] = str1[1];
    str2[2] = ':';
    str2[3] = str1[2];
    str2[4] = str1[3];
    str2[5] = ':';
    str2[6] = str1[4];
    str2[7] = str1[5];
    str2[8] = ':';
    str2[9] = str1[6];
    str2[10] = str1[7];
    str2[11] = ':';
    str2[12] = str1[8];
    str2[13] = str1[9];
    str2[14] = ':';
    str2[15] = str1[10];
    str2[16] = str1[11];

    aos_kv_set("linkkit_device_name", str2, sizeof(str2), 1);

    hal_wifi_set_mac_addr(NULL, read_mac_address());

    printf("VSON write MAC : %s \n", str2);

//    read_mac_address();
}

//读取mac地址
uint8_t vson_mac[6] = {0};
uint8_t * read_mac_address()
{
    char str[32];
    int len = sizeof(str);
    if(aos_kv_get("linkkit_device_name", str, &len) != 0)
    {
        printf("read fail \n");
        memset(vson_mac,0,6);
        return;
    }
//    printf("VSON read MAC : %s \n", str);

    for(int i = 0 ; i < 17 ; i++)
    {
        if(str[i] != ':')
        {
            if(str[i] >= 'A')
            {
                str[i] = str[i] - 'A' + 0x0a;
            }
            else
            {
                str[i] -= '0';
            }
        }
    }
    vson_mac[0] = ( str[0] << 4 ) | str[1];
    vson_mac[1] = ( str[3] << 4 ) | str[4];
    vson_mac[2] = ( str[6] << 4 ) | str[7];
    vson_mac[3] = ( str[9] << 4 ) | str[10];
    vson_mac[4] = ( str[12] << 4 ) | str[13];
    vson_mac[5] = ( str[15] << 4 ) | str[16];
    printf("mac : %d %d %d %d %d %d \n", vson_mac[0],vson_mac[1],vson_mac[2],vson_mac[3],vson_mac[4],vson_mac[5]);
    return vson_mac;
}
