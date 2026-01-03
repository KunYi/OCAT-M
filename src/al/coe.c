/**
 * @file coe.c
 * @brief CANopen over EtherCAT (CoE) - Core Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "ethercat/coe.h"
#include "ethercat/al.h"
#include "ethercat/hal.h"
#include <string.h>
#include <stdlib.h>

/* ========================================================================== */
/* CoE Context                                                               */
/* ========================================================================== */

typedef struct {
    bool initialized;
    uint32_t sdo_timeout_ms;
} coe_context_t;

static coe_context_t g_coe_context = {0};

/* ========================================================================== */
/* Initialization and Shutdown                                               */
/* ========================================================================== */

coe_status_t coe_init(void)
{
    if (g_coe_context.initialized) {
        return COE_STATUS_ERROR;
    }

    memset(&g_coe_context, 0, sizeof(coe_context_t));
    g_coe_context.sdo_timeout_ms = 1000; /* Default 1 second timeout */
    g_coe_context.initialized = true;

    return COE_STATUS_SUCCESS;
}

coe_status_t coe_shutdown(void)
{
    if (!g_coe_context.initialized) {
        return COE_STATUS_ERROR;
    }

    memset(&g_coe_context, 0, sizeof(coe_context_t));
    return COE_STATUS_SUCCESS;
}

/* ========================================================================== */
/* SDO Download (Write to Object Dictionary)                                */
/* ========================================================================== */

