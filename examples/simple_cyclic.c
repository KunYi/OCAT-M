/**
 * @file simple_cyclic.c
 * @brief Simple EtherCAT Cyclic I/O Example
 * @version 1.0.0
 * @date 2026-01-03
 *
 * This example demonstrates basic cyclic I/O operation with EtherCAT slaves.
 * It performs the following steps:
 * 1. Initialize the EtherCAT master
 * 2. Scan the network for slaves
 * 3. Configure all slaves
 * 4. Allocate process data
 * 5. Transition to OPERATIONAL state
 * 6. Run cyclic I/O loop
 * 7. Clean shutdown
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "ethercat/master.h"
#include "ethercat/process_data.h"

/* Configuration */
#define NETWORK_INTERFACE "eth0"
#define CYCLE_TIME_US     1000      /* 1ms cycle time */
#define CYCLE_FREQUENCY   1000      /* 1kHz */
#define RUN_DURATION_SEC  10        /* Run for 10 seconds */

/* Global flag for clean shutdown */
static volatile sig_atomic_t g_running = 1;

/**
 * @brief Signal handler for clean shutdown
 */
static void signal_handler(int signum)
{
    (void)signum;
    printf("\nShutdown signal received...\n");
    g_running = 0;
}

/**
 * @brief Sleep for microseconds
 */
