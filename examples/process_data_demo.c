/**
 * @file process_data_demo.c
 * @brief EtherCAT Process Data Demonstration
 * @version 1.0.0
 * @date 2026-01-03
 *
 * This example demonstrates advanced process data operations:
 * 1. Direct process data image access
 * 2. Per-slave data read/write
 * 3. Working counter monitoring
 * 4. Cycle time statistics
 * 5. Error handling and recovery
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
#define STATS_INTERVAL_MS 1000      /* Print stats every 1 second */
#define MAX_CYCLES        10000     /* Run for 10000 cycles (10 seconds at 1kHz) */

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
 * @brief Print process data image contents
 */
static void print_pd_image(const pd_image_t* image)
{
    printf("\nProcess Data Image:\n");
    printf("  Logical Address: 0x%08X\n", image->logical_address);
    printf("  Input Size:      %u bytes\n", image->input_size);
    printf("  Output Size:     %u bytes\n", image->output_size);
    printf("  Frame Index:     %u\n", image->frame_index);
    printf("  Current Port:    %u\n", image->current_port);

    /* Print first 16 bytes of output data */
    if (image->output_size > 0) {
        printf("\n  Output Data (first 16 bytes):\n    ");
        uint32_t print_size = (image->output_size < 16) ? image->output_size : 16;
        for (uint32_t i = 0; i < print_size; i++) {
            printf("%02X ", image->output_data[i]);
            if ((i + 1) % 8 == 0) printf("\n    ");
        }
        printf("\n");
    }

    /* Print first 16 bytes of input data */
    if (image->input_size > 0) {
        printf("  Input Data (first 16 bytes):\n    ");
        uint32_t print_size = (image->input_size < 16) ? image->input_size : 16;
        for (uint32_t i = 0; i < print_size; i++) {
            printf("%02X ", image->input_data[i]);
            if ((i + 1) % 8 == 0) printf("\n    ");
        }
        printf("\n");
    }
}

/**
 * @brief Print detailed statistics
 */
static void print_detailed_stats(const pd_statistics_t* stats)
{
    printf("\n========================================\n");
    printf("Process Data Statistics\n");
    printf("========================================\n");
    printf("Cycle Statistics:\n");
    printf("  Total Cycles:       %lu\n", stats->cycle_count);
    printf("  WKC Errors:         %lu (%.2f%%)\n",
           stats->wkc_error_count,
           stats->cycle_count > 0 ? (100.0 * stats->wkc_error_count / stats->cycle_count) : 0.0);
    printf("  Timeouts:           %lu (%.2f%%)\n",
           stats->timeout_count,
           stats->cycle_count > 0 ? (100.0 * stats->timeout_count / stats->cycle_count) : 0.0);

    printf("\nTiming Statistics:\n");
    printf("  Min Cycle Time:     %u us\n", stats->min_cycle_time_us);
    printf("  Max Cycle Time:     %u us\n", stats->max_cycle_time_us);
    printf("  Avg Cycle Time:     %u us\n", stats->avg_cycle_time_us);
    printf("  Jitter:             %u us\n",
           stats->max_cycle_time_us - stats->min_cycle_time_us);

    printf("\nWorking Counter:\n");
    printf("  Last WKC:           %u\n", stats->last_working_counter);
    printf("  Expected WKC:       %u\n", stats->expected_working_counter);

    printf("========================================\n");
}

/**
 * @brief Process slave outputs (application logic)
 */
static void process_slave_outputs(uint16_t slave_count, uint64_t cycle_counter)
{
    /* Example: Generate different patterns for each slave */
    for (uint16_t i = 0; i < slave_count; i++) {
        uint8_t output_data[8];

        /* Generate pattern based on cycle counter and slave position */
        switch (i % 3) {
            case 0:
                /* Counter pattern */
                output_data[0] = (cycle_counter & 0xFF);
                output_data[1] = ((cycle_counter >> 8) & 0xFF);
                break;

            case 1:
                /* Toggle pattern */
                output_data[0] = (cycle_counter & 0x01) ? 0xFF : 0x00;
                output_data[1] = (cycle_counter & 0x02) ? 0xFF : 0x00;
                break;

            case 2:
                /* Sine wave pattern (simplified) */
                output_data[0] = (uint8_t)(128 + 127 * (cycle_counter % 100) / 100);
                output_data[1] = (uint8_t)(128 - 127 * (cycle_counter % 100) / 100);
                break;
        }

        /* Write to slave output */
        master_write_slave_output(i, output_data, 2);
    }
}

/**
 * @brief Process slave inputs (application logic)
 */
