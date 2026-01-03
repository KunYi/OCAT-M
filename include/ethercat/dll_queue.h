/**
 * @file dll_queue.h
 * @brief EtherCAT Data Link Layer - Queue Management Interface
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Based on ETG1000.3 - EtherCAT Data Link Layer Services
 *
 * This file contains the queue management interface for TX and RX queues.
 * Implements circular buffer with priority support for TX queue.
 */

#ifndef ETHERCAT_DLL_QUEUE_H
#define ETHERCAT_DLL_QUEUE_H

#include "dll_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup DLL_Queue Data Link Layer Queue Management
 * @{
 */

/* ========================================================================== */
/* Queue Handle                                                               */
/* ========================================================================== */

/** Opaque queue handle */
typedef struct dl_queue_s* dl_queue_handle_t;

/* ========================================================================== */
/* Queue Creation and Destruction                                             */
/* ========================================================================== */

/**
 * @brief Create a queue
 *
 * Creates a circular buffer queue with the specified capacity.
 *
 * @param capacity Maximum number of entries in the queue
 * @param enable_priority Enable priority ordering (for TX queue)
 * @return Queue handle on success, NULL on failure
 */
dl_queue_handle_t dl_queue_create(uint16_t capacity, bool enable_priority);

/**
 * @brief Destroy a queue
 *
 * Destroys the queue and frees all resources.
 * All entries in the queue are discarded.
 *
 * @param queue Queue handle
 */
void dl_queue_destroy(dl_queue_handle_t queue);

/* ========================================================================== */
/* Queue Operations                                                           */
/* ========================================================================== */

/**
 * @brief Enqueue an entry
 *
 * Adds an entry to the queue. If priority is enabled, the entry
 * is inserted according to its priority level.
 *
 * @param queue Queue handle
 * @param entry Pointer to entry to enqueue
 * @return DL_STATUS_SUCCESS on success, DL_STATUS_ERROR if queue is full
 */
dl_status_t dl_queue_enqueue(dl_queue_handle_t queue, const dl_queue_entry_t* entry);

/**
 * @brief Dequeue an entry
 *
 * Removes and returns the next entry from the queue.
 * For priority queues, returns the highest priority entry.
 *
 * @param queue Queue handle
 * @param entry Pointer to buffer for dequeued entry
 * @return DL_STATUS_SUCCESS on success, DL_STATUS_ERROR if queue is empty
 */
dl_status_t dl_queue_dequeue(dl_queue_handle_t queue, dl_queue_entry_t* entry);

/**
 * @brief Peek at next entry without removing it
 *
 * Returns the next entry that would be dequeued, without removing it.
 *
 * @param queue Queue handle
 * @param entry Pointer to buffer for peeked entry
 * @return DL_STATUS_SUCCESS on success, DL_STATUS_ERROR if queue is empty
 */
dl_status_t dl_queue_peek(dl_queue_handle_t queue, dl_queue_entry_t* entry);

/**
 * @brief Flush queue
 *
 * Removes all entries from the queue.
 *
 * @param queue Queue handle
 * @return DL_STATUS_SUCCESS on success, error code otherwise
 */
dl_status_t dl_queue_flush(dl_queue_handle_t queue);

/* ========================================================================== */
/* Queue Status                                                               */
/* ========================================================================== */

/**
 * @brief Get number of entries in queue
 *
 * @param queue Queue handle
 * @return Number of entries currently in the queue
 */
uint16_t dl_queue_count(dl_queue_handle_t queue);

/**
 * @brief Get queue capacity
 *
 * @param queue Queue handle
 * @return Maximum number of entries the queue can hold
 */
uint16_t dl_queue_capacity(dl_queue_handle_t queue);

/**
 * @brief Check if queue is empty
 *
 * @param queue Queue handle
 * @return true if queue is empty, false otherwise
 */
bool dl_queue_is_empty(dl_queue_handle_t queue);

/**
 * @brief Check if queue is full
 *
 * @param queue Queue handle
 * @return true if queue is full, false otherwise
 */
bool dl_queue_is_full(dl_queue_handle_t queue);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ETHERCAT_DLL_QUEUE_H */
