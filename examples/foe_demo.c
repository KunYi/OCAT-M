/**
 * @file foe_demo.c
 * @brief FoE (File over EtherCAT) Protocol Demonstration
 *
 * This example demonstrates the FoE protocol for file transfer and firmware updates.
 *
 * Features demonstrated:
 * - File upload (read from slave)
 * - File download (write to slave)
 * - Firmware update with progress tracking
 * - Error handling
 *
 * @author EtherCAT Master Stack
 * @date 2026-01-04
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>

#include "ethercat/master.h"
#include "ethercat/foe.h"
#include "ethercat/hal.h"

/* ========================================================================== */
/*                             Configuration                                  */
/* ========================================================================== */

#define NETWORK_INTERFACE   "eth0"
#define EXPECTED_SLAVES     1
#define SLAVE_ADDRESS       0x1001

/* ========================================================================== */
/*                             Global Variables                               */
/* ========================================================================== */

static volatile bool g_running = true;

/* ========================================================================== */
/*                             Signal Handler                                 */
/* ========================================================================== */

static void signal_handler(int signum)
{
    (void)signum;
    printf("\n\nReceived interrupt signal, shutting down...\n");
    g_running = false;
}

/* ========================================================================== */
/*                             Progress Callback                              */
/* ========================================================================== */

static void progress_callback(uint32_t bytes_transferred, uint32_t total_bytes, void* user_data)
{
    (void)user_data;

    if (total_bytes == 0) {
        return;
    }

    uint32_t percentage = (bytes_transferred * 100) / total_bytes;
    uint32_t bar_width = 50;
    uint32_t filled = (bytes_transferred * bar_width) / total_bytes;

    printf("\r[");
    for (uint32_t i = 0; i < bar_width; i++) {
        if (i < filled) {
            printf("=");
        } else if (i == filled) {
            printf(">");
        } else {
            printf(" ");
        }
    }
    printf("] %3u%% (%u / %u bytes)", percentage, bytes_transferred, total_bytes);
    fflush(stdout);

    if (bytes_transferred >= total_bytes) {
        printf("\n");
    }
}

/* ========================================================================== */
/*                             Demo Functions                                 */
/* ========================================================================== */

/**
 * @brief Demo 1: Check FoE support
 */
static bool demo_check_foe_support(void)
{
    printf("\n=== Demo 1: Check FoE Support ===\n");

    bool supported = false;
    foe_status_t status = foe_check_support(SLAVE_ADDRESS, &supported);

    if (status != FOE_STATUS_SUCCESS) {
        printf("Error: Failed to check FoE support: %s\n", foe_get_status_string(status));
        return false;
    }

    printf("FoE support: %s\n", supported ? "YES" : "NO");

    if (!supported) {
        printf("Warning: Slave does not support FoE protocol\n");
        return false;
    }

    return true;
}

/**
 * @brief Demo 2: Read file from slave
 */
