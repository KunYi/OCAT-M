/**
 * @file test_frame.c
 * @brief Unit tests for EtherCAT frame builder and parser
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "ethercat/frame.h"
#include "ethercat/frame_builder.h"
#include "ethercat/frame_parser.h"
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

void test_command_functions(void)
{
    TEST_START("Command Type Functions");

    /* Test command names */
    TEST_ASSERT(strcmp(ecat_cmd_get_name(ECAT_CMD_APRD), "APRD") == 0,
                "APRD command name");
    TEST_ASSERT(strcmp(ecat_cmd_get_name(ECAT_CMD_LWR), "LWR") == 0,
                "LWR command name");
    TEST_ASSERT(strcmp(ecat_cmd_get_name(0xFF), "UNKNOWN") == 0,
                "Unknown command name");

    /* Test read operations */
    TEST_ASSERT(ecat_cmd_is_read(ECAT_CMD_APRD) == true,
                "APRD is read operation");
    TEST_ASSERT(ecat_cmd_is_read(ECAT_CMD_FPRD) == true,
                "FPRD is read operation");
    TEST_ASSERT(ecat_cmd_is_read(ECAT_CMD_APWR) == false,
                "APWR is not read operation");

    /* Test write operations */
    TEST_ASSERT(ecat_cmd_is_write(ECAT_CMD_APWR) == true,
                "APWR is write operation");
    TEST_ASSERT(ecat_cmd_is_write(ECAT_CMD_BWR) == true,
                "BWR is write operation");
    TEST_ASSERT(ecat_cmd_is_write(ECAT_CMD_APRD) == false,
                "APRD is not write operation");

    /* Test read/write operations */
    TEST_ASSERT(ecat_cmd_is_readwrite(ECAT_CMD_APRW) == true,
                "APRW is read/write operation");
    TEST_ASSERT(ecat_cmd_is_readwrite(ECAT_CMD_LRW) == true,
                "LRW is read/write operation");
    TEST_ASSERT(ecat_cmd_is_readwrite(ECAT_CMD_APRD) == false,
                "APRD is not read/write operation");
}

void test_addressing_functions(void)
{
    TEST_START("Addressing Functions");

    /* Test auto-increment addressing */
    uint32_t addr = ecat_addr_autoincrement(1, 0x1000);
    TEST_ASSERT((addr >> 16) == 0xFFFF && (addr & 0xFFFF) == 0x1000,
                "Auto-increment address for position 1");

    addr = ecat_addr_autoincrement(5, 0x0010);
    TEST_ASSERT((addr >> 16) == 0xFFFB && (addr & 0xFFFF) == 0x0010,
                "Auto-increment address for position 5");

    /* Test configured addressing */
    addr = ecat_addr_configured(0x1001, 0x2000);
    TEST_ASSERT((addr >> 16) == 0x1001 && (addr & 0xFFFF) == 0x2000,
                "Configured address");

    /* Test logical addressing */
    addr = ecat_addr_logical(0x12345678);
    TEST_ASSERT(addr == 0x12345678,
                "Logical address");
}

void test_working_counter(void)
{
    TEST_START("Working Counter");

    /* Test validation */
    TEST_ASSERT(ecat_wkc_validate(5, 5) == true,
                "WKC validation - match");
    TEST_ASSERT(ecat_wkc_validate(5, 3) == false,
                "WKC validation - mismatch");

    /* Test expected WKC calculation */
    TEST_ASSERT(ecat_wkc_expected(ECAT_CMD_APRD, 3) == 3,
                "Expected WKC for read (3 slaves)");
    TEST_ASSERT(ecat_wkc_expected(ECAT_CMD_APRW, 2) == 4,
                "Expected WKC for read/write (2 slaves)");
}

void test_frame_builder_init(void)
{
    TEST_START("Frame Builder Initialization");

    uint8_t buffer[1518];
    ecat_frame_builder_t builder;
    uint8_t src_mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    dl_status_t status = ecat_frame_builder_init(&builder, buffer, sizeof(buffer),
                                                  src_mac, dst_mac);
    TEST_ASSERT(status == DL_STATUS_SUCCESS,
                "Frame builder initialization");

    TEST_ASSERT(ecat_frame_builder_get_datagram_count(&builder) == 0,
                "Initial datagram count is 0");

    TEST_ASSERT(ecat_frame_builder_get_size(&builder) == 16,
                "Initial size is 16 (headers only)");
}