static void sleep_us(uint32_t us)
{
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

/**
 * @brief Print slave information
 */
static void print_slave_info(uint16_t position, const slave_info_t* info)
{
    printf("  Slave %u:\n", position);
    printf("    Station Address: 0x%04X\n", info->station_address);
    printf("    Alias Address:   0x%04X\n", info->alias_address);
    printf("    Vendor ID:       0x%08X\n", info->vendor_id);
    printf("    Product Code:    0x%08X\n", info->product_code);
    printf("    Revision:        0x%08X\n", info->revision_number);
    printf("    Serial:          0x%08X\n", info->serial_number);
    printf("    Name:            %s\n", info->name);
    printf("    Ports:           %u\n", info->num_ports);
    printf("    Mailbox:         %s\n", info->has_mailbox ? "Yes" : "No");
    printf("    CoE:             %s\n", info->has_coe ? "Yes" : "No");
}

/**
 * @brief Print network topology
 */
static void print_topology(const network_topology_t* topology)
{
    printf("\nNetwork Topology:\n");
    printf("  Slave Count:     %u\n", topology->slave_count);
    printf("  Expected WKC:    %u\n", topology->working_counter_expected);
    printf("  Cycle Time:      %u us\n", topology->cycle_time_us);
    printf("  Topology Valid:  %s\n", topology->topology_valid ? "Yes" : "No");
    printf("  DC Available:    %s\n", topology->dc_available ? "Yes" : "No");
}

/**
 * @brief Print cyclic statistics
 */
static void print_statistics(const pd_statistics_t* stats)
{
    printf("\nCyclic Operation Statistics:\n");
    printf("  Total Cycles:    %lu\n", stats->cycle_count);
    printf("  WKC Errors:      %lu\n", stats->wkc_error_count);
    printf("  Timeouts:        %lu\n", stats->timeout_count);
    printf("  Min Cycle Time:  %u us\n", stats->min_cycle_time_us);
    printf("  Max Cycle Time:  %u us\n", stats->max_cycle_time_us);
    printf("  Avg Cycle Time:  %u us\n", stats->avg_cycle_time_us);
    printf("  Last WKC:        %u\n", stats->last_working_counter);
}

/**
 * @brief Main function
 */
int main(int argc, char* argv[])
{
    master_status_t status;
    uint16_t slave_count = 0;
    pd_image_t* pd_image = NULL;
    uint64_t cycle_counter = 0;
    uint64_t start_time, end_time;

    printf("=================================================\n");
    printf("EtherCAT Simple Cyclic I/O Example\n");
    printf("=================================================\n\n");

    /* Parse command line arguments */
    const char* interface = NETWORK_INTERFACE;
    if (argc > 1) {
        interface = argv[1];
    }

    printf("Network Interface: %s\n", interface);
    printf("Cycle Time:        %u us (%u Hz)\n", CYCLE_TIME_US, CYCLE_FREQUENCY);
    printf("Run Duration:      %u seconds\n\n", RUN_DURATION_SEC);

    /* Install signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Step 1: Initialize EtherCAT Master */
    printf("Step 1: Initializing EtherCAT Master...\n");

    master_config_t config = {
        .interface_name = interface,
        .mac_address = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  /* Will be auto-detected */
        .cycle_time_us = CYCLE_TIME_US,
        .scan_timeout_ms = 1000,
        .state_change_timeout_ms = 1000,
        .enable_dc = false,
        .auto_configure = true
    };

    status = master_init(&config);
    if (status != MASTER_STATUS_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to initialize master (status=%d)\n", status);
        return 1;
    }
    printf("  Master initialized successfully\n\n");

    /* Step 2: Scan Network */
    printf("Step 2: Scanning EtherCAT network...\n");

    status = master_scan_network();
    if (status == MASTER_STATUS_NO_SLAVES) {
        printf("  WARNING: No slaves found on network\n");
        printf("  Continuing with zero slaves for demonstration...\n\n");
        slave_count = 0;
    } else if (status != MASTER_STATUS_SUCCESS) {
        fprintf(stderr, "ERROR: Network scan failed (status=%d)\n", status);
        master_shutdown();
        return 1;
    } else {
        status = master_get_slave_count(&slave_count);
        printf("  Found %u slave(s)\n\n", slave_count);

        /* Print slave information */
        if (slave_count > 0) {
            printf("Slave Information:\n");
            for (uint16_t i = 0; i < slave_count; i++) {
                slave_info_t info;
                status = master_get_slave_info(i, &info);
                if (status == MASTER_STATUS_SUCCESS) {
                    print_slave_info(i, &info);
                }
            }
            printf("\n");

            /* Print topology */
            network_topology_t topology;
            status = master_get_topology(&topology);
            if (status == MASTER_STATUS_SUCCESS) {
                print_topology(&topology);
            }
            printf("\n");
        }
    }

    /* Step 3: Configure Slaves */
    if (slave_count > 0) {
        printf("Step 3: Configuring slaves...\n");

        status = master_configure_slaves();
        if (status != MASTER_STATUS_SUCCESS) {
            fprintf(stderr, "ERROR: Slave configuration failed (status=%d)\n", status);
            master_shutdown();
            return 1;
        }
        printf("  Slaves configured successfully\n\n");
    } else {
        printf("Step 3: Skipping slave configuration (no slaves)\n\n");
    }

    /* Step 4: Allocate Process Data */
    printf("Step 4: Allocating process data...\n");

    status = master_allocate_process_data(NULL);  /* No redundancy */
    if (status != MASTER_STATUS_SUCCESS) {
        fprintf(stderr, "ERROR: Process data allocation failed (status=%d)\n", status);
        master_shutdown();
        return 1;
    }

    status = master_get_process_data_image(&pd_image);
    if (status != MASTER_STATUS_SUCCESS || pd_image == NULL) {
        fprintf(stderr, "ERROR: Failed to get process data image (status=%d)\n", status);
        master_shutdown();
        return 1;
    }

    printf("  Process data allocated:\n");
    printf("    Input size:  %u bytes\n", pd_image->input_size);
    printf("    Output size: %u bytes\n\n", pd_image->output_size);

    /* Step 5: Transition to OPERATIONAL State */
    printf("Step 5: Transitioning to OPERATIONAL state...\n");

    /* PREOP -> SAFEOP */
    status = master_request_state(0x04);  /* AL_STATE_SAFEOP */
    if (status != MASTER_STATUS_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to transition to SAFEOP (status=%d)\n", status);
        master_shutdown();
        return 1;
    }
    printf("  Transitioned to SAFEOP\n");

    /* Start cyclic operation */
    status = master_start_cyclic();
    if (status != MASTER_STATUS_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to start cyclic operation (status=%d)\n", status);
        master_shutdown();
        return 1;
    }

    /* SAFEOP -> OP */
    status = master_request_state(0x08);  /* AL_STATE_OP */
    if (status != MASTER_STATUS_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to transition to OP (status=%d)\n", status);
        master_shutdown();
        return 1;
    }
    printf("  Transitioned to OPERATIONAL\n\n");

    /* Step 6: Run Cyclic I/O Loop */
    printf("Step 6: Running cyclic I/O loop...\n");
    printf("  Press Ctrl+C to stop\n\n");

    start_time = get_time_us();
    uint64_t next_cycle = start_time;
    uint64_t print_time = start_time;

    while (g_running) {
        uint64_t current_time = get_time_us();

        /* Check if it's time for next cycle */
        if (current_time >= next_cycle) {
            /* Process one cycle */
            status = master_process_cycle();
            if (status != MASTER_STATUS_SUCCESS) {
                fprintf(stderr, "WARNING: Cycle processing failed (status=%d)\n", status);
            }

            /* Example: Write output data (toggle pattern) */
            if (pd_image->output_size > 0) {
                uint8_t output_data = (cycle_counter & 0xFF);
                master_write_slave_output(0, &output_data, 1);
            }

            /* Example: Read input data */
            if (pd_image->input_size > 0) {
                uint8_t input_data = 0;
                master_read_slave_input(0, &input_data, 1);
                /* Process input data here */
            }

            cycle_counter++;
            next_cycle += CYCLE_TIME_US;

            /* Print status every second */
            if (current_time - print_time >= 1000000) {
                pd_statistics_t stats;
                master_get_cyclic_statistics(&stats);
                printf("  Cycles: %lu, WKC Errors: %lu, Avg Cycle Time: %u us\n",
                       stats.cycle_count, stats.wkc_error_count, stats.avg_cycle_time_us);
                print_time = current_time;
            }
        }

        /* Check run duration */
        end_time = get_time_us();
        if ((end_time - start_time) >= (RUN_DURATION_SEC * 1000000ULL)) {
            break;
        }

        /* Sleep until next cycle */
        if (next_cycle > current_time) {
            sleep_us(next_cycle - current_time);
        }
    }

    printf("\n");

    /* Print final statistics */
    pd_statistics_t final_stats;
    status = master_get_cyclic_statistics(&final_stats);
    if (status == MASTER_STATUS_SUCCESS) {
        print_statistics(&final_stats);
    }

    /* Step 7: Clean Shutdown */
    printf("\nStep 7: Shutting down...\n");

    /* Stop cyclic operation */
    master_stop_cyclic();
    printf("  Cyclic operation stopped\n");

    /* Transition to INIT */
    master_request_state(0x01);  /* AL_STATE_INIT */
    printf("  Transitioned to INIT\n");

    /* Shutdown master */
    master_shutdown();
    printf("  Master shutdown complete\n");

    printf("\n=================================================\n");
    printf("Example completed successfully\n");
    printf("=================================================\n");

    return 0;
}
