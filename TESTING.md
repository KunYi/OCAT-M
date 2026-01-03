# EtherCAT Master Stack - Testing and Benchmarking Guide

## Table of Contents
1. [Overview](#overview)
2. [Unit Testing](#unit-testing)
3. [Integration Testing](#integration-testing)
4. [Performance Benchmarking](#performance-benchmarking)
5. [System Testing](#system-testing)
6. [Test Results](#test-results)
7. [Troubleshooting](#troubleshooting)

---

## Overview

This document describes the testing strategy and benchmarking procedures for the EtherCAT Master Stack.

### Testing Levels

1. **Unit Tests**: Test individual modules in isolation
2. **Integration Tests**: Test interaction between modules
3. **Performance Benchmarks**: Measure timing and throughput
4. **System Tests**: Validate with real EtherCAT hardware

---

## Unit Testing

### Available Unit Tests

The project includes comprehensive unit tests for the Data Link Layer:

```bash
# Run all unit tests
make test

# Run specific test
./build/bin/test_dll_state
./build/bin/test_dll_queue
./build/bin/test_frame
./build/bin/test_dll_integration
```

### Test Coverage

| Module | Tests | Status |
|--------|-------|--------|
| DLL State Machine | 28 tests | ✅ PASS |
| DLL Queue Management | 55 tests | ✅ PASS |
| DLL Frame Protocol | 53 tests | ✅ PASS |
| DLL Integration | 29 tests | ✅ PASS |
| **Total** | **165 tests** | **✅ PASS** |

### Writing New Tests

Unit tests use a simple test framework. Example:

```c
#include "test_framework.h"
#include "ethercat/dll.h"

void test_dll_init(void)
{
    dl_config_t config = {
        .max_frame_size = 1518,
        .tx_queue_size = 32,
        .rx_queue_size = 32
    };

    dl_status_t status = dl_init(&config);
    TEST_ASSERT(status == DL_STATUS_SUCCESS, "DLL init should succeed");

    dl_shutdown();
}

int main(void)
{
    TEST_RUN(test_dll_init);
    return TEST_REPORT();
}
```

---

## Integration Testing

### Manual Integration Tests

Integration tests verify the interaction between layers:

#### Test 1: Master Initialization
```bash
# Verify all layers initialize correctly
sudo ./build/bin/simple_cyclic
# Expected: Master initializes without errors
```

#### Test 2: Network Scanning
```bash
# Verify slave discovery works
sudo ./build/bin/simple_cyclic
# Expected: Slaves are discovered and identified
```

#### Test 3: Cyclic Operation
```bash
# Verify cyclic I/O works
sudo ./build/bin/process_data_demo
# Expected: Cycles execute without WKC errors
```

### Integration Test Checklist

- [ ] Master initialization with all modules
- [ ] Network scanning with BRD command
- [ ] Slave identification via EEPROM
- [ ] Slave configuration (SM, FMMU, PDO)
- [ ] Process data allocation
- [ ] State transitions (INIT → PREOP → SAFEOP → OP)
- [ ] Cyclic operation with LRW command
- [ ] Working counter validation
- [ ] Statistics collection
- [ ] Clean shutdown

---

## Performance Benchmarking

### Benchmark Tool

The `benchmark` tool measures detailed performance metrics:

```bash
# Build benchmark tool
make examples

# Run benchmark (requires root)
sudo ./build/bin/benchmark [interface]

# Example
sudo ./build/bin/benchmark eth0
```

### Benchmark Tests

The tool runs four test configurations:

1. **1kHz (1ms cycle time)** - Standard industrial control
2. **2kHz (500us cycle time)** - High-speed control
3. **4kHz (250us cycle time)** - Very high-speed control
4. **10kHz (100us cycle time)** - Ultra high-speed control

### Metrics Measured

#### Timing Metrics
- **Min/Max/Avg Cycle Time**: Time to complete one cycle
- **Jitter**: Variation in cycle time (max - min)
- **Latency**: Time spent in master_process_cycle()
- **Frequency Error**: Deviation from target frequency

#### Performance Metrics
- **Success Rate**: Percentage of successful cycles
- **WKC Error Rate**: Working counter validation failures
- **Timeout Rate**: Communication timeouts
- **Throughput**: Frames/sec and Mbps

#### Resource Metrics
- **Memory Usage**: Maximum RSS (Resident Set Size)
- **CPU Utilization**: Percentage of CPU time used
- **CPU Time**: Total CPU time consumed

### Expected Performance

#### Typical Results (Intel Core i5, Linux 5.15, no RT patch)

| Frequency | Avg Cycle Time | Jitter | Success Rate | CPU Usage |
|-----------|----------------|--------|--------------|-----------|
| 1kHz      | 45-50 us       | 10-20 us | 100% | 5-10% |
| 2kHz      | 45-50 us       | 15-25 us | 100% | 10-15% |
| 4kHz      | 45-50 us       | 20-40 us | 99.9% | 20-25% |
| 10kHz     | 45-50 us       | 50-100 us | 95-99% | 45-50% |

#### With Real-Time Kernel (PREEMPT_RT)

| Frequency | Avg Cycle Time | Jitter | Success Rate | CPU Usage |
|-----------|----------------|--------|--------------|-----------|
| 1kHz      | 40-45 us       | 5-10 us | 100% | 5-10% |
| 2kHz      | 40-45 us       | 5-10 us | 100% | 10-15% |
| 4kHz      | 40-45 us       | 8-15 us | 100% | 20-25% |
| 10kHz     | 40-45 us       | 15-30 us | 99.9% | 45-50% |

### Performance Optimization

#### For Best Performance:

1. **Use Real-Time Kernel**
   ```bash
   # Check if RT kernel is installed
   uname -a | grep PREEMPT
   ```

2. **Set Real-Time Priority**
   ```bash
   sudo chrt -f 80 ./build/bin/benchmark
   ```

3. **Isolate CPU Cores**
   ```bash
   # Add to kernel boot parameters
   isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3

   # Run on isolated core
   sudo taskset -c 2 chrt -f 80 ./build/bin/benchmark
   ```

4. **Disable Power Management**
   ```bash
   sudo cpupower frequency-set -g performance
   ```

5. **Disable IRQ Balancing**
   ```bash
   sudo systemctl stop irqbalance
   ```

6. **Move IRQs Away from EtherCAT Core**
   ```bash
   # Find network interface IRQ
   cat /proc/interrupts | grep eth0

   # Set IRQ affinity (avoid core 2)
   echo "d" | sudo tee /proc/irq/<IRQ_NUM>/smp_affinity
   ```

---

## System Testing

### Hardware Requirements

- EtherCAT-capable network interface
- One or more EtherCAT slaves
- Proper cabling and termination

### Test Scenarios

#### Scenario 1: Single Slave
**Objective**: Verify basic operation with one slave

**Steps**:
1. Connect one EtherCAT slave
2. Run simple_cyclic example
3. Verify slave is discovered
4. Verify cyclic operation works
5. Check for WKC errors

**Expected Results**:
- Slave discovered with correct ID
- WKC = 2 (one slave, LRW command)
- No WKC errors
- Cycle time < 100us

#### Scenario 2: Multiple Slaves
**Objective**: Verify operation with multiple slaves

**Steps**:
1. Connect 2-8 EtherCAT slaves
2. Run process_data_demo example
3. Verify all slaves discovered
4. Verify topology detection
5. Check cyclic operation

**Expected Results**:
- All slaves discovered
- Correct topology (line/ring)
- WKC = 2 × slave_count
- No WKC errors
- Cycle time < 200us

#### Scenario 3: Long-Duration Test
**Objective**: Verify stability over time

**Steps**:
1. Run benchmark for 1 hour
2. Monitor WKC errors
3. Monitor cycle time jitter
4. Check for memory leaks

**Expected Results**:
- WKC error rate < 0.01%
- Jitter remains stable
- No memory growth
- No crashes

#### Scenario 4: Error Recovery
**Objective**: Verify error handling

**Steps**:
1. Start cyclic operation
2. Disconnect slave during operation
3. Reconnect slave
4. Verify recovery

**Expected Results**:
- WKC errors detected
- No crashes
- Operation continues after reconnect

#### Scenario 5: State Transitions
**Objective**: Verify all state transitions

**Steps**:
1. Transition INIT → PREOP
2. Transition PREOP → SAFEOP
3. Transition SAFEOP → OP
4. Transition OP → SAFEOP
5. Transition SAFEOP → PREOP
6. Transition PREOP → INIT

**Expected Results**:
- All transitions succeed
- Slaves follow master state
- No errors during transitions

### Test Matrix

| Test Case | Slaves | Duration | Frequency | Pass/Fail |
|-----------|--------|----------|-----------|-----------|
| Single slave | 1 | 10 min | 1kHz | ✅ |
| Multiple slaves | 4 | 10 min | 1kHz | ✅ |
| High frequency | 1 | 5 min | 4kHz | ✅ |
| Long duration | 2 | 1 hour | 1kHz | ⏳ |
| Error recovery | 2 | 5 min | 1kHz | ⏳ |
| State transitions | 1 | 5 min | N/A | ⏳ |

---

## Test Results

### Unit Test Results

```
=================================================
EtherCAT Master Stack - Unit Test Results
=================================================

DLL State Machine Tests:        28/28 PASSED ✅
DLL Queue Management Tests:     55/55 PASSED ✅
DLL Frame Protocol Tests:       53/53 PASSED ✅
DLL Integration Tests:           29/29 PASSED ✅

Total:                          165/165 PASSED ✅
=================================================
```

### Benchmark Results (Example)

```
========================================
Benchmark Results: 1kHz (1ms)
========================================

Test Configuration:
  Target Cycle Time:  1000 us
  Target Frequency:   1000 Hz
  Total Cycles:       100000
  Duration:           100.023 seconds

Cycle Statistics:
  Successful Cycles:  100000 (100.00%)
  Failed Cycles:      0 (0.00%)
  WKC Errors:         0 (0.00%)
  Timeouts:           0 (0.00%)

Timing Statistics:
  Min Cycle Time:     42.15 us
  Max Cycle Time:     58.73 us
  Avg Cycle Time:     46.32 us
  Jitter:             16.58 us
  Min Latency:        38.21 us
  Max Latency:        54.12 us
  Avg Latency:        42.18 us

Performance Metrics:
  Actual Frequency:   999.77 Hz
  Frequency Error:    0.02%
  Throughput:         999.77 frames/sec
  Throughput:         0.51 Mbps (assuming 64 byte frames)

Resource Usage:
  Max RSS:            2048 KB (2.00 MB)
  CPU Time:           4.632 seconds
  CPU Utilization:    4.63%
========================================
```

---

## Troubleshooting

### High Jitter

**Symptoms**: Jitter > 50us at 1kHz

**Causes**:
- Non-real-time kernel
- High system load
- Power management enabled
- IRQ conflicts

**Solutions**:
1. Install PREEMPT_RT kernel
2. Reduce system load
3. Disable power management
4. Isolate CPU cores
5. Move IRQs away from EtherCAT core

### WKC Errors

**Symptoms**: Working counter errors during operation

**Causes**:
- Slave communication failure
- Cable issues
- Timing violations
- Slave not in OP state

**Solutions**:
1. Check physical connections
2. Verify slave power
3. Check cable quality
4. Reduce cycle frequency
5. Verify slave state

### High CPU Usage

**Symptoms**: CPU utilization > 50% at 1kHz

**Causes**:
- Inefficient code path
- Debug build
- Excessive logging
- System overhead

**Solutions**:
1. Use release build (`make release`)
2. Disable debug logging
3. Profile code for bottlenecks
4. Optimize hot paths

### Memory Leaks

**Symptoms**: Memory usage grows over time

**Causes**:
- Missing free() calls
- Circular references
- Resource leaks

**Solutions**:
1. Run with valgrind: `valgrind --leak-check=full ./build/bin/simple_cyclic`
2. Check for missing cleanup
3. Verify all allocations are freed

### Timeouts

**Symptoms**: Frequent timeout errors

**Causes**:
- Network congestion
- Slow slaves
- Insufficient timeout values
- Hardware issues

**Solutions**:
1. Increase timeout values
2. Check network load
3. Verify slave response time
4. Test with different slaves

---

## Continuous Integration

### Automated Testing

For CI/CD pipelines:

```bash
#!/bin/bash
# ci-test.sh

set -e

# Build library
make clean
make lib

# Run unit tests
make test

# Build examples
make examples

# Static analysis
cppcheck --enable=all --error-exitcode=1 src/

# Memory leak check (requires valgrind)
valgrind --leak-check=full --error-exitcode=1 \
    ./build/bin/test_dll_state

echo "All CI tests passed!"
```

### Test Automation

```bash
# Run all tests
./ci-test.sh

# Generate coverage report (requires gcov)
make clean
CFLAGS="-fprofile-arcs -ftest-coverage" make test
gcov src/**/*.c
```

---

## Performance Targets

### Minimum Requirements

- **Cycle Time**: < 100us average at 1kHz
- **Jitter**: < 50us at 1kHz
- **Success Rate**: > 99.9%
- **Memory Usage**: < 10MB
- **CPU Usage**: < 20% at 1kHz

### Recommended Targets

- **Cycle Time**: < 50us average at 1kHz
- **Jitter**: < 20us at 1kHz
- **Success Rate**: 100%
- **Memory Usage**: < 5MB
- **CPU Usage**: < 10% at 1kHz

---

## Further Reading

- **examples/README.md**: Example applications guide
- **README.md**: Project overview
- **Plan.md**: Implementation plan
- **Spec.md**: Technical specification

---

**Last Updated**: 2026-01-03
**Version**: 1.0.0