coe_status_t coe_sdo_download(uint16_t slave_address,
                               uint16_t index,
                               uint8_t subindex,
                               const uint8_t* data,
                               uint32_t size,
                               uint32_t timeout_ms)
{
    if (!g_coe_context.initialized) {
        return COE_STATUS_ERROR;
    }

    if (data == NULL || size == 0) {
        return COE_STATUS_INVALID_PARAM;
    }

    /* Check if expedited transfer is possible (1-4 bytes) */
    if (size <= 4) {
        uint32_t value = 0;
        memcpy(&value, data, size);
        return coe_sdo_download_expedited(slave_address, index, subindex,
                                          value, (uint8_t)size, timeout_ms);
    }

    /* Normal (segmented) transfer */
    coe_status_t status;

    /* Build CoE header */
    coe_header_t coe_header = {0};
    coe_header.number = 0;
    coe_header.service = COE_SERVICE_SDO_REQUEST;

    /* Build SDO Download Initiate request */
    sdo_download_init_req_t sdo_req = {0};
    sdo_command_byte_t* cmd = (sdo_command_byte_t*)&sdo_req.command;
    cmd->ccs = SDO_CCS_DOWNLOAD_INIT;
    cmd->n = 0;
    cmd->e = 0; /* Normal transfer */
    cmd->s = 1; /* Size indicated */

    sdo_req.index = index;
    sdo_req.subindex = subindex;
    sdo_req.data = size; /* Total size */

    /* Build mailbox message */
    uint8_t mailbox_data[256];
    uint16_t mailbox_length = 0;

    /* Copy CoE header */
    memcpy(mailbox_data, &coe_header, COE_HEADER_SIZE);
    mailbox_length += COE_HEADER_SIZE;

    /* Copy SDO request */
    memcpy(mailbox_data + mailbox_length, &sdo_req, sizeof(sdo_req));
    mailbox_length += sizeof(sdo_req);

    /* Send mailbox message */
    mbx_send_req_t mbx_req = {
        .slave_address = slave_address,
        .type = MBOX_TYPE_COE,
        .data = mailbox_data,
        .length = mailbox_length,
        .priority = 0,
        .user_data = NULL
    };

    status = (coe_status_t)al_mailbox_send(&mbx_req);
    if (status != COE_STATUS_SUCCESS) {
        return status;
    }

    /* Wait for initiate response */
    uint64_t start_time = hal_get_time_ms();
    bool init_response_received = false;

    while (!init_response_received) {
        /* Check timeout */
        if (timeout_ms > 0) {
            uint64_t elapsed = hal_get_time_ms() - start_time;
            if (elapsed >= timeout_ms) {
                return COE_STATUS_TIMEOUT;
            }
        }

        /* Check for mailbox response */
        bool available = false;
        al_status_t al_status = al_mailbox_check(slave_address, &available);
        if (al_status == AL_STATUS_SUCCESS && available) {
            /* Receive response */
            mailbox_type_t type;
            uint8_t response_data[256];
            uint16_t response_length = sizeof(response_data);

            al_status = al_mailbox_receive(slave_address, &type,
                                           response_data, &response_length);
            if (al_status == AL_STATUS_SUCCESS && type == MBOX_TYPE_COE) {
                /* Parse CoE response */
                if (response_length >= COE_HEADER_SIZE + sizeof(sdo_download_init_res_t)) {
                    coe_header_t* res_header = (coe_header_t*)response_data;

                    if (res_header->service == COE_SERVICE_SDO_RESPONSE) {
                        sdo_download_init_res_t* sdo_res =
                            (sdo_download_init_res_t*)(response_data + COE_HEADER_SIZE);

                        sdo_command_byte_t* res_cmd = (sdo_command_byte_t*)&sdo_res->command;

                        /* Check for abort */
                        if (res_cmd->ccs == SDO_CCS_ABORT) {
                            return COE_STATUS_ABORT;
                        }

                        /* Check if response matches request */
                        if (sdo_res->index == index && sdo_res->subindex == subindex) {
                            init_response_received = true;
                            break;
                        }
                    }
                }
            }
        }

        /* Small delay to avoid busy waiting */
        hal_sleep_us(100);
    }

    /* Send data segments */
    uint32_t bytes_sent = 0;
    uint8_t toggle = 0;

    while (bytes_sent < size) {
        /* Calculate segment size (max 7 bytes per segment) */
        uint32_t segment_size = (size - bytes_sent > 7) ? 7 : (size - bytes_sent);
        bool last_segment = (bytes_sent + segment_size >= size);

        /* Build segment request */
        sdo_download_segment_req_t seg_req = {0};
        sdo_segment_command_byte_t* seg_cmd = (sdo_segment_command_byte_t*)&seg_req.command;
        seg_cmd->ccs = SDO_CCS_DOWNLOAD_SEGMENT;
        seg_cmd->toggle = toggle;
        seg_cmd->n = 7 - segment_size; /* Number of bytes that do NOT contain data */
        seg_cmd->c = last_segment ? 1 : 0;

        /* Copy segment data */
        memcpy(seg_req.data, data + bytes_sent, segment_size);

        /* Build mailbox message */
        mailbox_length = 0;
        memcpy(mailbox_data, &coe_header, COE_HEADER_SIZE);
        mailbox_length += COE_HEADER_SIZE;
        memcpy(mailbox_data + mailbox_length, &seg_req, sizeof(seg_req));
        mailbox_length += sizeof(seg_req);

        /* Send segment */
        mbx_req.data = mailbox_data;
        mbx_req.length = mailbox_length;

        al_status_t al_status = al_mailbox_send(&mbx_req);
        if (al_status != AL_STATUS_SUCCESS) {
            return COE_STATUS_ERROR;
        }

        /* Wait for segment response */
        start_time = hal_get_time_ms();
        bool seg_response_received = false;

        while (!seg_response_received) {
            /* Check timeout */
            if (timeout_ms > 0) {
                uint64_t elapsed = hal_get_time_ms() - start_time;
                if (elapsed >= timeout_ms) {
                    return COE_STATUS_TIMEOUT;
                }
            }

            /* Check for mailbox response */
            bool available = false;
            al_status = al_mailbox_check(slave_address, &available);
            if (al_status == AL_STATUS_SUCCESS && available) {
                /* Receive response */
                mailbox_type_t type;
                uint8_t response_data[256];
                uint16_t response_length = sizeof(response_data);

                al_status = al_mailbox_receive(slave_address, &type,
                                               response_data, &response_length);
                if (al_status == AL_STATUS_SUCCESS && type == MBOX_TYPE_COE) {
                    /* Parse CoE response */
                    if (response_length >= COE_HEADER_SIZE + sizeof(sdo_download_segment_res_t)) {
                        coe_header_t* res_header = (coe_header_t*)response_data;

                        if (res_header->service == COE_SERVICE_SDO_RESPONSE) {
                            sdo_download_segment_res_t* seg_res =
                                (sdo_download_segment_res_t*)(response_data + COE_HEADER_SIZE);

                            sdo_segment_command_byte_t* res_seg_cmd =
                                (sdo_segment_command_byte_t*)&seg_res->command;

                            /* Check for abort */
                            if (res_seg_cmd->ccs == SDO_CCS_ABORT) {
                                return COE_STATUS_ABORT;
                            }

                            /* Verify toggle bit */
                            if (res_seg_cmd->toggle != toggle) {
                                return COE_STATUS_ERROR;
                            }

                            seg_response_received = true;
                            break;
                        }
                    }
                }
            }

            /* Small delay to avoid busy waiting */
            hal_sleep_us(100);
        }

        /* Update progress */
        bytes_sent += segment_size;
        toggle = !toggle; /* Toggle bit for next segment */
    }

    return COE_STATUS_SUCCESS;
}