static bool demo_read_file(void)
{
    printf("\n=== Demo 2: Read File from Slave ===\n");

    const char* filename = "config.bin";
    uint8_t buffer[65536];  /* 64KB buffer */
    uint32_t size = sizeof(buffer);

    printf("Reading file '%s' from slave 0x%04X...\n", filename, SLAVE_ADDRESS);

    foe_status_t status = foe_read(SLAVE_ADDRESS, filename, buffer, &size,
                                    5000, progress_callback, NULL);

    if (status != FOE_STATUS_SUCCESS) {
        printf("Error: Failed to read file: %s\n", foe_get_status_string(status));
        return false;
    }

    printf("Successfully read %u bytes\n", size);

    /* Display first 256 bytes in hex */
    printf("\nFirst 256 bytes (hex):\n");
    uint32_t display_size = (size < 256) ? size : 256;
    for (uint32_t i = 0; i < display_size; i++) {
        if (i % 16 == 0) {
            printf("%04X: ", i);
        }
        printf("%02X ", buffer[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
    if (display_size % 16 != 0) {
        printf("\n");
    }

    return true;
}

/**
 * @brief Demo 3: Write file to slave
 */
static bool demo_write_file(void)
{
    printf("\n=== Demo 3: Write File to Slave ===\n");

    const char* filename = "test.bin";

    /* Create test data (1KB) */
    uint8_t data[1024];
    for (uint32_t i = 0; i < sizeof(data); i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }

    printf("Writing file '%s' (%zu bytes) to slave 0x%04X...\n",
           filename, sizeof(data), SLAVE_ADDRESS);

    foe_status_t status = foe_write(SLAVE_ADDRESS, filename, data, sizeof(data),
                                     5000, progress_callback, NULL);

    if (status != FOE_STATUS_SUCCESS) {
        printf("Error: Failed to write file: %s\n", foe_get_status_string(status));
        return false;
    }

    printf("Successfully wrote %zu bytes\n", sizeof(data));

    return true;
}

/**
 * @brief Demo 4: Firmware update
 */
static bool demo_firmware_update(void) __attribute__((unused));
static bool demo_firmware_update(void)
{
    printf("\n=== Demo 4: Firmware Update ===\n");

    /* Create dummy firmware (16KB) */
    uint32_t firmware_size = 16384;
    uint8_t* firmware = malloc(firmware_size);
    if (!firmware) {
        printf("Error: Failed to allocate firmware buffer\n");
        return false;
    }

    /* Fill with pattern */
    for (uint32_t i = 0; i < firmware_size; i++) {
        firmware[i] = (uint8_t)((i * 7) & 0xFF);
    }

    printf("Updating firmware on slave 0x%04X (%u bytes)...\n",
           SLAVE_ADDRESS, firmware_size);
    printf("This will:\n");
    printf("  1. Transition slave to Bootstrap state\n");
    printf("  2. Transfer firmware file\n");
    printf("  3. Restart slave\n\n");

    foe_status_t status = foe_firmware_update(SLAVE_ADDRESS, firmware, firmware_size,
                                               progress_callback, NULL, 30000);

    free(firmware);

    if (status != FOE_STATUS_SUCCESS) {
        printf("Error: Firmware update failed: %s\n", foe_get_status_string(status));
        return false;
    }

    printf("Firmware update completed successfully\n");
    printf("Slave is restarting...\n");

    return true;
}

/**
 * @brief Demo 5: Large file transfer
 */
static bool demo_large_file_transfer(void)
{
    printf("\n=== Demo 5: Large File Transfer ===\n");

    const char* filename = "large_data.bin";
    uint32_t file_size = 131072;  /* 128KB */

    /* Allocate buffer */
    uint8_t* data = malloc(file_size);
    if (!data) {
        printf("Error: Failed to allocate buffer\n");
        return false;
    }

    /* Fill with pattern */
    for (uint32_t i = 0; i < file_size; i++) {
        data[i] = (uint8_t)((i * 13) & 0xFF);
    }

    printf("Writing large file '%s' (%u bytes) to slave 0x%04X...\n",
           filename, file_size, SLAVE_ADDRESS);

    uint64_t start_time = hal_get_time_ms();

    foe_status_t status = foe_write(SLAVE_ADDRESS, filename, data, file_size,
                                     30000, progress_callback, NULL);

    uint64_t end_time = hal_get_time_ms();
    uint64_t duration_ms = end_time - start_time;

    free(data);

    if (status != FOE_STATUS_SUCCESS) {
        printf("Error: Failed to write large file: %s\n", foe_get_status_string(status));
        return false;
    }

    printf("Successfully wrote %u bytes in %llu ms\n", file_size,
           (unsigned long long)duration_ms);

    if (duration_ms > 0) {
        double throughput_kbps = (file_size * 8.0) / duration_ms;
        printf("Throughput: %.2f Kbps\n", throughput_kbps);
    }

    return true;
}

/**
 * @brief Demo 6: Error handling
 */
static bool demo_error_handling(void)
{
    printf("\n=== Demo 6: Error Handling ===\n");

    /* Try to read non-existent file */
    const char* filename = "nonexistent.bin";
    uint8_t buffer[1024];
    uint32_t size = sizeof(buffer);

    printf("Attempting to read non-existent file '%s'...\n", filename);

    foe_status_t status = foe_read(SLAVE_ADDRESS, filename, buffer, &size,
                                    5000, NULL, NULL);

    if (status != FOE_STATUS_SUCCESS) {
        printf("Expected error occurred: %s\n", foe_get_status_string(status));
        printf("This demonstrates proper error handling\n");
        return true;  /* Expected failure */
    }

    printf("Unexpected: File read succeeded\n");
    return false;
}

/* ========================================================================== */
/*                             Main Function                                  */
/* ========================================================================== */

int main(int argc, char* argv[])
{
    printf("=======================================================\n");
    printf("  EtherCAT FoE (File over EtherCAT) Demo\n");
    printf("=======================================================\n");
    printf("\n");

    /* Parse command line arguments */
    const char* interface = NETWORK_INTERFACE;
    if (argc > 1) {
        interface = argv[1];
    }

    printf("Configuration:\n");
    printf("  Network Interface: %s\n", interface);
    printf("  Expected Slaves:   %d\n", EXPECTED_SLAVES);
    printf("  Slave Address:     0x%04X\n", SLAVE_ADDRESS);
    printf("\n");

    /* Setup signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize master */
    printf("Initializing EtherCAT master...\n");
    master_config_t config = {
        .interface_name = interface,
        .cycle_time_us = 1000,
        .enable_dc = false,
        .auto_configure = true
    };

    master_status_t status = master_init(&config);
    if (status != MASTER_STATUS_SUCCESS) {
        printf("Error: Failed to initialize master\n");
        return 1;
    }

    printf("Master initialized successfully\n");

    /* Scan network */
    printf("\nScanning EtherCAT network...\n");
    status = master_scan_network();
    if (status != MASTER_STATUS_SUCCESS) {
        printf("Error: Failed to scan network\n");
        master_shutdown();
        return 1;
    }

    uint16_t slave_count = 0;
    status = master_get_slave_count(&slave_count);
    if (status != MASTER_STATUS_SUCCESS) {
        printf("Error: Failed to get slave count\n");
        master_shutdown();
        return 1;
    }

    printf("Found %u slave(s)\n", slave_count);

    if (slave_count == 0) {
        printf("Error: No slaves found\n");
        master_shutdown();
        return 1;
    }

    /* Configure slaves */
    printf("\nConfiguring slaves...\n");
    status = master_configure_slaves();
    if (status != MASTER_STATUS_SUCCESS) {
        printf("Error: Failed to configure slaves\n");
        master_shutdown();
        return 1;
    }

    printf("Slaves configured successfully\n");

    /* Transition to Pre-Operational state */
    printf("\nTransitioning to Pre-Operational state...\n");
    status = master_request_state(MASTER_STATE_PREOP);
    if (status != MASTER_STATUS_SUCCESS) {
        printf("Error: Failed to transition to Pre-Op state\n");
        master_shutdown();
        return 1;
    }

    printf("All slaves in Pre-Operational state\n");

    /* Run demos */
    printf("\n=======================================================\n");
    printf("  Running FoE Demos\n");
    printf("=======================================================\n");

    bool success = true;

    /* Demo 1: Check FoE support */
    if (g_running && success) {
        success = demo_check_foe_support();
    }

    /* Demo 2: Read file (may fail if slave doesn't have the file) */
    if (g_running && success) {
        printf("\nNote: Demo 2 may fail if slave doesn't have 'config.bin'\n");
        demo_read_file();  /* Don't fail on error */
    }

    /* Demo 3: Write file */
    if (g_running && success) {
        success = demo_write_file();
    }

    /* Demo 4: Firmware update (commented out by default - requires real slave) */
    if (g_running && success) {
        printf("\n=== Demo 4: Firmware Update (SKIPPED) ===\n");
        printf("Firmware update demo is skipped by default.\n");
        printf("Uncomment in code to test with real hardware.\n");
        // success = demo_firmware_update();
    }

    /* Demo 5: Large file transfer */
    if (g_running && success) {
        success = demo_large_file_transfer();
    }

    /* Demo 6: Error handling */
    if (g_running && success) {
        success = demo_error_handling();
    }

    /* Summary */
    printf("\n=======================================================\n");
    printf("  Demo Summary\n");
    printf("=======================================================\n");
    printf("Overall result: %s\n", success ? "SUCCESS" : "FAILED");

    /* Cleanup */
    printf("\nShutting down master...\n");
    master_shutdown();

    printf("Demo completed\n");

    return success ? 0 : 1;
}
