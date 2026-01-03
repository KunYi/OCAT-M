/**
 * @file test_dll_queue.c
 * @brief Unit tests for DLL queue management
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "ethercat/dll_queue.h"
#include "ethercat/dll_errors.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* ========================================================================== */
/* Test Helpers                                                               */
/* ========================================================================== */

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("  [PASS] %s\n", message); \
        } else { \
            tests_failed++; \
            printf("  [FAIL] %s\n", message); \
        } \
    } while(0)

#define TEST_START(name) \
    printf("\n=== Test: %s ===\n", name)

/* ========================================================================== */
/* Test Cases                                                                 */
/* ========================================================================== */

void test_queue_create_destroy(void)
{
    TEST_START("Queue Creation and Destruction");

    /* Create queue */
    dl_queue_handle_t queue = dl_queue_create(10, false);
    TEST_ASSERT(queue != NULL, "Queue creation");

    /* Check initial state */
    TEST_ASSERT(dl_queue_count(queue) == 0, "Initial count is 0");
    TEST_ASSERT(dl_queue_capacity(queue) == 10, "Capacity is 10");
    TEST_ASSERT(dl_queue_is_empty(queue) == true, "Queue is empty");
    TEST_ASSERT(dl_queue_is_full(queue) == false, "Queue is not full");

    /* Destroy queue */
    dl_queue_destroy(queue);
    TEST_ASSERT(true, "Queue destruction");
}

void test_queue_enqueue_dequeue(void)
{
    TEST_START("Queue Enqueue and Dequeue");

    dl_queue_handle_t queue = dl_queue_create(5, false);
    dl_status_t status;
    dl_queue_entry_t entry_in, entry_out;

    /* Prepare test entry */
    uint8_t buffer1[] = {0x01, 0x02, 0x03};
    entry_in.buffer = buffer1;
    entry_in.length = 3;
    entry_in.priority = 0;
    entry_in.user_data = NULL;
    entry_in.timestamp = 1000;

    /* Enqueue entry */
    status = dl_queue_enqueue(queue, &entry_in);
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "Enqueue success");
    TEST_ASSERT(dl_queue_count(queue) == 1, "Count is 1 after enqueue");
    TEST_ASSERT(dl_queue_is_empty(queue) == false, "Queue is not empty");

    /* Dequeue entry */
    status = dl_queue_dequeue(queue, &entry_out);
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "Dequeue success");
    TEST_ASSERT(entry_out.length == 3, "Dequeued length matches");
    TEST_ASSERT(entry_out.buffer == buffer1, "Dequeued buffer matches");
    TEST_ASSERT(dl_queue_count(queue) == 0, "Count is 0 after dequeue");
    TEST_ASSERT(dl_queue_is_empty(queue) == true, "Queue is empty");

    dl_queue_destroy(queue);
}

void test_queue_multiple_entries(void)
{
    TEST_START("Queue Multiple Entries");

    dl_queue_handle_t queue = dl_queue_create(5, false);
    dl_status_t status;
    dl_queue_entry_t entry;

    /* Enqueue multiple entries */
    for (int i = 0; i < 3; i++) {
        entry.buffer = (uint8_t*)(uintptr_t)i;
        entry.length = i;
        entry.priority = 0;
        entry.user_data = NULL;
        entry.timestamp = i * 1000;

        status = dl_queue_enqueue(queue, &entry);
        TEST_ASSERT(status == DL_STATUS_SUCCESS, "Enqueue entry");
    }

    TEST_ASSERT(dl_queue_count(queue) == 3, "Count is 3");

    /* Dequeue and verify order (FIFO) */
    for (int i = 0; i < 3; i++) {
        status = dl_queue_dequeue(queue, &entry);
        TEST_ASSERT(status == DL_STATUS_SUCCESS && entry.length == (uint16_t)i,
                    "Dequeue in FIFO order");
    }

    TEST_ASSERT(dl_queue_is_empty(queue) == true, "Queue is empty after dequeue all");

    dl_queue_destroy(queue);
}

void test_queue_full(void)
{
    TEST_START("Queue Full Condition");

    dl_queue_handle_t queue = dl_queue_create(3, false);
    dl_status_t status;
    dl_queue_entry_t entry = {0};

    /* Fill queue */
    for (int i = 0; i < 3; i++) {
        status = dl_queue_enqueue(queue, &entry);
        TEST_ASSERT(status == DL_STATUS_SUCCESS, "Enqueue to fill queue");
    }

    TEST_ASSERT(dl_queue_is_full(queue) == true, "Queue is full");

    /* Try to enqueue when full */
    status = dl_queue_enqueue(queue, &entry);
    TEST_ASSERT(status != DL_STATUS_SUCCESS, "Enqueue fails when full");

    dl_queue_destroy(queue);
}

void test_queue_empty(void)
{
    TEST_START("Queue Empty Condition");

    dl_queue_handle_t queue = dl_queue_create(5, false);
    dl_status_t status;
    dl_queue_entry_t entry;

    /* Try to dequeue from empty queue */
    status = dl_queue_dequeue(queue, &entry);
    TEST_ASSERT(status != DL_STATUS_SUCCESS, "Dequeue fails when empty");

    dl_queue_destroy(queue);
}

