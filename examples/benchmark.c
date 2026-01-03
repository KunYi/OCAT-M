/**
 * @file benchmark.c
 * @brief EtherCAT Performance Benchmark Tool
 * @version 1.0.0
 * @date 2026-01-03
 *
 * This tool measures and reports detailed performance metrics for the
 * EtherCAT Master Stack, including:
 * - Cycle time statistics (min/max/avg/jitter)
 * - Frame transmission latency
 * - Working counter validation overhead
 * - Memory usage
 * - CPU utilization
 * - Throughput (frames/sec, bytes/sec)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sys/resource.h>
#include "ethercat/master.h"
#include "ethercat/process_data.h"

/* Benchmark Configuration */
#define NETWORK_INTERFACE "eth0"
#define WARMUP_CYCLES     1000      /* Warmup cycles before measurement */
#define BENCHMARK_CYCLES  100000    /* Number of cycles to measure */

/* Test configurations */
typedef struct {
    const char* name;
    uint32_t cycle_time_us;
    uint32_t frequency_hz;
} test_config_t;

static const test_config_t test_configs[] = {
    {"1kHz (1ms)",   1000,  1000},
    {"2kHz (500us)", 500,   2000},
    {"4kHz (250us)", 250,   4000},
    {"10kHz (100us)", 100,  10000},
};

#define NUM_TEST_CONFIGS (sizeof(test_configs) / sizeof(test_configs[0]))

/* Global flag for clean shutdown */
static volatile sig_atomic_t g_running = 1;

/* Benchmark results */
typedef struct {
    uint64_t total_cycles;
    uint64_t successful_cycles;
    uint64_t failed_cycles;
    uint64_t wkc_errors;
    uint64_t timeouts;

    uint32_t min_cycle_time_ns;
    uint32_t max_cycle_time_ns;
    uint64_t total_cycle_time_ns;

    uint32_t min_latency_ns;
    uint32_t max_latency_ns;
    uint64_t total_latency_ns;

    uint64_t start_time_ns;
    uint64_t end_time_ns;

    /* Resource usage */
    long max_rss_kb;
    double cpu_time_sec;
} benchmark_results_t;

/**
 * @brief Signal handler for clean shutdown
 */
static void signal_handler(int signum)
{
    (void)signum;
    printf("\nBenchmark interrupted...\n");
    g_running = 0;
}

/**
 * @brief Get current time in nanoseconds
 */
static uint64_t get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
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
 * @brief Get resource usage
 */
static void get_resource_usage(long* max_rss_kb, double* cpu_time_sec)
{
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    *max_rss_kb = usage.ru_maxrss;
    *cpu_time_sec = (double)usage.ru_utime.tv_sec +
                    (double)usage.ru_utime.tv_usec / 1000000.0 +
                    (double)usage.ru_stime.tv_sec +
                    (double)usage.ru_stime.tv_usec / 1000000.0;
}

/**
 * @brief Initialize benchmark results
 */
static void init_results(benchmark_results_t* results)
{
    memset(results, 0, sizeof(benchmark_results_t));
    results->min_cycle_time_ns = UINT32_MAX;
    results->min_latency_ns = UINT32_MAX;
}

/**
 * @brief Update benchmark results with cycle measurement
 */
static void update_results(benchmark_results_t* results,
                          uint32_t cycle_time_ns,
                          uint32_t latency_ns,
                          bool success,
                          bool wkc_error)
{
    results->total_cycles++;

    if (success) {
        results->successful_cycles++;
    } else {
        results->failed_cycles++;
    }

    if (wkc_error) {
        results->wkc_errors++;
    }

    /* Update cycle time statistics */
    if (cycle_time_ns < results->min_cycle_time_ns) {
        results->min_cycle_time_ns = cycle_time_ns;
    }
    if (cycle_time_ns > results->max_cycle_time_ns) {
        results->max_cycle_time_ns = cycle_time_ns;
    }
    results->total_cycle_time_ns += cycle_time_ns;

    /* Update latency statistics */
    if (latency_ns < results->min_latency_ns) {
        results->min_latency_ns = latency_ns;
    }
    if (latency_ns > results->max_latency_ns) {
        results->max_latency_ns = latency_ns;
    }
    results->total_latency_ns += latency_ns;
}

/**
 * @brief Print benchmark results
 */
