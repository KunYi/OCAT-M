/**
 * @file master.c
 * @brief EtherCAT Master - Core Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "ethercat/master.h"
#include "ethercat/scan.h"
#include "ethercat/al.h"
#include "ethercat/coe.h"
#include "ethercat/dll.h"
#include "ethercat/hal.h"
#include "master_internal.h"
#include <string.h>
#include <stdlib.h>

/* ========================================================================== */
/* Global Master Context                                                     */
/* ========================================================================== */

static master_context_t g_master_context = {0};

/* ========================================================================== */
/* Internal Functions                                                        */
/* ========================================================================== */

master_context_t* master_get_context(void)
{
    return &g_master_context;
}

/* ========================================================================== */
/* Initialization and Shutdown                                               */
/* ========================================================================== */

master_status_t master_init(const master_config_t* config)
{
    if (g_master_context.initialized) {
        return MASTER_STATUS_ERROR;
    }

    if (config == NULL) {
        return MASTER_STATUS_INVALID_PARAM;
    }

    /* Initialize context */
    memset(&g_master_context, 0, sizeof(master_context_t));
    memcpy(&g_master_context.config, config, sizeof(master_config_t));

    /* Initialize HAL */
    hal_config_t hal_config = {
        .platform = HAL_PLATFORM_LINUX_RAW_SOCKET,
        .interface_name = config->interface_name,
        .promiscuous_mode = true,
        .rx_buffer_count = 32,
        .tx_buffer_count = 32,
        .rx_buffer_size = 1518,
        .tx_buffer_size = 1518,
        .blocking_mode = false
    };
    memcpy(hal_config.mac_address, config->mac_address, 6);

    hal_status_t hal_status = hal_init(&hal_config);
    if (hal_status != HAL_STATUS_SUCCESS) {
        return MASTER_STATUS_ERROR;
    }

    /* Initialize DLL */
    dl_config_t dll_config = {
        .max_frame_size = 1518,
        .tx_queue_size = 32,
        .rx_queue_size = 32
    };
    memcpy(dll_config.mac_address, config->mac_address, 6);

    dl_status_t dll_status = dl_init(&dll_config);
    if (dll_status != DL_STATUS_SUCCESS) {
        hal_shutdown();
        return MASTER_STATUS_ERROR;
    }

    /* Initialize AL */
    al_config_t al_config = {
        .max_slaves = MASTER_MAX_SLAVES,
        .mailbox_timeout_ms = 1000,
        .state_transition_timeout_ms = 1000
    };

    al_status_t al_status = al_init(&al_config);
    if (al_status != AL_STATUS_SUCCESS) {
        dl_shutdown();
        hal_shutdown();
        return MASTER_STATUS_ERROR;
    }

    /* Initialize CoE */
    coe_status_t coe_status = coe_init();
    if (coe_status != COE_STATUS_SUCCESS) {
        al_shutdown();
        dl_shutdown();
        hal_shutdown();
        return MASTER_STATUS_ERROR;
    }

    /* Initialize scan module */
    scan_status_t scan_status = scan_init();
    if (scan_status != SCAN_STATUS_SUCCESS) {
        coe_shutdown();
        al_shutdown();
        dl_shutdown();
        hal_shutdown();
        return MASTER_STATUS_ERROR;
    }

    g_master_context.state = MASTER_STATE_IDLE;
    g_master_context.initialized = true;

    return MASTER_STATUS_SUCCESS;
}

