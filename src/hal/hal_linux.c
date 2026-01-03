/**
 * @file hal_linux.c
 * @brief HAL Linux Raw Socket Implementation
 * @version 1.0.0
 * @date 2026-01-03
 *
 * This implementation uses Linux raw sockets (AF_PACKET) for frame
 * transmission and reception. It provides direct access to the Ethernet
 * layer without going through the kernel's network stack.
 */

/* Need _GNU_SOURCE for struct ifreq and other Linux-specific features */
#ifdef __linux__
#define _GNU_SOURCE
#endif

#include <string.h>
#include <stdlib.h>

#ifdef __linux__
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#endif

#include "hal_internal.h"

/* ========================================================================== */
/* Linux Platform Context                                                     */
/* ========================================================================== */

#ifdef __linux__
typedef struct {
    int socket_fd;
    int ifindex;
    struct sockaddr_ll socket_address;
    hal_frame_buffer_t* tx_buffers[32];
    hal_frame_buffer_t* rx_buffers[32];
    uint32_t tx_buffer_count;
    uint32_t rx_buffer_count;
} hal_linux_context_t;
#endif

/* ========================================================================== */
/* Linux Platform Operations                                                  */
/* ========================================================================== */

#ifdef __linux__

static hal_status_t hal_linux_init(hal_context_t* ctx)
{
    if (ctx == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    /* Allocate Linux context */
    hal_linux_context_t* linux_ctx = (hal_linux_context_t*)calloc(1, sizeof(hal_linux_context_t));
    if (linux_ctx == NULL) {
        return HAL_STATUS_ERROR;
    }

    /* Create raw socket */
    linux_ctx->socket_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (linux_ctx->socket_fd < 0) {
        free(linux_ctx);
        return HAL_STATUS_ERROR;
    }

    /* Get interface index */
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ctx->config.interface_name, IF_NAMESIZE - 1);

    if (ioctl(linux_ctx->socket_fd, SIOCGIFINDEX, &ifr) < 0) {
        close(linux_ctx->socket_fd);
        free(linux_ctx);
        return HAL_STATUS_NO_DEVICE;
    }
    linux_ctx->ifindex = ifr.ifr_ifindex;

    /* Get MAC address */
    if (ioctl(linux_ctx->socket_fd, SIOCGIFHWADDR, &ifr) < 0) {
        close(linux_ctx->socket_fd);
        free(linux_ctx);
        return HAL_STATUS_ERROR;
    }
    memcpy(ctx->config.mac_address, ifr.ifr_hwaddr.sa_data, 6);

    /* Set promiscuous mode if requested */
    if (ctx->config.promiscuous_mode) {
        struct packet_mreq mreq;
        memset(&mreq, 0, sizeof(mreq));
        mreq.mr_ifindex = linux_ctx->ifindex;
        mreq.mr_type = PACKET_MR_PROMISC;

        if (setsockopt(linux_ctx->socket_fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
                       &mreq, sizeof(mreq)) < 0) {
            close(linux_ctx->socket_fd);
            free(linux_ctx);
            return HAL_STATUS_ERROR;
        }
    }

    /* Set non-blocking mode if requested */
    if (!ctx->config.blocking_mode) {
        int flags = fcntl(linux_ctx->socket_fd, F_GETFL, 0);
        if (flags < 0 || fcntl(linux_ctx->socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            close(linux_ctx->socket_fd);
            free(linux_ctx);
            return HAL_STATUS_ERROR;
        }
    }

    /* Bind socket to interface */
    memset(&linux_ctx->socket_address, 0, sizeof(linux_ctx->socket_address));
    linux_ctx->socket_address.sll_family = AF_PACKET;
    linux_ctx->socket_address.sll_protocol = htons(ETH_P_ALL);
    linux_ctx->socket_address.sll_ifindex = linux_ctx->ifindex;

    if (bind(linux_ctx->socket_fd, (struct sockaddr*)&linux_ctx->socket_address,
             sizeof(linux_ctx->socket_address)) < 0) {
        close(linux_ctx->socket_fd);
        free(linux_ctx);
        return HAL_STATUS_ERROR;
    }

    ctx->platform_context = linux_ctx;

    /* Initialize device info */
    strncpy(ctx->device_info.interface_name, ctx->config.interface_name,
            sizeof(ctx->device_info.interface_name) - 1);
    memcpy(ctx->device_info.mac_address, ctx->config.mac_address, 6);
    ctx->device_info.mtu = 1500;
    ctx->device_info.speed_mbps = 1000;
    ctx->device_info.link_up = true;
    ctx->device_info.full_duplex = true;
    ctx->device_info.platform = HAL_PLATFORM_LINUX_RAW_SOCKET;

    return HAL_STATUS_SUCCESS;
}

static hal_status_t hal_linux_shutdown(hal_context_t* ctx)
{
    if (ctx == NULL || ctx->platform_context == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    hal_linux_context_t* linux_ctx = (hal_linux_context_t*)ctx->platform_context;

    /* Free all allocated buffers */
    for (uint32_t i = 0; i < linux_ctx->tx_buffer_count; i++) {
        if (linux_ctx->tx_buffers[i] != NULL) {
            free(linux_ctx->tx_buffers[i]->data);
            free(linux_ctx->tx_buffers[i]);
        }
    }

    for (uint32_t i = 0; i < linux_ctx->rx_buffer_count; i++) {
        if (linux_ctx->rx_buffers[i] != NULL) {
            free(linux_ctx->rx_buffers[i]->data);
            free(linux_ctx->rx_buffers[i]);
        }
    }

    /* Close socket */
    if (linux_ctx->socket_fd >= 0) {
        close(linux_ctx->socket_fd);
    }

    free(linux_ctx);
    ctx->platform_context = NULL;

    return HAL_STATUS_SUCCESS;
}

static hal_status_t hal_linux_send_frame(hal_context_t* ctx, hal_frame_buffer_t* buffer)
{
    if (ctx == NULL || buffer == NULL || ctx->platform_context == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    hal_linux_context_t* linux_ctx = (hal_linux_context_t*)ctx->platform_context;

    ssize_t sent = sendto(linux_ctx->socket_fd, buffer->data, buffer->length, 0,
                          (struct sockaddr*)&linux_ctx->socket_address,
                          sizeof(linux_ctx->socket_address));

    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return HAL_STATUS_WOULD_BLOCK;
        }
        return HAL_STATUS_ERROR;
    }

    if ((size_t)sent != buffer->length) {
        return HAL_STATUS_ERROR;
    }

    return HAL_STATUS_SUCCESS;
}

static hal_status_t hal_linux_receive_frame(hal_context_t* ctx, hal_frame_buffer_t** buffer)
{
    if (ctx == NULL || buffer == NULL || ctx->platform_context == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    hal_linux_context_t* linux_ctx = (hal_linux_context_t*)ctx->platform_context;

    /* Allocate RX buffer if needed */
    hal_frame_buffer_t* rx_buf = (hal_frame_buffer_t*)calloc(1, sizeof(hal_frame_buffer_t));
    if (rx_buf == NULL) {
        return HAL_STATUS_NO_BUFFER;
    }

    rx_buf->data = (uint8_t*)malloc(ctx->config.rx_buffer_size);
    if (rx_buf->data == NULL) {
        free(rx_buf);
        return HAL_STATUS_NO_BUFFER;
    }

    rx_buf->capacity = ctx->config.rx_buffer_size;

    /* Receive frame */
    ssize_t received = recvfrom(linux_ctx->socket_fd, rx_buf->data, rx_buf->capacity,
                                0, NULL, NULL);

    if (received < 0) {
        free(rx_buf->data);
        free(rx_buf);

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return HAL_STATUS_WOULD_BLOCK;
        }
        return HAL_STATUS_ERROR;
    }

    rx_buf->length = (uint16_t)received;
    rx_buf->timestamp = 0;  /* TODO: Get timestamp */
    rx_buf->port = 0;

    /* Store in RX buffer list */
    if (linux_ctx->rx_buffer_count < 32) {
        linux_ctx->rx_buffers[linux_ctx->rx_buffer_count++] = rx_buf;
    }

    *buffer = rx_buf;
    return HAL_STATUS_SUCCESS;
}

static hal_status_t hal_linux_alloc_tx_buffer(hal_context_t* ctx, uint16_t size,
                                                hal_frame_buffer_t** buffer)
{
    if (ctx == NULL || buffer == NULL || size == 0 || ctx->platform_context == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    hal_linux_context_t* linux_ctx = (hal_linux_context_t*)ctx->platform_context;

    if (linux_ctx->tx_buffer_count >= 32) {
        return HAL_STATUS_NO_BUFFER;
    }

    /* Allocate buffer structure */
    hal_frame_buffer_t* buf = (hal_frame_buffer_t*)calloc(1, sizeof(hal_frame_buffer_t));
    if (buf == NULL) {
        return HAL_STATUS_ERROR;
    }

    /* Allocate data buffer */
    buf->data = (uint8_t*)malloc(size);
    if (buf->data == NULL) {
        free(buf);
        return HAL_STATUS_ERROR;
    }

    buf->length = 0;
    buf->capacity = size;
    buf->timestamp = 0;
    buf->port = 0;
    buf->user_data = NULL;
    buf->hal_private = NULL;

    linux_ctx->tx_buffers[linux_ctx->tx_buffer_count++] = buf;
    *buffer = buf;

    return HAL_STATUS_SUCCESS;
}

static hal_status_t hal_linux_free_tx_buffer(hal_context_t* ctx, hal_frame_buffer_t* buffer)
{
    if (ctx == NULL || buffer == NULL || ctx->platform_context == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    hal_linux_context_t* linux_ctx = (hal_linux_context_t*)ctx->platform_context;

    /* Find and remove buffer from list */
    for (uint32_t i = 0; i < linux_ctx->tx_buffer_count; i++) {
        if (linux_ctx->tx_buffers[i] == buffer) {
            free(buffer->data);
            free(buffer);

            /* Shift remaining buffers */
            for (uint32_t j = i; j < linux_ctx->tx_buffer_count - 1; j++) {
                linux_ctx->tx_buffers[j] = linux_ctx->tx_buffers[j + 1];
            }
            linux_ctx->tx_buffer_count--;
            return HAL_STATUS_SUCCESS;
        }
    }

    return HAL_STATUS_ERROR;
}

static hal_status_t hal_linux_free_rx_buffer(hal_context_t* ctx, hal_frame_buffer_t* buffer)
{
    if (ctx == NULL || buffer == NULL || ctx->platform_context == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    hal_linux_context_t* linux_ctx = (hal_linux_context_t*)ctx->platform_context;

    /* Find and remove buffer from list */
    for (uint32_t i = 0; i < linux_ctx->rx_buffer_count; i++) {
        if (linux_ctx->rx_buffers[i] == buffer) {
            free(buffer->data);
            free(buffer);

            /* Shift remaining buffers */
            for (uint32_t j = i; j < linux_ctx->rx_buffer_count - 1; j++) {
                linux_ctx->rx_buffers[j] = linux_ctx->rx_buffers[j + 1];
            }
            linux_ctx->rx_buffer_count--;
            return HAL_STATUS_SUCCESS;
        }
    }

    return HAL_STATUS_ERROR;
}

static hal_status_t hal_linux_get_device_info(hal_context_t* ctx, hal_device_info_t* info)
{
    if (ctx == NULL || info == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    memcpy(info, &ctx->device_info, sizeof(hal_device_info_t));
    return HAL_STATUS_SUCCESS;
}

static hal_status_t hal_linux_set_promiscuous_mode(hal_context_t* ctx, bool enable)
{
    if (ctx == NULL || ctx->platform_context == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    hal_linux_context_t* linux_ctx = (hal_linux_context_t*)ctx->platform_context;

    struct packet_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    mreq.mr_ifindex = linux_ctx->ifindex;
    mreq.mr_type = PACKET_MR_PROMISC;

    int opt = enable ? PACKET_ADD_MEMBERSHIP : PACKET_DROP_MEMBERSHIP;

    if (setsockopt(linux_ctx->socket_fd, SOL_PACKET, opt, &mreq, sizeof(mreq)) < 0) {
        return HAL_STATUS_ERROR;
    }

    return HAL_STATUS_SUCCESS;
}

static hal_status_t hal_linux_flush_tx_buffers(hal_context_t* ctx)
{
    if (ctx == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    /* Linux raw socket doesn't buffer TX frames */
    return HAL_STATUS_SUCCESS;
}

static hal_status_t hal_linux_flush_rx_buffers(hal_context_t* ctx)
{
    if (ctx == NULL || ctx->platform_context == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    hal_linux_context_t* linux_ctx = (hal_linux_context_t*)ctx->platform_context;

    /* Drain socket receive buffer */
    uint8_t drain_buffer[2048];
    while (recv(linux_ctx->socket_fd, drain_buffer, sizeof(drain_buffer),
                MSG_DONTWAIT) > 0) {
        /* Discard received data */
    }

    return HAL_STATUS_SUCCESS;
}

/* ========================================================================== */
/* Linux Time Functions                                                       */
/* ========================================================================== */

static uint64_t hal_linux_get_time_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static uint64_t hal_linux_get_time_ms(void)
{
    return hal_linux_get_time_ns() / 1000000ULL;
}

static void hal_linux_sleep_ms(uint32_t ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

static void hal_linux_sleep_us(uint32_t us)
{
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

#endif /* __linux__ */

/* ========================================================================== */
/* Linux Platform Operations Table                                            */
/* ========================================================================== */

#ifdef __linux__
const hal_platform_ops_t hal_linux_raw_socket_ops = {
    .init = hal_linux_init,
    .shutdown = hal_linux_shutdown,
    .send_frame = hal_linux_send_frame,
    .receive_frame = hal_linux_receive_frame,
    .alloc_tx_buffer = hal_linux_alloc_tx_buffer,
    .free_tx_buffer = hal_linux_free_tx_buffer,
    .free_rx_buffer = hal_linux_free_rx_buffer,
    .get_device_info = hal_linux_get_device_info,
    .set_promiscuous_mode = hal_linux_set_promiscuous_mode,
    .flush_tx_buffers = hal_linux_flush_tx_buffers,
    .flush_rx_buffers = hal_linux_flush_rx_buffers,
    .get_time_ns = hal_linux_get_time_ns,
    .get_time_ms = hal_linux_get_time_ms,
    .sleep_ms = hal_linux_sleep_ms,
    .sleep_us = hal_linux_sleep_us
};
#else
/* Stub implementation for non-Linux platforms */
const hal_platform_ops_t hal_linux_raw_socket_ops = {
    .init = NULL,
    .shutdown = NULL,
    .send_frame = NULL,
    .receive_frame = NULL,
    .alloc_tx_buffer = NULL,
    .free_tx_buffer = NULL,
    .free_rx_buffer = NULL,
    .get_device_info = NULL,
    .set_promiscuous_mode = NULL,
    .flush_tx_buffers = NULL,
    .flush_rx_buffers = NULL,
    .get_time_ns = NULL,
    .get_time_ms = NULL,
    .sleep_ms = NULL,
    .sleep_us = NULL
};
#endif
