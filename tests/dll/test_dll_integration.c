/**
 * @file test_dll_integration.c
 * @brief Integration tests for complete DLL functionality
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "ethercat/dll.h"
#include "ethercat/frame.h"
#include "ethercat/frame_builder.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Callback test variables */
static bool send_callback_invoked = false;
static bool receive_callback_invoked = false;

/* ========================================================================== */
/* Test Helpers                                                               */
/* ========================================================================== */

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("  [PASS] %s\n", message); \
        } else { \
            tests_failed++; \
            printf("  [FAIL] %s\n", message); \
        } \
    } while(0)

#define TEST_START(name) \
    printf("\n=== Test: %s ===\n", name)

/* ========================================================================== */
/* Callback Functions                                                         */
/* ========================================================================== */

static void test_send_callback(const dl_send_con_t* con)
{
    (void)con;
    send_callback_invoked = true;
}

static void test_receive_callback(const dl_receive_ind_t* ind)
{
    (void)ind;
    receive_callback_invoked = true;
}

/* ========================================================================== */
/* Test Cases                                                                 */
/* ========================================================================== */

void test_dll_init_shutdown(void)
{
    TEST_START("DLL Initialization and Shutdown");

    /* Initialize configuration */
    dl_config_t config;
    dl_status_t status = dl_config_init_defaults(&config);
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "Config initialization");

    /* Set MAC address */
    uint8_t mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    memcpy(config.mac_address, mac, 6);

    /* Initialize DLL */
    status = dl_init(&config);
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "DLL initialization");

    /* Check state */
    dl_state_t state = dl_get_state();
    TEST_ASSERT(state == DL_STATE_INITIALIZED, "State is INITIALIZED");

    /* Check queue counts */
    TEST_ASSERT(dl_get_tx_queue_count() == 0, "TX queue is empty");
    TEST_ASSERT(dl_get_rx_queue_count() == 0, "RX queue is empty");

    /* Shutdown DLL */
    status = dl_shutdown();
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "DLL shutdown");

    /* Check state */
    state = dl_get_state();
    TEST_ASSERT(state == DL_STATE_UNINITIALIZED, "State is UNINITIALIZED");
}

void test_dll_start_stop(void)
{
    TEST_START("DLL Start and Stop");

    /* Initialize */
    dl_config_t config;
    dl_config_init_defaults(&config);
    uint8_t mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    memcpy(config.mac_address, mac, 6);
    dl_init(&config);

    /* Start DLL */
    dl_status_t status = dl_start();
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "DLL start");

    /* Check state */
    dl_state_t state = dl_get_state();
    TEST_ASSERT(state == DL_STATE_READY, "State is READY");

    /* Stop DLL */
    status = dl_stop();
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "DLL stop");

    /* Check state */
    state = dl_get_state();
    TEST_ASSERT(state == DL_STATE_INITIALIZED, "State is INITIALIZED");

    /* Cleanup */
    dl_shutdown();
}

void test_dll_parameter_access(void)
{
    TEST_START("DLL Parameter Access");

    /* Initialize */
    dl_config_t config;
    dl_config_init_defaults(&config);
    uint8_t mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    memcpy(config.mac_address, mac, 6);
    dl_init(&config);

    /* Get MAC address */
    uint8_t read_mac[6];
    uint16_t length = 6;
    dl_status_t status = dl_get_parameter(DL_PARAM_MAC_ADDRESS, read_mac, &length);
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "Get MAC address");
    TEST_ASSERT(memcmp(read_mac, mac, 6) == 0, "MAC address matches");

    /* Get cycle time */
    uint32_t cycle_time;
    length = sizeof(cycle_time);
    status = dl_get_parameter(DL_PARAM_CYCLE_TIME, &cycle_time, &length);
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "Get cycle time");
    TEST_ASSERT(cycle_time == DL_DEFAULT_CYCLE_TIME_US, "Cycle time is default");

    /* Set cycle time */
    cycle_time = 500;
    status = dl_set_parameter(DL_PARAM_CYCLE_TIME, &cycle_time, sizeof(cycle_time));
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "Set cycle time");

    /* Verify new cycle time */
    uint32_t new_cycle_time;
    length = sizeof(new_cycle_time);
    dl_get_parameter(DL_PARAM_CYCLE_TIME, &new_cycle_time, &length);
    TEST_ASSERT(new_cycle_time == 500, "Cycle time updated");

    /* Cleanup */
    dl_shutdown();
}

void test_dll_callback_registration(void)
{
    TEST_START("DLL Callback Registration");

    /* Initialize */
    dl_config_t config;
    dl_config_init_defaults(&config);
    uint8_t mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    memcpy(config.mac_address, mac, 6);
    dl_init(&config);

    /* Register send callback */
    dl_status_t status = dl_register_send_callback(test_send_callback);
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "Register send callback");

    /* Register receive callback */
    status = dl_register_receive_callback(test_receive_callback);
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "Register receive callback");

    /* Cleanup */
    dl_shutdown();
}