coe_status_t coe_sdo_download_expedited(uint16_t slave_address,
                                         uint16_t index,
                                         uint8_t subindex,
                                         uint32_t value,
                                         uint8_t size,
                                         uint32_t timeout_ms)
{
    if (!g_coe_context.initialized) {
        return COE_STATUS_ERROR;
    }

    if (size == 0 || size > 4) {
        return COE_STATUS_INVALID_PARAM;
    }

    /* Build CoE header */
    coe_header_t coe_header = {0};
    coe_header.number = 0;
    coe_header.service = COE_SERVICE_SDO_REQUEST;

    /* Build SDO Download Initiate request (expedited) */
    sdo_download_init_req_t sdo_req = {0};
    sdo_command_byte_t* cmd = (sdo_command_byte_t*)&sdo_req.command;
    cmd->ccs = SDO_CCS_DOWNLOAD_INIT;
    cmd->n = (4 - size) & 0x03; /* Number of bytes that do NOT contain data */
    cmd->e = 1; /* Expedited transfer */
    cmd->s = 1; /* Size indicated */

    sdo_req.index = index;
    sdo_req.subindex = subindex;
    sdo_req.data = value;

    /* Build mailbox message */
    uint8_t mailbox_data[256];
    uint16_t mailbox_length = 0;

    /* Copy CoE header */
    memcpy(mailbox_data, &coe_header, COE_HEADER_SIZE);
    mailbox_length += COE_HEADER_SIZE;

    /* Copy SDO request */
    memcpy(mailbox_data + mailbox_length, &sdo_req, sizeof(sdo_req));
    mailbox_length += sizeof(sdo_req);

    /* Send mailbox message */
    mbx_send_req_t mbx_req = {
        .slave_address = slave_address,
        .type = MBOX_TYPE_COE,
        .data = mailbox_data,
        .length = mailbox_length,
        .priority = 0,
        .user_data = NULL
    };

    al_status_t al_status = al_mailbox_send(&mbx_req);
    if (al_status != AL_STATUS_SUCCESS) {
        return COE_STATUS_ERROR;
    }

    /* Wait for response with timeout */
    uint64_t start_time = hal_get_time_ms();
    bool response_received = false;

    while (!response_received) {
        /* Check timeout */
        if (timeout_ms > 0) {
            uint64_t elapsed = hal_get_time_ms() - start_time;
            if (elapsed >= timeout_ms) {
                return COE_STATUS_TIMEOUT;
            }
        }

        /* Check for mailbox response */
        bool available = false;
        al_status = al_mailbox_check(slave_address, &available);
        if (al_status == AL_STATUS_SUCCESS && available) {
            /* Receive response */
            mailbox_type_t type;
            uint8_t response_data[256];
            uint16_t response_length = sizeof(response_data);

            al_status = al_mailbox_receive(slave_address, &type,
                                           response_data, &response_length);
            if (al_status == AL_STATUS_SUCCESS && type == MBOX_TYPE_COE) {
                /* Parse CoE response */
                if (response_length >= COE_HEADER_SIZE + sizeof(sdo_download_init_res_t)) {
                    coe_header_t* res_header = (coe_header_t*)response_data;

                    if (res_header->service == COE_SERVICE_SDO_RESPONSE) {
                        sdo_download_init_res_t* sdo_res =
                            (sdo_download_init_res_t*)(response_data + COE_HEADER_SIZE);

                        sdo_command_byte_t* res_cmd = (sdo_command_byte_t*)&sdo_res->command;

                        /* Check for abort */
                        if (res_cmd->ccs == SDO_CCS_ABORT) {
                            /* SDO aborted */
                            return COE_STATUS_ABORT;
                        }

                        /* Check if response matches request */
                        if (sdo_res->index == index && sdo_res->subindex == subindex) {
                            response_received = true;
                            return COE_STATUS_SUCCESS;
                        }
                    }
                }
            }
        }

        /* Small delay to avoid busy waiting */
        hal_sleep_us(100);
    }

    return COE_STATUS_TIMEOUT;
}