void test_frame_builder_add_datagram(void)
{
    TEST_START("Frame Builder Add Datagram");

    uint8_t buffer[1518];
    ecat_frame_builder_t builder;
    uint8_t src_mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    ecat_frame_builder_init(&builder, buffer, sizeof(buffer), src_mac, dst_mac);

    /* Add a read datagram */
    uint32_t addr = ecat_addr_autoincrement(1, 0x0000);
    dl_status_t status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_APRD,
                                                          0, addr, NULL, 4, false);
    TEST_ASSERT(status == DL_STATUS_SUCCESS,
                "Add read datagram");

    TEST_ASSERT(ecat_frame_builder_get_datagram_count(&builder) == 1,
                "Datagram count is 1");

    /* Add a write datagram */
    uint8_t write_data[4] = {0x11, 0x22, 0x33, 0x44};
    addr = ecat_addr_autoincrement(2, 0x0010);
    status = ecat_frame_builder_add_datagram(&builder, ECAT_CMD_APWR,
                                              1, addr, write_data, 4, false);
    TEST_ASSERT(status == DL_STATUS_SUCCESS,
                "Add write datagram");

    TEST_ASSERT(ecat_frame_builder_get_datagram_count(&builder) == 2,
                "Datagram count is 2");
}

void test_frame_builder_finalize(void)
{
    TEST_START("Frame Builder Finalize");

    uint8_t buffer[1518];
    ecat_frame_builder_t builder;
    uint8_t src_mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint16_t frame_length;

    ecat_frame_builder_init(&builder, buffer, sizeof(buffer), src_mac, dst_mac);

    /* Add datagram */
    uint32_t addr = ecat_addr_autoincrement(1, 0x0000);
    ecat_frame_builder_add_datagram(&builder, ECAT_CMD_APRD, 0, addr, NULL, 4, false);

    /* Finalize */
    dl_status_t status = ecat_frame_builder_finalize(&builder, &frame_length);
    TEST_ASSERT(status == DL_STATUS_SUCCESS,
                "Frame finalization");

    TEST_ASSERT(frame_length >= ECAT_MIN_FRAME_SIZE,
                "Frame length meets minimum");

    /* Verify Ethernet header */
    TEST_ASSERT(memcmp(&buffer[0], dst_mac, 6) == 0,
                "Destination MAC correct");
    TEST_ASSERT(memcmp(&buffer[6], src_mac, 6) == 0,
                "Source MAC correct");
    TEST_ASSERT(buffer[12] == 0xA4 && buffer[13] == 0x88,
                "EtherType correct");
}

void test_frame_parser_init(void)
{
    TEST_START("Frame Parser Initialization");

    /* Build a frame first */
    uint8_t buffer[1518];
    ecat_frame_builder_t builder;
    uint8_t src_mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint16_t frame_length;

    ecat_frame_builder_init(&builder, buffer, sizeof(buffer), src_mac, dst_mac);
    uint32_t addr = ecat_addr_autoincrement(1, 0x0000);
    ecat_frame_builder_add_datagram(&builder, ECAT_CMD_APRD, 0, addr, NULL, 4, false);
    ecat_frame_builder_finalize(&builder, &frame_length);

    /* Parse the frame */
    ecat_frame_parser_t parser;
    dl_status_t status = ecat_frame_parser_init(&parser, buffer, frame_length);
    TEST_ASSERT(status == DL_STATUS_SUCCESS,
                "Frame parser initialization");

    status = ecat_frame_parser_validate(&parser);
    TEST_ASSERT(status == DL_STATUS_SUCCESS,
                "Frame validation");
}