static void print_results(const char* test_name,
                         uint32_t target_cycle_us,
                         const benchmark_results_t* results)
{
    double duration_sec = (double)(results->end_time_ns - results->start_time_ns) / 1e9;
    double avg_cycle_time_us = (double)results->total_cycle_time_ns /
                               (double)results->total_cycles / 1000.0;
    double avg_latency_us = (double)results->total_latency_ns /
                           (double)results->total_cycles / 1000.0;
    double jitter_us = (double)(results->max_cycle_time_ns - results->min_cycle_time_ns) / 1000.0;
    double success_rate = (double)results->successful_cycles /
                         (double)results->total_cycles * 100.0;
    double actual_frequency = (double)results->total_cycles / duration_sec;
    double throughput_mbps = (double)(results->total_cycles * 64 * 8) / duration_sec / 1e6;

    printf("\n========================================\n");
    printf("Benchmark Results: %s\n", test_name);
    printf("========================================\n");

    printf("\nTest Configuration:\n");
    printf("  Target Cycle Time:  %u us\n", target_cycle_us);
    printf("  Target Frequency:   %.0f Hz\n", 1000000.0 / target_cycle_us);
    printf("  Total Cycles:       %lu\n", results->total_cycles);
    printf("  Duration:           %.3f seconds\n", duration_sec);

    printf("\nCycle Statistics:\n");
    printf("  Successful Cycles:  %lu (%.2f%%)\n",
           results->successful_cycles, success_rate);
    printf("  Failed Cycles:      %lu (%.2f%%)\n",
           results->failed_cycles, 100.0 - success_rate);
    printf("  WKC Errors:         %lu (%.2f%%)\n",
           results->wkc_errors,
           (double)results->wkc_errors / results->total_cycles * 100.0);
    printf("  Timeouts:           %lu (%.2f%%)\n",
           results->timeouts,
           (double)results->timeouts / results->total_cycles * 100.0);

    printf("\nTiming Statistics:\n");
    printf("  Min Cycle Time:     %.2f us\n",
           (double)results->min_cycle_time_ns / 1000.0);
    printf("  Max Cycle Time:     %.2f us\n",
           (double)results->max_cycle_time_ns / 1000.0);
    printf("  Avg Cycle Time:     %.2f us\n", avg_cycle_time_us);
    printf("  Jitter:             %.2f us\n", jitter_us);
    printf("  Min Latency:        %.2f us\n",
           (double)results->min_latency_ns / 1000.0);
    printf("  Max Latency:        %.2f us\n",
           (double)results->max_latency_ns / 1000.0);
    printf("  Avg Latency:        %.2f us\n", avg_latency_us);

    printf("\nPerformance Metrics:\n");
    printf("  Actual Frequency:   %.2f Hz\n", actual_frequency);
    printf("  Frequency Error:    %.2f%%\n",
           fabs(actual_frequency - 1000000.0/target_cycle_us) /
           (1000000.0/target_cycle_us) * 100.0);
    printf("  Throughput:         %.2f frames/sec\n", actual_frequency);
    printf("  Throughput:         %.2f Mbps (assuming 64 byte frames)\n", throughput_mbps);

    printf("\nResource Usage:\n");
    printf("  Max RSS:            %ld KB (%.2f MB)\n",
           results->max_rss_kb, (double)results->max_rss_kb / 1024.0);
    printf("  CPU Time:           %.3f seconds\n", results->cpu_time_sec);
    printf("  CPU Utilization:    %.2f%%\n",
           results->cpu_time_sec / duration_sec * 100.0);

    printf("========================================\n");
}

/**
 * @brief Run benchmark for specific configuration
 */