void test_dll_send_request(void)
{
    TEST_START("DLL Send Request");

    /* Initialize and start */
    dl_config_t config;
    dl_config_init_defaults(&config);
    uint8_t mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    memcpy(config.mac_address, mac, 6);
    dl_init(&config);
    dl_start();

    /* Build a test frame */
    uint8_t frame_buffer[1518];
    ecat_frame_builder_t builder;
    uint8_t src_mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    ecat_frame_builder_init(&builder, frame_buffer, sizeof(frame_buffer), src_mac, dst_mac);

    uint32_t addr = ecat_addr_autoincrement(1, 0x0000);
    ecat_frame_builder_add_datagram(&builder, ECAT_CMD_APRD, 0, addr, NULL, 4, false);

    uint16_t frame_length;
    ecat_frame_builder_finalize(&builder, &frame_length);

    /* Send frame */
    dl_send_req_t req;
    req.frame_data = frame_buffer;
    req.frame_length = frame_length;
    req.priority = 0;
    req.user_data = NULL;

    dl_status_t status = dl_send_req(&req);
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "Send request");

    /* Check state transitioned to RUNNING */
    dl_state_t state = dl_get_state();
    TEST_ASSERT(state == DL_STATE_RUNNING, "State is RUNNING");

    /* Check TX queue count */
    TEST_ASSERT(dl_get_tx_queue_count() == 1, "TX queue has 1 frame");

    /* Cleanup */
    dl_stop();
    dl_shutdown();
}

void test_dll_statistics(void)
{
    TEST_START("DLL Statistics");

    /* Initialize */
    dl_config_t config;
    dl_config_init_defaults(&config);
    uint8_t mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    memcpy(config.mac_address, mac, 6);
    dl_init(&config);

    /* Get statistics */
    dl_statistics_t stats;
    dl_status_t status = dl_get_statistics(&stats);
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "Get statistics");
    TEST_ASSERT(stats.frames_sent == 0, "Initial frames_sent is 0");
    TEST_ASSERT(stats.frames_received == 0, "Initial frames_received is 0");

    /* Reset statistics */
    status = dl_reset_statistics();
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "Reset statistics");

    /* Cleanup */
    dl_shutdown();
}

void test_dll_queue_operations(void)
{
    TEST_START("DLL Queue Operations");

    /* Initialize and start */
    dl_config_t config;
    dl_config_init_defaults(&config);
    uint8_t mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    memcpy(config.mac_address, mac, 6);
    dl_init(&config);
    dl_start();

    /* Send multiple frames */
    uint8_t frame_buffer[1518];
    for (int i = 0; i < 3; i++) {
        ecat_frame_builder_t builder;
        uint8_t src_mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
        uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

        ecat_frame_builder_init(&builder, frame_buffer, sizeof(frame_buffer), src_mac, dst_mac);
        uint32_t addr = ecat_addr_autoincrement(i + 1, 0x0000);
        ecat_frame_builder_add_datagram(&builder, ECAT_CMD_APRD, i, addr, NULL, 4, false);

        uint16_t frame_length;
        ecat_frame_builder_finalize(&builder, &frame_length);

        dl_send_req_t req;
        req.frame_data = frame_buffer;
        req.frame_length = frame_length;
        req.priority = i;
        req.user_data = NULL;

        dl_send_req(&req);
    }

    /* Check TX queue count */
    TEST_ASSERT(dl_get_tx_queue_count() == 3, "TX queue has 3 frames");

    /* Flush TX queue */
    dl_status_t status = dl_flush_tx_queue();
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "Flush TX queue");
    TEST_ASSERT(dl_get_tx_queue_count() == 0, "TX queue is empty after flush");

    /* Cleanup */
    dl_stop();
    dl_shutdown();
}

/* ========================================================================== */
/* Main Test Runner                                                           */
/* ========================================================================== */

int main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("DLL Integration Tests\n");
    printf("========================================\n");

    /* Run all tests */
    test_dll_init_shutdown();
    test_dll_start_stop();
    test_dll_parameter_access();
    test_dll_callback_registration();
    test_dll_send_request();
    test_dll_statistics();
    test_dll_queue_operations();

    /* Print summary */
    printf("\n");
    printf("========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Total tests:  %d\n", tests_run);
    printf("Passed:       %d\n", tests_passed);
    printf("Failed:       %d\n", tests_failed);
    printf("========================================\n");

    if (tests_failed == 0) {
        printf("\nAll tests PASSED!\n\n");
        return 0;
    } else {
        printf("\nSome tests FAILED!\n\n");
        return 1;
    }
}
