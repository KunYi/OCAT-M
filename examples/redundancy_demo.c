/**
 * @file redundancy_demo.c
 * @brief EtherCAT Redundancy Demonstration
 * @version 1.0.0
 * @date 2026-01-04
 *
 * This example demonstrates EtherCAT redundancy features including:
 * - Dual-port initialization
 * - Port health monitoring
 * - Automatic failover
 * - Port switching
 * - Redundant process data exchange
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include "ethercat/master.h"
#include "ethercat/process_data.h"
#include "ethercat/hal.h"
#include "ethercat/al.h"

/* ========================================================================== */
/* Configuration                                                              */
/* ========================================================================== */

#define CYCLE_TIME_US           1000    /* 1ms cycle time (1kHz) */
#define SLAVE_COUNT             2       /* Number of slaves */
#define INPUT_SIZE              4       /* Input data size per slave */
#define OUTPUT_SIZE             4       /* Output data size per slave */
#define HEALTH_CHECK_INTERVAL   100     /* Check port health every 100 cycles */
#define FAILOVER_THRESHOLD      3       /* Failover after 3 consecutive errors */

/* ========================================================================== */
/* Global Variables                                                           */
/* ========================================================================== */

static volatile bool g_running = true;
static pd_image_t g_pd_image = {0};

/* Redundancy state */
typedef struct {
    pd_port_select_t active_port;
    uint32_t primary_error_count;
    uint32_t secondary_error_count;
    uint32_t primary_success_count;
    uint32_t secondary_success_count;
    uint32_t failover_count;
    bool auto_failover_enabled;
} redundancy_state_t;

static redundancy_state_t g_redundancy = {
    .active_port = PD_PORT_PRIMARY,
    .primary_error_count = 0,
    .secondary_error_count = 0,
    .primary_success_count = 0,
    .secondary_success_count = 0,
    .failover_count = 0,
    .auto_failover_enabled = true
};

/* ========================================================================== */
/* Signal Handler                                                             */
/* ========================================================================== */

static void signal_handler(int signum)
{
    (void)signum;
    printf("\n[INFO] Received signal, shutting down...\n");
    g_running = false;
}

/* ========================================================================== */
/* Port Health Monitoring                                                     */
/* ========================================================================== */

static void check_port_health(void)
{
    bool primary_healthy = false;
    bool secondary_healthy = false;

    /* Check primary port health */
    pd_status_t status = pd_check_port_health(&g_pd_image, PD_PORT_PRIMARY, &primary_healthy);
    if (status != PD_STATUS_SUCCESS) {
        printf("[WARN] Failed to check primary port health\n");
    }

    /* Check secondary port health */
    status = pd_check_port_health(&g_pd_image, PD_PORT_SECONDARY, &secondary_healthy);
    if (status != PD_STATUS_SUCCESS) {
        printf("[WARN] Failed to check secondary port health\n");
    }

    /* Print health status */
    printf("[HEALTH] Primary: %s, Secondary: %s\n",
           primary_healthy ? "HEALTHY" : "UNHEALTHY",
           secondary_healthy ? "HEALTHY" : "UNHEALTHY");

    /* Get port statistics */
    pd_port_status_t primary_status, secondary_status;
    pd_get_port_status(&g_pd_image, PD_PORT_PRIMARY, &primary_status);
    pd_get_port_status(&g_pd_image, PD_PORT_SECONDARY, &secondary_status);

    printf("[STATS] Primary - Sent: %lu, Recv: %lu, Errors: %lu, WKC: %u\n",
           primary_status.frames_sent,
           primary_status.frames_received,
           primary_status.errors,
           primary_status.last_wkc);

    printf("[STATS] Secondary - Sent: %lu, Recv: %lu, Errors: %lu, WKC: %u\n",
           secondary_status.frames_sent,
           secondary_status.frames_received,
           secondary_status.errors,
           secondary_status.last_wkc);
}

/* ========================================================================== */
/* Automatic Failover Logic                                                  */
/* ========================================================================== */