master_status_t master_shutdown(void)
{
    if (!g_master_context.initialized) {
        return MASTER_STATUS_NOT_INITIALIZED;
    }

    /* Stop cyclic operation if active */
    if (g_master_context.cyclic_active) {
        master_stop_cyclic();
    }

    /* Shutdown all modules */
    scan_shutdown();
    coe_shutdown();
    al_shutdown();
    dl_shutdown();
    hal_shutdown();

    /* Clear context */
    memset(&g_master_context, 0, sizeof(master_context_t));

    return MASTER_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Network Scanning                                                          */
/* ========================================================================== */

master_status_t master_scan_network(void)
{
    if (!g_master_context.initialized) {
        return MASTER_STATUS_NOT_INITIALIZED;
    }

    g_master_context.state = MASTER_STATE_SCANNING;

    /* Discover slaves */
    uint16_t slave_count = 0;
    uint32_t timeout_ms = g_master_context.config.scan_timeout_ms;
    if (timeout_ms == 0) {
        timeout_ms = MASTER_DEFAULT_TIMEOUT_MS;
    }

    scan_status_t status = scan_discover_slaves(&slave_count, timeout_ms);
    if (status != SCAN_STATUS_SUCCESS) {
        g_master_context.state = MASTER_STATE_ERROR;
        return MASTER_STATUS_ERROR;
    }

    if (slave_count == 0) {
        g_master_context.state = MASTER_STATE_IDLE;
        return MASTER_STATUS_NO_SLAVES;
    }

    g_master_context.slave_count = slave_count;

    /* Assign station addresses */
    status = scan_assign_station_addresses(slave_count, timeout_ms);
    if (status != SCAN_STATUS_SUCCESS) {
        g_master_context.state = MASTER_STATE_ERROR;
        return MASTER_STATUS_ERROR;
    }

    /* Read slave information */
    for (uint16_t i = 0; i < slave_count; i++) {
        master_slave_t* slave = &g_master_context.slaves[i];

        /* Get discovery info */
        slave_discovery_t discovery;
        status = scan_get_discovery_info(i, &discovery);
        if (status == SCAN_STATUS_SUCCESS) {
            slave->position = discovery.position;
            slave->auto_inc_address = discovery.auto_inc_address;
            slave->station_address = discovery.station_address;
            slave->alias_address = discovery.alias_address;
            slave->port_descriptors = discovery.port_descriptors;
            slave->link_port[0] = discovery.link_port0;
            slave->link_port[1] = discovery.link_port1;
            slave->link_port[2] = discovery.link_port2;
            slave->link_port[3] = discovery.link_port3;
        }

        /* Read slave identification */
        uint16_t station_addr = 0x1000 + i;
        status = scan_read_slave_id(station_addr,
                                     &slave->vendor_id,
                                     &slave->product_code,
                                     &slave->revision_number,
                                     &slave->serial_number,
                                     timeout_ms);

        /* Read slave name */
        status = scan_read_slave_name(station_addr,
                                       slave->name,
                                       sizeof(slave->name),
                                       timeout_ms);

        /* Determine capabilities from EEPROM */
        /* TODO: Read mailbox protocol support from EEPROM */
        slave->has_mailbox = true;  /* Assume for now */
        slave->has_coe = true;
        slave->has_foe = false;
        slave->has_soe = false;
        slave->has_eoe = false;

        /* Initialize state */
        slave->al_state = AL_STATE_INIT;
        slave->al_status_code = 0;
        slave->configured = false;
        slave->operational = false;

        /* Count ports */
        slave->num_ports = 0;
        for (int j = 0; j < 4; j++) {
            if (slave->link_port[j]) {
                slave->num_ports++;
            }
        }
    }

    /* Detect topology */
    status = scan_detect_topology(slave_count);
    if (status != SCAN_STATUS_SUCCESS) {
        g_master_context.state = MASTER_STATE_ERROR;
        return MASTER_STATUS_ERROR;
    }

    /* Update topology information */
    g_master_context.topology.slave_count = slave_count;
    g_master_context.topology.working_counter_expected = slave_count;
    g_master_context.topology.cycle_time_us = g_master_context.config.cycle_time_us;
    g_master_context.topology.topology_valid = true;
    g_master_context.topology.dc_available = false;  /* TODO: Detect DC support */

    g_master_context.state = MASTER_STATE_PREOP;

    return MASTER_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Slave Information                                                         */
/* ========================================================================== */

master_status_t master_get_slave_count(uint16_t* count)
{
    if (!g_master_context.initialized || count == NULL) {
        return MASTER_STATUS_INVALID_PARAM;
    }

    *count = g_master_context.slave_count;
    return MASTER_STATUS_SUCCESS;
}

master_status_t master_get_slave_info(uint16_t position, slave_info_t* info)
{
    if (!g_master_context.initialized || info == NULL) {
        return MASTER_STATUS_INVALID_PARAM;
    }

    if (position >= g_master_context.slave_count) {
        return MASTER_STATUS_INVALID_PARAM;
    }

    master_slave_t* slave = &g_master_context.slaves[position];

    /* Copy information to output structure */
    info->station_address = slave->station_address;
    info->alias_address = slave->alias_address;
    info->vendor_id = slave->vendor_id;
    info->product_code = slave->product_code;
    info->revision_number = slave->revision_number;
    info->serial_number = slave->serial_number;
    info->position = slave->position;
    info->port_descriptors = slave->port_descriptors;
    info->num_ports = slave->num_ports;
    info->has_mailbox = slave->has_mailbox;
    info->has_coe = slave->has_coe;
    info->has_foe = slave->has_foe;
    info->has_soe = slave->has_soe;
    info->has_eoe = slave->has_eoe;
    strncpy(info->name, slave->name, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';

    return MASTER_STATUS_SUCCESS;
}

master_status_t master_get_topology(network_topology_t* topology)
{
    if (!g_master_context.initialized || topology == NULL) {
        return MASTER_STATUS_INVALID_PARAM;
    }

    memcpy(topology, &g_master_context.topology, sizeof(network_topology_t));
    return MASTER_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Slave Configuration                                                       */
/* ========================================================================== */

master_status_t master_configure_slaves(void)
{
    if (!g_master_context.initialized) {
        return MASTER_STATUS_NOT_INITIALIZED;
    }

    g_master_context.state = MASTER_STATE_CONFIGURING;

    /* Configure each slave */
    for (uint16_t i = 0; i < g_master_context.slave_count; i++) {
        master_slave_t* slave = &g_master_context.slaves[i];

        /* TODO: Configure sync managers */
        /* TODO: Configure mailbox */
        /* TODO: Configure PDO mappings */

        slave->configured = true;
    }

    g_master_context.state = MASTER_STATE_PREOP;

    return MASTER_STATUS_SUCCESS;
}

/* ========================================================================== */
/* State Management                                                          */
/* ========================================================================== */

master_status_t master_request_state(uint8_t requested_state)
{
    if (!g_master_context.initialized) {
        return MASTER_STATUS_NOT_INITIALIZED;
    }

    uint32_t timeout_ms = g_master_context.config.state_change_timeout_ms;
    if (timeout_ms == 0) {
        timeout_ms = MASTER_DEFAULT_TIMEOUT_MS;
    }

    /* Request state change for all slaves */
    for (uint16_t i = 0; i < g_master_context.slave_count; i++) {
        master_slave_t* slave = &g_master_context.slaves[i];

        /* TODO: Request state change via AL */
        /* al_request_state(slave->station_address, requested_state, timeout_ms); */

        slave->al_state = requested_state;
    }

    /* Update master state */
    switch (requested_state) {
        case AL_STATE_INIT:
            g_master_context.state = MASTER_STATE_INIT;
            break;
        case AL_STATE_PREOP:
            g_master_context.state = MASTER_STATE_PREOP;
            break;
        case AL_STATE_SAFEOP:
            g_master_context.state = MASTER_STATE_SAFEOP;
            break;
        case AL_STATE_OP:
            g_master_context.state = MASTER_STATE_OP;
            break;
        default:
            return MASTER_STATUS_INVALID_PARAM;
    }

    return MASTER_STATUS_SUCCESS;
}

master_status_t master_get_state(master_state_t* state)
{
    if (!g_master_context.initialized || state == NULL) {
        return MASTER_STATUS_INVALID_PARAM;
    }

    *state = g_master_context.state;
    return MASTER_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Cyclic Operation                                                          */
/* ========================================================================== */

master_status_t master_start_cyclic(void)
{
    if (!g_master_context.initialized) {
        return MASTER_STATUS_NOT_INITIALIZED;
    }

    if (g_master_context.state != MASTER_STATE_SAFEOP &&
        g_master_context.state != MASTER_STATE_OP) {
        return MASTER_STATUS_ERROR;
    }

    g_master_context.cyclic_active = true;
    g_master_context.cycle_counter = 0;
    g_master_context.cycle_start_time = hal_get_time_ns();

    return MASTER_STATUS_SUCCESS;
}

master_status_t master_stop_cyclic(void)
{
    if (!g_master_context.initialized) {
        return MASTER_STATUS_NOT_INITIALIZED;
    }

    g_master_context.cyclic_active = false;

    return MASTER_STATUS_SUCCESS;
}

master_status_t master_process_cycle(void)
{
    if (!g_master_context.initialized) {
        return MASTER_STATUS_NOT_INITIALIZED;
    }

    if (!g_master_context.cyclic_active) {
        return MASTER_STATUS_ERROR;
    }

    /* TODO: Send process data frame (LRW) */
    /* TODO: Receive process data response */
    /* TODO: Check working counter */

    g_master_context.cycle_counter++;

    return MASTER_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Utility Functions                                                         */
/* ========================================================================== */

const char* master_get_version(void)
{
    return "1.0.0";
}
