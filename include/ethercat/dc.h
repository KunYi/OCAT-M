/**
 * @file dc.h
 * @brief EtherCAT Distributed Clocks - Public API
 * @version 1.0.0
 * @date 2026-01-03
 *
 * This file contains the API for EtherCAT Distributed Clocks (DC) including
 * time synchronization, drift compensation, and DC monitoring.
 */

#ifndef ETHERCAT_DC_H
#define ETHERCAT_DC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup DC_API Distributed Clocks API
 * @{
 */

/* ========================================================================== */
/* DC Status Codes                                                           */
/* ========================================================================== */

/**
 * @brief DC status codes
 */
typedef enum {
    DC_STATUS_SUCCESS = 0x00,           /**< Operation successful */
    DC_STATUS_ERROR = 0x01,             /**< General error */
    DC_STATUS_TIMEOUT = 0x02,           /**< Operation timeout */
    DC_STATUS_INVALID_PARAM = 0x03,     /**< Invalid parameter */
    DC_STATUS_NOT_SUPPORTED = 0x04,     /**< DC not supported by slave */
    DC_STATUS_NOT_INITIALIZED = 0x05    /**< DC not initialized */
} dc_status_t;

/* ========================================================================== */
/* DC Register Addresses                                                     */
/* ========================================================================== */

#define DC_REG_SYSTEM_TIME              0x0910  /**< System time (64-bit) */
#define DC_REG_RECEIVE_TIME_PORT0       0x0900  /**< Receive time port 0 (32-bit) */
#define DC_REG_RECEIVE_TIME_PORT1       0x0904  /**< Receive time port 1 (32-bit) */
#define DC_REG_RECEIVE_TIME_PORT2       0x0908  /**< Receive time port 2 (32-bit) */
#define DC_REG_RECEIVE_TIME_PORT3       0x090C  /**< Receive time port 3 (32-bit) */
#define DC_REG_SYSTEM_TIME_OFFSET       0x0920  /**< System time offset (64-bit) */
#define DC_REG_SYSTEM_TIME_DELAY        0x0928  /**< System time delay (32-bit) */
#define DC_REG_SYSTEM_TIME_DIFF         0x092C  /**< System time difference (32-bit) */
#define DC_REG_SPEED_COUNTER_START      0x0930  /**< Speed counter start (16-bit) */
#define DC_REG_SPEED_COUNTER_DIFF       0x0932  /**< Speed counter diff (16-bit) */
#define DC_REG_SYSTEM_TIME_DIFF_FILTER  0x0934  /**< System time diff filter depth (16-bit) */
#define DC_REG_SPEED_COUNTER_FILTER     0x0936  /**< Speed counter filter depth (16-bit) */
#define DC_REG_CYCLIC_UNIT_CONTROL      0x0980  /**< Cyclic unit control (8-bit) */
#define DC_REG_ACTIVATION               0x0981  /**< Activation register (8-bit) */
#define DC_REG_SYNC_IMPULSE_LENGTH      0x0982  /**< Sync impulse length (16-bit) */
#define DC_REG_SYNC0_STATUS             0x098E  /**< SYNC0 status (16-bit) */
#define DC_REG_SYNC1_STATUS             0x098F  /**< SYNC1 status (16-bit) */
#define DC_REG_SYNC0_CYCLE_TIME         0x09A0  /**< SYNC0 cycle time (32-bit) */
#define DC_REG_SYNC1_CYCLE_TIME         0x09A4  /**< SYNC1 cycle time (32-bit) */
#define DC_REG_LATCH0_CONTROL           0x09A8  /**< Latch0 control (8-bit) */
#define DC_REG_LATCH1_CONTROL           0x09A9  /**< Latch1 control (8-bit) */
#define DC_REG_LATCH0_STATUS            0x09AE  /**< Latch0 status (8-bit) */
#define DC_REG_LATCH1_STATUS            0x09AF  /**< Latch1 status (8-bit) */
#define DC_REG_LATCH0_TIME_POS          0x09B0  /**< Latch0 time positive edge (64-bit) */
#define DC_REG_LATCH0_TIME_NEG          0x09B8  /**< Latch0 time negative edge (64-bit) */
#define DC_REG_LATCH1_TIME_POS          0x09C0  /**< Latch1 time positive edge (64-bit) */
#define DC_REG_LATCH1_TIME_NEG          0x09C8  /**< Latch1 time negative edge (64-bit) */

/* ========================================================================== */
/* DC Activation Register Bits                                               */
/* ========================================================================== */

#define DC_ACTIVATION_SYNC0             0x01    /**< SYNC0 activation */
#define DC_ACTIVATION_SYNC1             0x02    /**< SYNC1 activation */
#define DC_ACTIVATION_AUTO_ACTIVATION   0x04    /**< Auto activation */
#define DC_ACTIVATION_EXT_ACTIVATION    0x08    /**< Extension activation */
#define DC_ACTIVATION_START_TIME        0x10    /**< Start time check */
#define DC_ACTIVATION_NEAR_FUTURE       0x20    /**< Near future check */
#define DC_ACTIVATION_SYNC0_DEACTIVATE  0x40    /**< SYNC0 deactivate */
#define DC_ACTIVATION_SYNC1_DEACTIVATE  0x80    /**< SYNC1 deactivate */

/* ========================================================================== */
/* DC Cyclic Unit Control Bits                                               */
/* ========================================================================== */

#define DC_CYCLIC_OPERATION             0x01    /**< Cyclic operation */
#define DC_SYNC0_GENERATION             0x02    /**< SYNC0 generation */
#define DC_SYNC1_GENERATION             0x04    /**< SYNC1 generation */

/* ========================================================================== */
/* DC Configuration Structures                                               */
/* ========================================================================== */

/**
 * @brief DC synchronization mode
 */
typedef enum {
    DC_MODE_OFF = 0,                    /**< DC disabled */
    DC_MODE_DC_SYNC0,                   /**< DC with SYNC0 */
    DC_MODE_DC_SYNC01,                  /**< DC with SYNC0 and SYNC1 */
    DC_MODE_SM_SYNC,                    /**< SM synchronous */
    DC_MODE_FREE_RUN                    /**< Free run mode */
} dc_sync_mode_t;

/**
 * @brief DC slave information
 */
typedef struct {
    uint16_t station_address;           /**< Station address */
    bool dc_supported;                  /**< DC support flag */
    bool is_reference_clock;            /**< Reference clock flag */
    uint64_t system_time;               /**< Current system time (ns) */
    int64_t time_offset;                /**< Time offset from reference (ns) */
    int32_t time_delay;                 /**< Propagation delay (ns) */
    int32_t time_diff;                  /**< Time difference (ns) */
    uint32_t receive_time[4];           /**< Receive time for each port (ns) */
    uint16_t port_count;                /**< Number of active ports */
} dc_slave_info_t;

/**
 * @brief DC SYNC0 configuration
 */
typedef struct {
    bool enabled;                       /**< SYNC0 enabled */
    uint32_t cycle_time_ns;             /**< Cycle time in nanoseconds */
    int32_t shift_time_ns;              /**< Shift time in nanoseconds */
    uint16_t sync_impulse_length_ns;    /**< Sync impulse length in nanoseconds */
} dc_sync0_config_t;

/**
 * @brief DC SYNC1 configuration
 */
typedef struct {
    bool enabled;                       /**< SYNC1 enabled */
    uint32_t cycle_time_ns;             /**< Cycle time in nanoseconds */
    int32_t shift_time_ns;              /**< Shift time in nanoseconds */
} dc_sync1_config_t;

/**
 * @brief DC slave configuration
 */
typedef struct {
    uint16_t station_address;           /**< Station address */
    dc_sync_mode_t sync_mode;           /**< Synchronization mode */
    dc_sync0_config_t sync0;            /**< SYNC0 configuration */
    dc_sync1_config_t sync1;            /**< SYNC1 configuration */
    uint64_t start_time_ns;             /**< Start time in nanoseconds */
    bool assign_activate;               /**< Assign activate flag */
} dc_slave_config_t;

/**
 * @brief DC network configuration
 */
typedef struct {
    bool enabled;                       /**< DC enabled for network */
    uint16_t reference_clock_address;   /**< Reference clock station address */
    uint32_t cycle_time_ns;             /**< Network cycle time in nanoseconds */
    uint16_t filter_depth_time;         /**< Time filter depth */
    uint16_t filter_depth_speed;        /**< Speed filter depth */
} dc_network_config_t;

/**
 * @brief DC statistics
 */
typedef struct {
    uint64_t sync_count;                /**< Number of sync cycles */
    uint64_t sync_error_count;          /**< Number of sync errors */
    int32_t max_time_diff_ns;           /**< Maximum time difference (ns) */
    int32_t min_time_diff_ns;           /**< Minimum time difference (ns) */
    int32_t avg_time_diff_ns;           /**< Average time difference (ns) */
    uint32_t drift_rate_ppb;            /**< Drift rate in parts per billion */
} dc_statistics_t;

/* ========================================================================== */
/* DC API Functions                                                          */
/* ========================================================================== */

/**
 * @brief Initialize Distributed Clocks module
 *
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_init(void);

/**
 * @brief Shutdown Distributed Clocks module
 *
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_shutdown(void);

/**
 * @brief Check if slave supports Distributed Clocks
 *
 * @param station_address Slave station address
 * @param supported Pointer to receive DC support flag
 * @param timeout_ms Timeout in milliseconds
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_check_support(uint16_t station_address,
                              bool* supported,
                              uint32_t timeout_ms);

/**
 * @brief Read slave system time
 *
 * @param station_address Slave station address
 * @param system_time Pointer to receive system time (nanoseconds)
 * @param timeout_ms Timeout in milliseconds
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_read_system_time(uint16_t station_address,
                                 uint64_t* system_time,
                                 uint32_t timeout_ms);

/**
 * @brief Write slave system time offset
 *
 * @param station_address Slave station address
 * @param time_offset Time offset in nanoseconds
 * @param timeout_ms Timeout in milliseconds
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_write_system_time_offset(uint16_t station_address,
                                         int64_t time_offset,
                                         uint32_t timeout_ms);

/**
 * @brief Measure propagation delay for a slave
 *
 * @param station_address Slave station address
 * @param delay Pointer to receive propagation delay (nanoseconds)
 * @param timeout_ms Timeout in milliseconds
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_measure_propagation_delay(uint16_t station_address,
                                          int32_t* delay,
                                          uint32_t timeout_ms);

/**
 * @brief Configure DC for a slave
 *
 * @param config Pointer to DC slave configuration
 * @param timeout_ms Timeout in milliseconds
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_configure_slave(const dc_slave_config_t* config,
                                uint32_t timeout_ms);

/**
 * @brief Configure DC SYNC0 for a slave
 *
 * @param station_address Slave station address
 * @param sync0_config Pointer to SYNC0 configuration
 * @param timeout_ms Timeout in milliseconds
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_configure_sync0(uint16_t station_address,
                                const dc_sync0_config_t* sync0_config,
                                uint32_t timeout_ms);

/**
 * @brief Configure DC SYNC1 for a slave
 *
 * @param station_address Slave station address
 * @param sync1_config Pointer to SYNC1 configuration
 * @param timeout_ms Timeout in milliseconds
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_configure_sync1(uint16_t station_address,
                                const dc_sync1_config_t* sync1_config,
                                uint32_t timeout_ms);

/**
 * @brief Select reference clock slave
 *
 * This function selects the slave with the best clock characteristics
 * as the reference clock for the network.
 *
 * @param slave_count Number of slaves
 * @param station_addresses Array of station addresses
 * @param reference_address Pointer to receive reference clock address
 * @param timeout_ms Timeout in milliseconds
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_select_reference_clock(uint16_t slave_count,
                                       const uint16_t* station_addresses,
                                       uint16_t* reference_address,
                                       uint32_t timeout_ms);

/**
 * @brief Synchronize all slaves to reference clock
 *
 * @param slave_count Number of slaves
 * @param station_addresses Array of station addresses
 * @param reference_address Reference clock station address
 * @param timeout_ms Timeout in milliseconds
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_synchronize_slaves(uint16_t slave_count,
                                   const uint16_t* station_addresses,
                                   uint16_t reference_address,
                                   uint32_t timeout_ms);

/**
 * @brief Compensate clock drift for a slave
 *
 * @param station_address Slave station address
 * @param timeout_ms Timeout in milliseconds
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_compensate_drift(uint16_t station_address,
                                 uint32_t timeout_ms);

/**
 * @brief Get DC slave information
 *
 * @param station_address Slave station address
 * @param info Pointer to receive DC slave information
 * @param timeout_ms Timeout in milliseconds
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_get_slave_info(uint16_t station_address,
                               dc_slave_info_t* info,
                               uint32_t timeout_ms);

/**
 * @brief Get DC statistics
 *
 * @param station_address Slave station address
 * @param stats Pointer to receive DC statistics
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_get_statistics(uint16_t station_address,
                               dc_statistics_t* stats);

/**
 * @brief Reset DC statistics
 *
 * @param station_address Slave station address
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_reset_statistics(uint16_t station_address);

/**
 * @brief Enable DC for a slave
 *
 * @param station_address Slave station address
 * @param timeout_ms Timeout in milliseconds
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_enable(uint16_t station_address, uint32_t timeout_ms);

/**
 * @brief Disable DC for a slave
 *
 * @param station_address Slave station address
 * @param timeout_ms Timeout in milliseconds
 * @return DC_STATUS_SUCCESS on success, error code otherwise
 */
dc_status_t dc_disable(uint16_t station_address, uint32_t timeout_ms);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_DC_H */