static int run_benchmark(const char* interface __attribute__((unused)),
                        const test_config_t* config,
                        uint16_t slave_count __attribute__((unused)))
{
    master_status_t status;
    benchmark_results_t results;
    uint64_t cycle_counter = 0;
    uint64_t next_cycle;

    init_results(&results);

    printf("\nRunning benchmark: %s\n", config->name);
    printf("  Warmup cycles: %u\n", WARMUP_CYCLES);
    printf("  Benchmark cycles: %u\n", BENCHMARK_CYCLES);
    printf("  Press Ctrl+C to stop early\n");

    /* Warmup phase */
    printf("  Warming up...");
    fflush(stdout);

    next_cycle = get_time_ns();
    for (uint32_t i = 0; i < WARMUP_CYCLES && g_running; i++) {
        uint64_t current_time = get_time_ns();

        if (current_time >= next_cycle) {
            master_process_cycle();
            next_cycle += config->cycle_time_us * 1000ULL;
        }

        if (next_cycle > current_time) {
            sleep_us((next_cycle - current_time) / 1000);
        }
    }
    printf(" done\n");

    /* Benchmark phase */
    printf("  Benchmarking...");
    fflush(stdout);

    results.start_time_ns = get_time_ns();
    next_cycle = results.start_time_ns;

    while (cycle_counter < BENCHMARK_CYCLES && g_running) {
        uint64_t cycle_start = get_time_ns();
        uint64_t current_time = cycle_start;

        if (current_time >= next_cycle) {
            /* Measure cycle execution time */
            uint64_t exec_start = get_time_ns();
            status = master_process_cycle();
            uint64_t exec_end = get_time_ns();

            uint32_t cycle_time_ns = (uint32_t)(exec_end - cycle_start);
            uint32_t latency_ns = (uint32_t)(exec_end - exec_start);
            bool success = (status == MASTER_STATUS_SUCCESS);

            /* Get WKC status */
            pd_statistics_t stats;
            master_get_cyclic_statistics(&stats);
            bool wkc_error = (stats.wkc_error_count > results.wkc_errors);

            update_results(&results, cycle_time_ns, latency_ns, success, wkc_error);

            cycle_counter++;
            next_cycle += config->cycle_time_us * 1000ULL;

            /* Print progress every 10000 cycles */
            if (cycle_counter % 10000 == 0) {
                printf(".");
                fflush(stdout);
            }
        }

        /* Sleep until next cycle */
        current_time = get_time_ns();
        if (next_cycle > current_time) {
            sleep_us((next_cycle - current_time) / 1000);
        }
    }

    results.end_time_ns = get_time_ns();
    printf(" done\n");

    /* Get final resource usage */
    get_resource_usage(&results.max_rss_kb, &results.cpu_time_sec);

    /* Print results */
    print_results(config->name, config->cycle_time_us, &results);

    return 0;
}

/**
 * @brief Main function
 */
int main(int argc, char* argv[])
{
    master_status_t status;
    uint16_t slave_count = 0;

    printf("=================================================\n");
    printf("EtherCAT Performance Benchmark Tool\n");
    printf("=================================================\n\n");

    /* Parse command line arguments */
    const char* interface = NETWORK_INTERFACE;
    if (argc > 1) {
        interface = argv[1];
    }

    printf("Configuration:\n");
    printf("  Network Interface: %s\n", interface);
    printf("  Warmup Cycles:     %u\n", WARMUP_CYCLES);
    printf("  Benchmark Cycles:  %u per test\n", BENCHMARK_CYCLES);
    printf("  Number of Tests:   %zu\n", NUM_TEST_CONFIGS);
    printf("\n");

    /* Install signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize Master */
    printf("Initializing EtherCAT Master...\n");

    master_config_t config = {
        .interface_name = interface,
        .mac_address = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        .cycle_time_us = 1000,  /* Will be changed per test */
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

    /* Configure Slaves */
    if (slave_count > 0) {
        printf("Configuring slaves...\n");
        status = master_configure_slaves();
        if (status != MASTER_STATUS_SUCCESS) {
            fprintf(stderr, "ERROR: Configuration failed (status=%d)\n", status);
            master_shutdown();
            return 1;
        }
        printf("  Configuration complete\n");
    }

    /* Allocate Process Data */
    printf("Allocating process data...\n");
    status = master_allocate_process_data(NULL);
    if (status != MASTER_STATUS_SUCCESS) {
        fprintf(stderr, "ERROR: Process data allocation failed (status=%d)\n", status);
        master_shutdown();
        return 1;
    }
    printf("  Process data allocated\n");

    /* Transition to OPERATIONAL */
    printf("Transitioning to OPERATIONAL...\n");
    master_request_state(0x04);  /* SAFEOP */
    master_start_cyclic();
    master_request_state(0x08);  /* OP */
    printf("  Ready for benchmarking\n");

    /* Run benchmarks for each configuration */
    for (size_t i = 0; i < NUM_TEST_CONFIGS && g_running; i++) {
        run_benchmark(interface, &test_configs[i], slave_count);

        if (i < NUM_TEST_CONFIGS - 1) {
            printf("\nWaiting 2 seconds before next test...\n");
            sleep(2);
        }
    }

    /* Shutdown */
    printf("\nShutting down...\n");
    master_stop_cyclic();
    master_request_state(0x01);  /* INIT */
    master_shutdown();
    printf("Shutdown complete\n");

    printf("\n=================================================\n");
    printf("Benchmark completed\n");
    printf("=================================================\n");

    return 0;
}