static void handle_failover(void)
{
    if (!g_redundancy.auto_failover_enabled) {
        return;
    }

    pd_port_select_t current_port = g_redundancy.active_port;
    pd_port_select_t backup_port = (current_port == PD_PORT_PRIMARY) ?
                                    PD_PORT_SECONDARY : PD_PORT_PRIMARY;

    /* Check if current port has too many consecutive errors */
    uint32_t current_errors = (current_port == PD_PORT_PRIMARY) ?
                              g_redundancy.primary_error_count :
                              g_redundancy.secondary_error_count;

    if (current_errors >= FAILOVER_THRESHOLD) {
        /* Check if backup port is healthy */
        bool backup_healthy = false;
        pd_status_t status = pd_check_port_health(&g_pd_image, backup_port, &backup_healthy);

        if (status == PD_STATUS_SUCCESS && backup_healthy) {
            printf("\n[FAILOVER] Switching from %s to %s port (errors: %u)\n",
                   (current_port == PD_PORT_PRIMARY) ? "PRIMARY" : "SECONDARY",
                   (backup_port == PD_PORT_PRIMARY) ? "PRIMARY" : "SECONDARY",
                   current_errors);

            /* Perform port switch */
            status = pd_switch_port(&g_pd_image, backup_port);
            if (status == PD_STATUS_SUCCESS) {
                g_redundancy.active_port = backup_port;
                g_redundancy.failover_count++;

                /* Reset error counters */
                if (current_port == PD_PORT_PRIMARY) {
                    g_redundancy.primary_error_count = 0;
                } else {
                    g_redundancy.secondary_error_count = 0;
                }

                printf("[FAILOVER] Successfully switched to %s port (total failovers: %u)\n",
                       (backup_port == PD_PORT_PRIMARY) ? "PRIMARY" : "SECONDARY",
                       g_redundancy.failover_count);
            } else {
                printf("[ERROR] Failed to switch port: %d\n", status);
            }
        } else {
            printf("[WARN] Backup port is not healthy, cannot failover\n");
        }
    }
}

/* ========================================================================== */
/* Redundant Process Data Exchange                                           */
/* ========================================================================== */

static bool exchange_process_data_redundant(uint16_t expected_wkc)
{
    uint16_t wkc = 0;
    pd_status_t status;

    /* Try active port first */
    status = pd_exchange_port(&g_pd_image, g_redundancy.active_port, &wkc, 100);

    if (status == PD_STATUS_SUCCESS && wkc == expected_wkc) {
        /* Success on active port */
        if (g_redundancy.active_port == PD_PORT_PRIMARY) {
            g_redundancy.primary_success_count++;
            g_redundancy.primary_error_count = 0;
        } else {
            g_redundancy.secondary_success_count++;
            g_redundancy.secondary_error_count = 0;
        }
        return true;
    }

    /* Error on active port */
    if (g_redundancy.active_port == PD_PORT_PRIMARY) {
        g_redundancy.primary_error_count++;
    } else {
        g_redundancy.secondary_error_count++;
    }

    printf("[WARN] Exchange failed on %s port (status: %d, wkc: %u/%u)\n",
           (g_redundancy.active_port == PD_PORT_PRIMARY) ? "PRIMARY" : "SECONDARY",
           status, wkc, expected_wkc);

    /* Try backup port */
    pd_port_select_t backup_port = (g_redundancy.active_port == PD_PORT_PRIMARY) ?
                                    PD_PORT_SECONDARY : PD_PORT_PRIMARY;

    status = pd_exchange_port(&g_pd_image, backup_port, &wkc, 100);

    if (status == PD_STATUS_SUCCESS && wkc == expected_wkc) {
        printf("[INFO] Exchange succeeded on backup %s port\n",
               (backup_port == PD_PORT_PRIMARY) ? "PRIMARY" : "SECONDARY");

        if (backup_port == PD_PORT_PRIMARY) {
            g_redundancy.primary_success_count++;
        } else {
            g_redundancy.secondary_success_count++;
        }
        return true;
    }

    /* Both ports failed */
    printf("[ERROR] Exchange failed on both ports!\n");
    return false;
}

/* ========================================================================== */
/* Main Function                                                              */
/* ========================================================================== */