void test_queue_peek(void)
{
    TEST_START("Queue Peek");

    dl_queue_handle_t queue = dl_queue_create(5, false);
    dl_status_t status;
    dl_queue_entry_t entry_in, entry_out;

    /* Enqueue entry */
    entry_in.buffer = (uint8_t*)0x1234;
    entry_in.length = 100;
    entry_in.priority = 0;
    entry_in.user_data = NULL;
    entry_in.timestamp = 5000;

    dl_queue_enqueue(queue, &entry_in);

    /* Peek at entry */
    status = dl_queue_peek(queue, &entry_out);
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "Peek success");
    TEST_ASSERT(entry_out.length == 100, "Peeked entry matches");
    TEST_ASSERT(dl_queue_count(queue) == 1, "Count unchanged after peek");

    /* Peek again - should get same entry */
    status = dl_queue_peek(queue, &entry_out);
    TEST_ASSERT(status == DL_STATUS_SUCCESS && entry_out.length == 100,
                "Peek returns same entry");

    dl_queue_destroy(queue);
}

void test_queue_flush(void)
{
    TEST_START("Queue Flush");

    dl_queue_handle_t queue = dl_queue_create(5, false);
    dl_queue_entry_t entry = {0};

    /* Add some entries */
    for (int i = 0; i < 3; i++) {
        dl_queue_enqueue(queue, &entry);
    }

    TEST_ASSERT(dl_queue_count(queue) == 3, "Queue has 3 entries");

    /* Flush queue */
    dl_status_t status = dl_queue_flush(queue);
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "Flush success");
    TEST_ASSERT(dl_queue_count(queue) == 0, "Queue is empty after flush");
    TEST_ASSERT(dl_queue_is_empty(queue) == true, "Queue is empty");

    dl_queue_destroy(queue);
}

void test_queue_priority(void)
{
    TEST_START("Queue Priority Ordering");

    dl_queue_handle_t queue = dl_queue_create(10, true);
    dl_status_t status;
    dl_queue_entry_t entry_in, entry_out;

    /* Enqueue entries with different priorities */
    for (int i = 0; i < 5; i++) {
        entry_in.buffer = (uint8_t*)(uintptr_t)i;
        entry_in.length = i;
        entry_in.priority = i % 3;  /* Priorities: 0, 1, 2, 0, 1 */
        entry_in.user_data = NULL;
        entry_in.timestamp = i * 1000;

        status = dl_queue_enqueue(queue, &entry_in);
        TEST_ASSERT(status == DL_STATUS_SUCCESS, "Enqueue with priority");
    }

    /* Dequeue and verify priority order (higher priority first) */
    /* Expected order: priority 2 (index 2), then priority 1 (indices 1, 4),
       then priority 0 (indices 0, 3) */

    status = dl_queue_dequeue(queue, &entry_out);
    TEST_ASSERT(status == DL_STATUS_SUCCESS && entry_out.priority == 2,
                "Highest priority dequeued first");

    status = dl_queue_dequeue(queue, &entry_out);
    TEST_ASSERT(status == DL_STATUS_SUCCESS && entry_out.priority == 1,
                "Second highest priority");

    status = dl_queue_dequeue(queue, &entry_out);
    TEST_ASSERT(status == DL_STATUS_SUCCESS && entry_out.priority == 1,
                "Second highest priority (second entry)");

    status = dl_queue_dequeue(queue, &entry_out);
    TEST_ASSERT(status == DL_STATUS_SUCCESS && entry_out.priority == 0,
                "Lowest priority");

    status = dl_queue_dequeue(queue, &entry_out);
    TEST_ASSERT(status == DL_STATUS_SUCCESS && entry_out.priority == 0,
                "Lowest priority (second entry)");

    dl_queue_destroy(queue);
}

void test_queue_circular_buffer(void)
{
    TEST_START("Queue Circular Buffer Behavior");

    dl_queue_handle_t queue = dl_queue_create(3, false);
    dl_queue_entry_t entry_in, entry_out;

    /* Fill and empty queue multiple times */
    for (int cycle = 0; cycle < 3; cycle++) {
        /* Fill queue */
        for (int i = 0; i < 3; i++) {
            entry_in.buffer = (uint8_t*)(uintptr_t)(cycle * 10 + i);
            entry_in.length = cycle * 10 + i;
            entry_in.priority = 0;
            entry_in.user_data = NULL;
            entry_in.timestamp = 0;

            dl_queue_enqueue(queue, &entry_in);
        }

        /* Empty queue */
        for (int i = 0; i < 3; i++) {
            dl_queue_dequeue(queue, &entry_out);
            TEST_ASSERT(entry_out.length == (uint16_t)(cycle * 10 + i),
                        "Circular buffer maintains order");
        }
    }

    dl_queue_destroy(queue);
}

/* ========================================================================== */
/* Main Test Runner                                                           */
/* ========================================================================== */

int main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("DLL Queue Management Unit Tests\n");
    printf("========================================\n");

    /* Run all tests */
    test_queue_create_destroy();
    test_queue_enqueue_dequeue();
    test_queue_multiple_entries();
    test_queue_full();
    test_queue_empty();
    test_queue_peek();
    test_queue_flush();
    test_queue_priority();
    test_queue_circular_buffer();

    /* Print summary */
    printf("\n");
    printf("========================================\n");
    printf("Test Summary\n");
    printf("========================================\n");
    printf("Total tests:  %d\n", tests_run);
    printf("Passed:       %d\n", tests_passed);
    printf("Failed:       %d\n", tests_failed);
    printf("========================================\n");

    if (tests_failed == 0) {
        printf("\nAll tests PASSED!\n\n");
        return 0;
    } else {
        printf("\nSome tests FAILED!\n\n");
        return 1;
    }
}
