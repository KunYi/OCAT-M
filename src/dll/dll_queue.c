/**
 * @file dll_queue.c
 * @brief EtherCAT Data Link Layer - Queue Management Implementation
 * @version 1.0.0
 * @date 2026-01-03
 *
 * Implements circular buffer queue with optional priority support.
 * For priority queues, uses insertion sort to maintain priority order.
 */

#include "ethercat/dll_queue.h"
#include "ethercat/dll_errors.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================== */
/* Queue Structure                                                            */
/* ========================================================================== */

/**
 * @brief Queue structure (circular buffer)
 */
typedef struct dl_queue_s {
    dl_queue_entry_t* entries;      /**< Array of queue entries */
    uint16_t capacity;              /**< Maximum number of entries */
    uint16_t count;                 /**< Current number of entries */
    uint16_t head;                  /**< Head index (dequeue position) */
    uint16_t tail;                  /**< Tail index (enqueue position) */
    bool enable_priority;           /**< Enable priority ordering */
} dl_queue_t;

/* ========================================================================== */
/* Private Functions                                                          */
/* ========================================================================== */

/**
 * @brief Insert entry with priority ordering
 *
 * Inserts an entry into the queue maintaining priority order.
 * Higher priority values are dequeued first.
 *
 * @param queue Queue handle
 * @param entry Entry to insert
 */
static void insert_with_priority(dl_queue_t* queue, const dl_queue_entry_t* entry)
{
    /* If queue is empty, just add at tail */
    if (queue->count == 0) {
        queue->entries[queue->tail] = *entry;
        queue->tail = (queue->tail + 1) % queue->capacity;
        queue->count++;
        return;
    }

    /* Find insertion position (from tail backwards) */
    uint16_t insert_pos = queue->tail;
    uint16_t search_pos = (queue->tail + queue->capacity - 1) % queue->capacity;

    /* Search backwards from tail to find correct position */
    for (uint16_t i = 0; i < queue->count; i++) {
        if (queue->entries[search_pos].priority >= entry->priority) {
            /* Found position - insert after this entry */
            insert_pos = (search_pos + 1) % queue->capacity;
            break;
        }

        /* Move to previous entry */
        if (search_pos == queue->head) {
            /* Reached head - insert at head */
            insert_pos = queue->head;
            break;
        }
        search_pos = (search_pos + queue->capacity - 1) % queue->capacity;
    }

    /* Shift entries to make room */
    if (insert_pos != queue->tail) {
        uint16_t shift_pos = queue->tail;
        while (shift_pos != insert_pos) {
            uint16_t prev_pos = (shift_pos + queue->capacity - 1) % queue->capacity;
            queue->entries[shift_pos] = queue->entries[prev_pos];
            shift_pos = prev_pos;
        }
    }

    /* Insert entry */
    queue->entries[insert_pos] = *entry;
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
}

/* ========================================================================== */
/* Public Functions                                                           */
/* ========================================================================== */

dl_queue_handle_t dl_queue_create(uint16_t capacity, bool enable_priority)
{
    if (capacity == 0) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "dl_queue_create: capacity is zero");
        return NULL;
    }

    /* Allocate queue structure */
    dl_queue_t* queue = (dl_queue_t*)malloc(sizeof(dl_queue_t));
    if (queue == NULL) {
        dl_set_error(DL_ERROR_NO_MEMORY, "dl_queue_create: failed to allocate queue");
        return NULL;
    }

    /* Allocate entry array */
    queue->entries = (dl_queue_entry_t*)malloc(sizeof(dl_queue_entry_t) * capacity);
    if (queue->entries == NULL) {
        free(queue);
        dl_set_error(DL_ERROR_NO_MEMORY, "dl_queue_create: failed to allocate entries");
        return NULL;
    }

    /* Initialize queue */
    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->enable_priority = enable_priority;

    return queue;
}