int main(int argc, char* argv[])
{
    printf("=============================================================\n");
    printf("EtherCAT Redundancy Demonstration\n");
    printf("=============================================================\n\n");

    /* Parse command line arguments */
    const char* primary_interface = (argc > 1) ? argv[1] : "eth0";
    const char* secondary_interface = (argc > 2) ? argv[2] : "eth1";

    printf("[CONFIG] Primary interface: %s\n", primary_interface);
    printf("[CONFIG] Secondary interface: %s\n", secondary_interface);
    printf("[CONFIG] Cycle time: %d us (%d Hz)\n", CYCLE_TIME_US, 1000000 / CYCLE_TIME_US);
    printf("[CONFIG] Slave count: %d\n", SLAVE_COUNT);
    printf("[CONFIG] Auto-failover: %s\n", g_redundancy.auto_failover_enabled ? "ENABLED" : "DISABLED");
    printf("[CONFIG] Failover threshold: %d errors\n\n", FAILOVER_THRESHOLD);

    /* Install signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Step 1: Initialize HAL with dual ports */
    printf("[STEP 1] Initializing HAL with dual ports...\n");

    hal_config_t primary_config;
    hal_config_init_defaults(&primary_config);
    primary_config.platform = HAL_PLATFORM_LINUX_RAW_SOCKET;
    primary_config.interface_name = primary_interface;
    primary_config.promiscuous_mode = true;

    hal_config_t secondary_config;
    hal_config_init_defaults(&secondary_config);
    secondary_config.platform = HAL_PLATFORM_LINUX_RAW_SOCKET;
    secondary_config.interface_name = secondary_interface;
    secondary_config.promiscuous_mode = true;

    hal_status_t hal_status = hal_init_multiport(&primary_config, &secondary_config);
    if (hal_status != HAL_STATUS_SUCCESS) {
        printf("[ERROR] Failed to initialize HAL: %d\n", hal_status);
        printf("[INFO] Note: This demo requires root privileges and two network interfaces\n");
        return 1;
    }

    printf("[OK] HAL initialized with %d ports\n", hal_get_port_count());

    /* Check link status */
    bool primary_link = hal_is_port_link_up(0);
    bool secondary_link = hal_is_port_link_up(1);
    printf("[INFO] Primary port link: %s\n", primary_link ? "UP" : "DOWN");
    printf("[INFO] Secondary port link: %s\n\n", secondary_link ? "UP" : "DOWN");

    /* Step 2: Initialize Process Data */
    printf("[STEP 2] Initializing Process Data...\n");

    pd_status_t pd_status = pd_init();
    if (pd_status != PD_STATUS_SUCCESS) {
        printf("[ERROR] Failed to initialize Process Data: %d\n", pd_status);
        hal_shutdown();
        return 1;
    }

    /* Configure redundancy */
    pd_redundancy_config_t redundancy_config = {
        .mode = PD_REDUNDANCY_CABLE,
        .active_port = PD_PORT_PRIMARY,
        .auto_switch = true,
        .switch_threshold_ms = 100
    };

    /* Allocate process data image */
    uint32_t total_input_size = SLAVE_COUNT * INPUT_SIZE;
    uint32_t total_output_size = SLAVE_COUNT * OUTPUT_SIZE;

    pd_status = pd_allocate_image(&g_pd_image, total_input_size, total_output_size, &redundancy_config);
    if (pd_status != PD_STATUS_SUCCESS) {
        printf("[ERROR] Failed to allocate process data image: %d\n", pd_status);
        pd_shutdown();
        hal_shutdown();
        return 1;
    }

    printf("[OK] Process data image allocated (input: %u bytes, output: %u bytes)\n",
           total_input_size, total_output_size);
    printf("[OK] Redundancy mode: CABLE, Active port: PRIMARY\n\n");

    /* Step 3: Map slaves */
    printf("[STEP 3] Mapping slaves...\n");

    for (uint16_t i = 0; i < SLAVE_COUNT; i++) {
        pd_slave_mapping_t mapping = {
            .station_address = 0x1000 + i,
            .input_offset = i * INPUT_SIZE,
            .input_size = INPUT_SIZE,
            .output_offset = i * OUTPUT_SIZE,
            .output_size = OUTPUT_SIZE
        };

        pd_status = pd_map_slave(1, &mapping, &g_pd_image);
        if (pd_status != PD_STATUS_SUCCESS) {
            printf("[ERROR] Failed to map slave %d: %d\n", i, pd_status);
            pd_free_image(&g_pd_image);
            pd_shutdown();
            hal_shutdown();
            return 1;
        }

        printf("[OK] Mapped slave %d (addr: 0x%04X, in: %u@%u, out: %u@%u)\n",
               i, mapping.station_address,
               mapping.input_size, mapping.input_offset,
               mapping.output_size, mapping.output_offset);
    }

    printf("\n");

    /* Step 4: Cyclic operation with redundancy */
    printf("[STEP 4] Starting cyclic operation with redundancy...\n");
    printf("Press Ctrl+C to stop\n\n");

    uint32_t cycle_count = 0;
    uint32_t health_check_counter = 0;
    uint16_t expected_wkc = SLAVE_COUNT * 2;  /* Each slave increments WKC twice (read + write) */

    while (g_running) {
        cycle_count++;

        /* Write output data (simple counter pattern) */
        for (uint32_t i = 0; i < total_output_size; i++) {
            g_pd_image.output_data[i] = (uint8_t)(cycle_count + i);
        }

        /* Exchange process data with redundancy */
        bool success = exchange_process_data_redundant(expected_wkc);

        if (!success) {
            printf("[ERROR] Cycle %u: Process data exchange failed on both ports!\n", cycle_count);
        }

        /* Check for automatic failover */
        handle_failover();

        /* Periodic health check */
        health_check_counter++;
        if (health_check_counter >= HEALTH_CHECK_INTERVAL) {
            printf("\n[CYCLE %u] Port Health Check:\n", cycle_count);
            check_port_health();
            printf("\n");
            health_check_counter = 0;
        }

        /* Print status every 1000 cycles */
        if (cycle_count % 1000 == 0) {
            printf("[CYCLE %u] Active port: %s, Failovers: %u, Primary success: %u, Secondary success: %u\n",
                   cycle_count,
                   (g_redundancy.active_port == PD_PORT_PRIMARY) ? "PRIMARY" : "SECONDARY",
                   g_redundancy.failover_count,
                   g_redundancy.primary_success_count,
                   g_redundancy.secondary_success_count);
        }

        /* Sleep to maintain cycle time */
        hal_sleep_us(CYCLE_TIME_US);
    }

    /* Step 5: Cleanup */
    printf("\n[STEP 5] Cleaning up...\n");

    /* Get final statistics */
    pd_statistics_t stats;
    pd_get_statistics(&stats);

    printf("\n=============================================================\n");
    printf("Final Statistics:\n");
    printf("=============================================================\n");
    printf("Total cycles:           %lu\n", stats.cycle_count);
    printf("WKC errors:             %lu\n", stats.wkc_error_count);
    printf("Timeouts:               %lu\n", stats.timeout_count);
    printf("Min cycle time:         %u us\n", stats.min_cycle_time_us);
    printf("Max cycle time:         %u us\n", stats.max_cycle_time_us);
    printf("Avg cycle time:         %u us\n", stats.avg_cycle_time_us);
    printf("Last working counter:   %u\n", stats.last_working_counter);
    printf("\nRedundancy Statistics:\n");
    printf("Total failovers:        %u\n", g_redundancy.failover_count);
    printf("Primary successes:      %u\n", g_redundancy.primary_success_count);
    printf("Secondary successes:    %u\n", g_redundancy.secondary_success_count);
    printf("Final active port:      %s\n",
           (g_redundancy.active_port == PD_PORT_PRIMARY) ? "PRIMARY" : "SECONDARY");
    printf("=============================================================\n\n");

    /* Free resources */
    pd_free_image(&g_pd_image);
    pd_shutdown();
    hal_shutdown();

    printf("[OK] Shutdown complete\n");
    return 0;
}