static void process_slave_inputs(uint16_t slave_count, uint64_t cycle_counter)
{
    /* Example: Read and process input data from each slave */
    for (uint16_t i = 0; i < slave_count; i++) {
        uint8_t input_data[8];

        /* Read from slave input */
        master_status_t status = master_read_slave_input(i, input_data, 2);
        if (status == MASTER_STATUS_SUCCESS) {
            /* Process input data here */
            /* For demonstration, we just check for specific patterns */
            if (cycle_counter % 1000 == 0) {
                printf("  Slave %u input: 0x%02X 0x%02X\n", i, input_data[0], input_data[1]);
            }
        }
    }
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
    uint64_t start_time, current_time, last_stats_time;
    uint64_t next_cycle;

    printf("=================================================\n");
    printf("EtherCAT Process Data Demonstration\n");
    printf("=================================================\n\n");

    /* Parse command line arguments */
    const char* interface = NETWORK_INTERFACE;
    if (argc > 1) {
        interface = argv[1];
    }

    printf("Configuration:\n");
    printf("  Network Interface: %s\n", interface);
    printf("  Cycle Time:        %u us\n", CYCLE_TIME_US);
    printf("  Max Cycles:        %u\n", MAX_CYCLES);
    printf("  Stats Interval:    %u ms\n\n", STATS_INTERVAL_MS);

    /* Install signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize Master */
    printf("Initializing EtherCAT Master...\n");

    master_config_t config = {
        .interface_name = interface,
        .mac_address = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        .cycle_time_us = CYCLE_TIME_US,
        .scan_timeout_ms = 1000,
        .state_change_timeout_ms = 1000,
        .enable_dc = false,
        .auto_configure = true
    };

    status = master_init(&config);
    if (status != MASTER_STATUS_SUCCESS) {
        fprintf(stderr, "ERROR: Master initialization failed (status=%d)\n", status);
        return 1;
    }
    printf("  Master initialized\n\n");

    /* Scan Network */
    printf("Scanning network...\n");
    status = master_scan_network();
    if (status == MASTER_STATUS_NO_SLAVES) {
        printf("  WARNING: No slaves found\n");
        slave_count = 0;
    } else if (status != MASTER_STATUS_SUCCESS) {
        fprintf(stderr, "ERROR: Network scan failed (status=%d)\n", status);
        master_shutdown();
        return 1;
    } else {
        master_get_slave_count(&slave_count);
        printf("  Found %u slave(s)\n", slave_count);
    }
    printf("\n");

    /* Configure Slaves */
    if (slave_count > 0) {
        printf("Configuring slaves...\n");
        status = master_configure_slaves();
        if (status != MASTER_STATUS_SUCCESS) {
            fprintf(stderr, "ERROR: Configuration failed (status=%d)\n", status);
            master_shutdown();
            return 1;
        }
        printf("  Configuration complete\n\n");
    }

    /* Allocate Process Data */
    printf("Allocating process data...\n");
    status = master_allocate_process_data(NULL);
    if (status != MASTER_STATUS_SUCCESS) {
        fprintf(stderr, "ERROR: Process data allocation failed (status=%d)\n", status);
        master_shutdown();
        return 1;
    }

    status = master_get_process_data_image(&pd_image);
    if (status != MASTER_STATUS_SUCCESS || pd_image == NULL) {
        fprintf(stderr, "ERROR: Failed to get process data image\n");
        master_shutdown();
        return 1;
    }

    print_pd_image(pd_image);

    /* Transition to OPERATIONAL */
    printf("\nTransitioning to OPERATIONAL...\n");

    master_request_state(0x04);  /* SAFEOP */
    printf("  -> SAFEOP\n");

    master_start_cyclic();

    master_request_state(0x08);  /* OP */
    printf("  -> OPERATIONAL\n\n");

    /* Run Cyclic Loop */
    printf("Starting cyclic operation...\n");
    printf("Press Ctrl+C to stop\n\n");

    start_time = get_time_us();
    last_stats_time = start_time;
    next_cycle = start_time;

    while (g_running && cycle_counter < MAX_CYCLES) {
        current_time = get_time_us();

        /* Wait for next cycle */
        if (current_time >= next_cycle) {
            /* Process cycle */
            status = master_process_cycle();
            if (status != MASTER_STATUS_SUCCESS) {
                fprintf(stderr, "WARNING: Cycle %lu failed (status=%d)\n",
                        cycle_counter, status);
            }

            /* Application logic: Write outputs */
            if (slave_count > 0) {
                process_slave_outputs(slave_count, cycle_counter);
            }

            /* Application logic: Read inputs */
            if (slave_count > 0) {
                process_slave_inputs(slave_count, cycle_counter);
            }

            cycle_counter++;
            next_cycle += CYCLE_TIME_US;

            /* Print statistics periodically */
            if (current_time - last_stats_time >= (STATS_INTERVAL_MS * 1000)) {
                pd_statistics_t stats;
                master_get_cyclic_statistics(&stats);

                printf("Cycle %lu: WKC=%u, Errors=%lu, Avg Time=%u us\n",
                       stats.cycle_count,
                       stats.last_working_counter,
                       stats.wkc_error_count,
                       stats.avg_cycle_time_us);

                last_stats_time = current_time;
            }
        }

        /* Sleep until next cycle */
        current_time = get_time_us();
        if (next_cycle > current_time) {
            sleep_us(next_cycle - current_time);
        }
    }

    printf("\nCyclic operation stopped\n");

    /* Print final statistics */
    pd_statistics_t final_stats;
    status = master_get_cyclic_statistics(&final_stats);
    if (status == MASTER_STATUS_SUCCESS) {
        print_detailed_stats(&final_stats);
    }

    /* Print final process data image */
    print_pd_image(pd_image);

    /* Shutdown */
    printf("\nShutting down...\n");
    master_stop_cyclic();
    master_request_state(0x01);  /* INIT */
    master_shutdown();
    printf("Shutdown complete\n");

    printf("\n=================================================\n");
    printf("Demonstration completed successfully\n");
    printf("=================================================\n");

    return 0;
}
