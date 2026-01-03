/**
 * @file dll_config.h
 * @brief EtherCAT Data Link Layer - Configuration
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Based on ETG1000.3 - EtherCAT Data Link Layer Services
 *
 * This file contains configuration-related definitions and default values.
 */

#ifndef ETHERCAT_DLL_CONFIG_H
#define ETHERCAT_DLL_CONFIG_H

#include "dll_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup DLL_Config Data Link Layer Configuration
 * @{
 */

/* ========================================================================== */
/* Default Configuration Values                                               */
/* ========================================================================== */

/** Default maximum frame size (standard Ethernet frame) */
#define DL_DEFAULT_MAX_FRAME_SIZE       1518

/** Default TX queue size (number of frames) */
#define DL_DEFAULT_TX_QUEUE_SIZE        64

/** Default RX queue size (number of frames) */
#define DL_DEFAULT_RX_QUEUE_SIZE        64

/** Default cycle time in microseconds (1ms) */
#define DL_DEFAULT_CYCLE_TIME_US        1000

/** Default number of Ethernet ports */
#define DL_DEFAULT_NUM_PORTS            1

/** Default redundancy setting */
#define DL_DEFAULT_REDUNDANCY_ENABLE    false

/** Default distributed clocks setting */
#define DL_DEFAULT_DC_ENABLE            false

/* ========================================================================== */
/* Timing Constants (from ETG1000.3)                                          */
/* ========================================================================== */

/** Minimum cycle time in microseconds */
#define DL_MIN_CYCLE_TIME_US            50

/** Typical cycle time in microseconds */
#define DL_TYPICAL_CYCLE_TIME_US        250

/** Maximum cycle time in microseconds (for timeout calculations) */
#define DL_MAX_CYCLE_TIME_US            100000

/** Frame processing time per slave (microseconds) */
#define DL_FRAME_PROCESSING_TIME_US     1

/** Maximum jitter tolerance with Distributed Clocks (microseconds) */
#define DL_MAX_JITTER_US                1

/* ========================================================================== */
/* Size Limits                                                                */
/* ========================================================================== */

/** Minimum frame size (Ethernet minimum) */
#define DL_MIN_FRAME_SIZE               64

/** Maximum frame size (Ethernet maximum) */
#define DL_MAX_FRAME_SIZE               1518

/** Minimum queue size */
#define DL_MIN_QUEUE_SIZE               4

/** Maximum queue size */
#define DL_MAX_QUEUE_SIZE               1024

/** Maximum number of ports */
#define DL_MAX_NUM_PORTS                2

/** Priority levels (0-7) */
#define DL_MAX_PRIORITY                 7

/* ========================================================================== */
/* Configuration Functions                                                    */
/* ========================================================================== */

/**
 * @brief Initialize configuration structure with default values
 *
 * Fills the configuration structure with default values suitable
 * for most applications.
 *
 * @param config Pointer to configuration structure to initialize
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dl_config_init_defaults(dl_config_t* config);

/**
 * @brief Validate configuration structure
 *
 * Checks if all configuration parameters are within valid ranges.
 *
 * @param config Pointer to configuration structure to validate
 * @return DL_STATUS_SUCCESS if valid, DL_STATUS_INVALID_PARAM otherwise
 */
dl_status_t dl_config_validate(const dl_config_t* config);

/**
 * @brief Set parameter in configuration structure
 *
 * Helper function to set individual parameters in the configuration.
 *
 * @param config Pointer to configuration structure
 * @param param_id Parameter identifier
 * @param value Pointer to parameter value
 * @param length Length of parameter value
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dl_config_set_parameter(dl_config_t* config,
                                     dl_param_id_t param_id,
                                     const void* value,
                                     uint16_t length);

/**
 * @brief Get parameter from configuration structure
 *
 * Helper function to get individual parameters from the configuration.
 *
 * @param config Pointer to configuration structure
 * @param param_id Parameter identifier
 * @param value Pointer to buffer for parameter value
 * @param length Pointer to length (in: buffer size, out: actual size)
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dl_config_get_parameter(const dl_config_t* config,
                                     dl_param_id_t param_id,
                                     void* value,
                                     uint16_t* length);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_DLL_CONFIG_H */