/* ========================================================================== */
/* SDO Upload (Read from Object Dictionary)                                 */
/* ========================================================================== */

coe_status_t coe_sdo_upload(uint16_t slave_address,
                             uint16_t index,
                             uint8_t subindex,
                             uint8_t* data,
                             uint32_t* size,
                             uint32_t timeout_ms)
{
    if (!g_coe_context.initialized) {
        return COE_STATUS_ERROR;
    }

    if (data == NULL || size == NULL || *size == 0) {
        return COE_STATUS_INVALID_PARAM;
    }

    /* Build CoE header */
    coe_header_t coe_header = {0};
    coe_header.number = 0;
    coe_header.service = COE_SERVICE_SDO_REQUEST;

    /* Build SDO Upload Initiate request */
    sdo_upload_init_req_t sdo_req = {0};
    sdo_command_byte_t* cmd = (sdo_command_byte_t*)&sdo_req.command;
    cmd->ccs = SDO_CCS_UPLOAD_INIT;
    cmd->n = 0;
    cmd->e = 0;
    cmd->s = 0;

    sdo_req.index = index;
    sdo_req.subindex = subindex;
    sdo_req.reserved = 0;

    /* Build mailbox message */
    uint8_t mailbox_data[256];
    uint16_t mailbox_length = 0;

    /* Copy CoE header */
    memcpy(mailbox_data, &coe_header, COE_HEADER_SIZE);
    mailbox_length += COE_HEADER_SIZE;

    /* Copy SDO request */
    memcpy(mailbox_data + mailbox_length, &sdo_req, sizeof(sdo_req));
    mailbox_length += sizeof(sdo_req);

    /* Send mailbox message */
    mbx_send_req_t mbx_req = {
        .slave_address = slave_address,
        .type = MBOX_TYPE_COE,
        .data = mailbox_data,
        .length = mailbox_length,
        .priority = 0,
        .user_data = NULL
    };

    al_status_t al_status = al_mailbox_send(&mbx_req);
    if (al_status != AL_STATUS_SUCCESS) {
        return COE_STATUS_ERROR;
    }

    /* Wait for response with timeout */
    uint64_t start_time = hal_get_time_ms();
    bool response_received = false;

    while (!response_received) {
        /* Check timeout */
        if (timeout_ms > 0) {
            uint64_t elapsed = hal_get_time_ms() - start_time;
            if (elapsed >= timeout_ms) {
                return COE_STATUS_TIMEOUT;
            }
        }

        /* Check for mailbox response */
        bool available = false;
        al_status = al_mailbox_check(slave_address, &available);
        if (al_status == AL_STATUS_SUCCESS && available) {
            /* Receive response */
            mailbox_type_t type;
            uint8_t response_data[256];
            uint16_t response_length = sizeof(response_data);

            al_status = al_mailbox_receive(slave_address, &type,
                                           response_data, &response_length);
            if (al_status == AL_STATUS_SUCCESS && type == MBOX_TYPE_COE) {
                /* Parse CoE response */
                if (response_length >= COE_HEADER_SIZE + sizeof(sdo_upload_init_res_t)) {
                    coe_header_t* res_header = (coe_header_t*)response_data;

                    if (res_header->service == COE_SERVICE_SDO_RESPONSE) {
                        sdo_upload_init_res_t* sdo_res =
                            (sdo_upload_init_res_t*)(response_data + COE_HEADER_SIZE);

                        sdo_command_byte_t* res_cmd = (sdo_command_byte_t*)&sdo_res->command;

                        /* Check for abort */
                        if (res_cmd->ccs == SDO_CCS_ABORT) {
                            return COE_STATUS_ABORT;
                        }

                        /* Check if response matches request */
                        if (sdo_res->index == index && sdo_res->subindex == subindex) {
                            /* Check if expedited transfer */
                            if (res_cmd->e == 1) {
                                /* Expedited transfer - data in response */
                                uint8_t data_size = 4 - res_cmd->n;
                                if (data_size > *size) {
                                    return COE_STATUS_ERROR;
                                }
                                memcpy(data, &sdo_res->data, data_size);
                                *size = data_size;
                                response_received = true;
                                return COE_STATUS_SUCCESS;
                            } else {
                                /* Normal (segmented) transfer */
                                uint32_t total_size = sdo_res->data;
                                if (total_size > *size) {
                                    return COE_STATUS_ERROR;
                                }
                                response_received = true;
                                break;
                            }
                        }
                    }
                }
            }
        }

        /* Small delay to avoid busy waiting */
        hal_sleep_us(100);
    }

    /* Receive data segments */
    uint32_t bytes_received = 0;
    uint8_t toggle = 0;
    bool last_segment = false;

    while (!last_segment && bytes_received < *size) {
        /* Build segment upload request */
        coe_header_t coe_header = {0};
        coe_header.number = 0;
        coe_header.service = COE_SERVICE_SDO_REQUEST;

        sdo_upload_segment_req_t seg_req = {0};
        sdo_segment_command_byte_t* seg_cmd = (sdo_segment_command_byte_t*)&seg_req.command;
        seg_cmd->ccs = SDO_CCS_UPLOAD_SEGMENT;
        seg_cmd->toggle = toggle;

        /* Build mailbox message */
        uint8_t mailbox_data[256];
        uint16_t mailbox_length = 0;

        memcpy(mailbox_data, &coe_header, COE_HEADER_SIZE);
        mailbox_length += COE_HEADER_SIZE;
        memcpy(mailbox_data + mailbox_length, &seg_req, sizeof(seg_req));
        mailbox_length += sizeof(seg_req);

        /* Send segment request */
        mbx_send_req_t mbx_req = {
            .slave_address = slave_address,
            .type = MBOX_TYPE_COE,
            .data = mailbox_data,
            .length = mailbox_length,
            .priority = 0,
            .user_data = NULL
        };

        al_status_t al_status = al_mailbox_send(&mbx_req);
        if (al_status != AL_STATUS_SUCCESS) {
            return COE_STATUS_ERROR;
        }

        /* Wait for segment response */
        start_time = hal_get_time_ms();
        bool seg_response_received = false;

        while (!seg_response_received) {
            /* Check timeout */
            if (timeout_ms > 0) {
                uint64_t elapsed = hal_get_time_ms() - start_time;
                if (elapsed >= timeout_ms) {
                    return COE_STATUS_TIMEOUT;
                }
            }

            /* Check for mailbox response */
            bool available = false;
            al_status = al_mailbox_check(slave_address, &available);
            if (al_status == AL_STATUS_SUCCESS && available) {
                /* Receive response */
                mailbox_type_t type;
                uint8_t response_data[256];
                uint16_t response_length = sizeof(response_data);

                al_status = al_mailbox_receive(slave_address, &type,
                                               response_data, &response_length);
                if (al_status == AL_STATUS_SUCCESS && type == MBOX_TYPE_COE) {
                    /* Parse CoE response */
                    if (response_length >= COE_HEADER_SIZE + sizeof(sdo_upload_segment_res_t)) {
                        coe_header_t* res_header = (coe_header_t*)response_data;

                        if (res_header->service == COE_SERVICE_SDO_RESPONSE) {
                            sdo_upload_segment_res_t* seg_res =
                                (sdo_upload_segment_res_t*)(response_data + COE_HEADER_SIZE);

                            sdo_segment_command_byte_t* res_seg_cmd =
                                (sdo_segment_command_byte_t*)&seg_res->command;

                            /* Check for abort */
                            if (res_seg_cmd->ccs == SDO_CCS_ABORT) {
                                return COE_STATUS_ABORT;
                            }

                            /* Verify toggle bit */
                            if (res_seg_cmd->toggle != toggle) {
                                return COE_STATUS_ERROR;
                            }

                            /* Calculate segment data size */
                            uint8_t segment_size = 7 - res_seg_cmd->n;

                            /* Check buffer overflow */
                            if (bytes_received + segment_size > *size) {
                                return COE_STATUS_ERROR;
                            }

                            /* Copy segment data */
                            memcpy(data + bytes_received, seg_res->data, segment_size);
                            bytes_received += segment_size;

                            /* Check if last segment */
                            last_segment = (res_seg_cmd->c == 1);

                            seg_response_received = true;
                            break;
                        }
                    }
                }
            }

            /* Small delay to avoid busy waiting */
            hal_sleep_us(100);
        }

        /* Toggle bit for next segment */
        toggle = !toggle;
    }

    *size = bytes_received;
    return COE_STATUS_SUCCESS;
}