void dl_queue_destroy(dl_queue_handle_t queue)
{
    if (queue == NULL) {
        return;
    }

    dl_queue_t* q = (dl_queue_t*)queue;

    /* Free entry array */
    if (q->entries != NULL) {
        free(q->entries);
    }

    /* Free queue structure */
    free(q);
}

dl_status_t dl_queue_enqueue(dl_queue_handle_t queue, const dl_queue_entry_t* entry)
{
    if (queue == NULL || entry == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "dl_queue_enqueue: invalid parameter");
        return DL_STATUS_INVALID_PARAM;
    }

    dl_queue_t* q = (dl_queue_t*)queue;

    /* Check if queue is full */
    if (q->count >= q->capacity) {
        dl_set_error(DL_ERROR_QUEUE_FULL, "dl_queue_enqueue: queue is full");
        return DL_STATUS_ERROR;
    }

    /* Insert with or without priority */
    if (q->enable_priority) {
        insert_with_priority(q, entry);
    } else {
        /* Simple circular buffer insertion */
        q->entries[q->tail] = *entry;
        q->tail = (q->tail + 1) % q->capacity;
        q->count++;
    }

    return DL_STATUS_SUCCESS;
}

dl_status_t dl_queue_dequeue(dl_queue_handle_t queue, dl_queue_entry_t* entry)
{
    if (queue == NULL || entry == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "dl_queue_dequeue: invalid parameter");
        return DL_STATUS_INVALID_PARAM;
    }

    dl_queue_t* q = (dl_queue_t*)queue;

    /* Check if queue is empty */
    if (q->count == 0) {
        dl_set_error(DL_ERROR_QUEUE_EMPTY, "dl_queue_dequeue: queue is empty");
        return DL_STATUS_ERROR;
    }

    /* Copy entry */
    *entry = q->entries[q->head];

    /* Update head and count */
    q->head = (q->head + 1) % q->capacity;
    q->count--;

    return DL_STATUS_SUCCESS;
}

dl_status_t dl_queue_peek(dl_queue_handle_t queue, dl_queue_entry_t* entry)
{
    if (queue == NULL || entry == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "dl_queue_peek: invalid parameter");
        return DL_STATUS_INVALID_PARAM;
    }

    dl_queue_t* q = (dl_queue_t*)queue;

    /* Check if queue is empty */
    if (q->count == 0) {
        dl_set_error(DL_ERROR_QUEUE_EMPTY, "dl_queue_peek: queue is empty");
        return DL_STATUS_ERROR;
    }

    /* Copy entry without removing */
    *entry = q->entries[q->head];

    return DL_STATUS_SUCCESS;
}

dl_status_t dl_queue_flush(dl_queue_handle_t queue)
{
    if (queue == NULL) {
        dl_set_error(DL_ERROR_INVALID_PARAM, "dl_queue_flush: invalid parameter");
        return DL_STATUS_INVALID_PARAM;
    }

    dl_queue_t* q = (dl_queue_t*)queue;

    /* Reset queue to empty state */
    q->count = 0;
    q->head = 0;
    q->tail = 0;

    return DL_STATUS_SUCCESS;
}

uint16_t dl_queue_count(dl_queue_handle_t queue)
{
    if (queue == NULL) {
        return 0;
    }

    dl_queue_t* q = (dl_queue_t*)queue;
    return q->count;
}

uint16_t dl_queue_capacity(dl_queue_handle_t queue)
{
    if (queue == NULL) {
        return 0;
    }

    dl_queue_t* q = (dl_queue_t*)queue;
    return q->capacity;
}

bool dl_queue_is_empty(dl_queue_handle_t queue)
{
    if (queue == NULL) {
        return true;
    }

    dl_queue_t* q = (dl_queue_t*)queue;
    return (q->count == 0);
}

bool dl_queue_is_full(dl_queue_handle_t queue)
{
    if (queue == NULL) {
        return true;
    }

    dl_queue_t* q = (dl_queue_t*)queue;
    return (q->count >= q->capacity);
}