void test_frame_parser_datagrams(void)
{
    TEST_START("Frame Parser Datagram Parsing");

    /* Build a frame with multiple datagrams */
    uint8_t buffer[1518];
    ecat_frame_builder_t builder;
    uint8_t src_mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t dst_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint16_t frame_length;

    ecat_frame_builder_init(&builder, buffer, sizeof(buffer), src_mac, dst_mac);

    /* Add first datagram */
    uint32_t addr1 = ecat_addr_autoincrement(1, 0x0000);
    ecat_frame_builder_add_datagram(&builder, ECAT_CMD_APRD, 0, addr1, NULL, 4, true);

    /* Add second datagram */
    uint8_t write_data[4] = {0x11, 0x22, 0x33, 0x44};
    uint32_t addr2 = ecat_addr_autoincrement(2, 0x0010);
    ecat_frame_builder_add_datagram(&builder, ECAT_CMD_APWR, 1, addr2, write_data, 4, false);

    ecat_frame_builder_finalize(&builder, &frame_length);

    /* Parse the frame */
    ecat_frame_parser_t parser;
    ecat_frame_parser_init(&parser, buffer, frame_length);
    ecat_frame_parser_validate(&parser);

    /* Parse first datagram */
    ecat_parsed_datagram_t dgram;
    dl_status_t status = ecat_frame_parser_next_datagram(&parser, &dgram);
    TEST_ASSERT(status == DL_STATUS_SUCCESS,
                "Parse first datagram");
    TEST_ASSERT(dgram.cmd == ECAT_CMD_APRD,
                "First datagram command is APRD");
    TEST_ASSERT(dgram.idx == 0,
                "First datagram index is 0");
    TEST_ASSERT(dgram.length == 4,
                "First datagram length is 4");
    TEST_ASSERT(dgram.more == true,
                "First datagram has 'more' flag");

    /* Parse second datagram */
    status = ecat_frame_parser_next_datagram(&parser, &dgram);
    TEST_ASSERT(status == DL_STATUS_SUCCESS,
                "Parse second datagram");
    TEST_ASSERT(dgram.cmd == ECAT_CMD_APWR,
                "Second datagram command is APWR");
    TEST_ASSERT(dgram.idx == 1,
                "Second datagram index is 1");
    TEST_ASSERT(dgram.length == 4,
                "Second datagram length is 4");
    TEST_ASSERT(dgram.more == false,
                "Second datagram has no 'more' flag");

    /* Verify data */
    TEST_ASSERT(memcmp(dgram.data, write_data, 4) == 0,
                "Second datagram data matches");

    /* Check no more datagrams */
    TEST_ASSERT(ecat_frame_parser_has_more(&parser) == false,
                "No more datagrams");
}

void test_round_trip(void)
{
    TEST_START("Frame Build and Parse Round Trip");

    uint8_t buffer[1518];
    ecat_frame_builder_t builder;
    uint8_t src_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t dst_mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint16_t frame_length;

    /* Build frame */
    ecat_frame_builder_init(&builder, buffer, sizeof(buffer), src_mac, dst_mac);
    uint8_t test_data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint32_t addr = ecat_addr_logical(0x12345678);
    ecat_frame_builder_add_datagram(&builder, ECAT_CMD_LWR, 42, addr, test_data, 8, false);
    ecat_frame_builder_finalize(&builder, &frame_length);

    /* Parse frame */
    ecat_frame_parser_t parser;
    ecat_frame_parser_init(&parser, buffer, frame_length);
    ecat_frame_parser_validate(&parser);

    /* Verify MAC addresses */
    uint8_t parsed_src[6], parsed_dst[6];
    ecat_frame_parser_get_src_mac(&parser, parsed_src);
    ecat_frame_parser_get_dst_mac(&parser, parsed_dst);
    TEST_ASSERT(memcmp(parsed_src, src_mac, 6) == 0,
                "Source MAC matches");
    TEST_ASSERT(memcmp(parsed_dst, dst_mac, 6) == 0,
                "Destination MAC matches");

    /* Parse datagram */
    ecat_parsed_datagram_t dgram;
    ecat_frame_parser_next_datagram(&parser, &dgram);
    TEST_ASSERT(dgram.cmd == ECAT_CMD_LWR,
                "Command matches");
    TEST_ASSERT(dgram.idx == 42,
                "Index matches");
    TEST_ASSERT(dgram.address == 0x12345678,
                "Address matches");
    TEST_ASSERT(dgram.length == 8,
                "Length matches");
    TEST_ASSERT(memcmp(dgram.data, test_data, 8) == 0,
                "Data matches");
}

/* ========================================================================== */
/* Main Test Runner                                                           */
/* ========================================================================== */

int main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("EtherCAT Frame Unit Tests\n");
    printf("========================================\n");

    /* Run all tests */
    test_command_functions();
    test_addressing_functions();
    test_working_counter();
    test_frame_builder_init();
    test_frame_builder_add_datagram();
    test_frame_builder_finalize();
    test_frame_parser_init();
    test_frame_parser_datagrams();
    test_round_trip();

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