coe_status_t coe_sdo_upload_expedited(uint16_t slave_address,
                                       uint16_t index,
                                       uint8_t subindex,
                                       uint32_t* value,
                                       uint8_t* size,
                                       uint32_t timeout_ms)
{
    if (!g_coe_context.initialized) {
        return COE_STATUS_ERROR;
    }

    if (value == NULL || size == NULL) {
        return COE_STATUS_INVALID_PARAM;
    }

    uint8_t data[4];
    uint32_t data_size = 4;

    coe_status_t status = coe_sdo_upload(slave_address, index, subindex,
                                         data, &data_size, timeout_ms);
    if (status != COE_STATUS_SUCCESS) {
        return status;
    }

    if (data_size > 4) {
        return COE_STATUS_ERROR;
    }

    *value = 0;
    memcpy(value, data, data_size);
    *size = (uint8_t)data_size;

    return COE_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Utility Functions                                                         */
/* ========================================================================== */

const char* coe_get_abort_code_string(sdo_abort_code_t abort_code)
{
    switch (abort_code) {
        case SDO_ABORT_TOGGLE_BIT:
            return "Toggle bit not changed";
        case SDO_ABORT_TIMEOUT:
            return "SDO protocol timeout";
        case SDO_ABORT_INVALID_COMMAND:
            return "Invalid command specifier";
        case SDO_ABORT_OUT_OF_MEMORY:
            return "Out of memory";
        case SDO_ABORT_UNSUPPORTED_ACCESS:
            return "Unsupported access";
        case SDO_ABORT_WRITE_ONLY:
            return "Write only object";
        case SDO_ABORT_READ_ONLY:
            return "Read only object";
        case SDO_ABORT_OBJECT_NOT_EXIST:
            return "Object does not exist";
        case SDO_ABORT_OBJECT_CANNOT_MAP:
            return "Object cannot be mapped to PDO";
        case SDO_ABORT_PDO_LENGTH_EXCEEDED:
            return "PDO length exceeded";
        case SDO_ABORT_PARAMETER_INCOMPATIBLE:
            return "Parameter incompatibility";
        case SDO_ABORT_HARDWARE_ERROR:
            return "Hardware error";
        case SDO_ABORT_DATA_TYPE_MISMATCH:
            return "Data type mismatch";
        case SDO_ABORT_DATA_TYPE_LENGTH_HIGH:
            return "Data type length too high";
        case SDO_ABORT_DATA_TYPE_LENGTH_LOW:
            return "Data type length too low";
        case SDO_ABORT_SUBINDEX_NOT_EXIST:
            return "Subindex does not exist";
        case SDO_ABORT_VALUE_RANGE_EXCEEDED:
            return "Value range exceeded";
        case SDO_ABORT_VALUE_TOO_HIGH:
            return "Value too high";
        case SDO_ABORT_VALUE_TOO_LOW:
            return "Value too low";
        case SDO_ABORT_GENERAL_ERROR:
            return "General error";
        case SDO_ABORT_DATA_CANNOT_TRANSFER:
            return "Data cannot be transferred";
        case SDO_ABORT_DATA_CANNOT_TRANSFER_LOCAL:
            return "Data cannot be transferred (local control)";
        case SDO_ABORT_DATA_CANNOT_TRANSFER_STATE:
            return "Data cannot be transferred (device state)";
        case SDO_ABORT_NO_OBJECT_DICTIONARY:
            return "No object dictionary present";
        case SDO_ABORT_NO_DATA_AVAILABLE:
            return "No data available";
        default:
            return "Unknown abort code";
    }
}

const char* coe_get_version(void)
{
    return "1.0.0";
}
