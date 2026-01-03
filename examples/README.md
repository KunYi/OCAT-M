# EtherCAT Master Stack - Example Applications

This directory contains example applications demonstrating the use of the EtherCAT Master Stack.

## Building Examples

To build all examples:

```bash
make examples
```

The compiled binaries will be placed in `build/bin/`.

## Examples

### 1. simple_cyclic.c

**Purpose**: Demonstrates basic cyclic I/O operation with EtherCAT slaves.

**Features**:
- Master initialization and configuration
- Network scanning and slave discovery
- Slave configuration
- Process data allocation
- State transitions (INIT → PREOP → SAFEOP → OP)
- Cyclic I/O loop with 1kHz cycle rate
- Working counter monitoring
- Statistics collection
- Clean shutdown

**Usage**:
```bash
# Run with default interface (eth0)
sudo ./build/bin/simple_cyclic

# Run with specific interface
sudo ./build/bin/simple_cyclic eth1
```

**Output**:
- Slave information (Vendor ID, Product Code, Name, etc.)
- Network topology
- Cyclic operation status (every 1 second)
- Final statistics (cycle count, errors, timing)

**Duration**: Runs for 10 seconds or until Ctrl+C

### 2. process_data_demo.c

**Purpose**: Demonstrates advanced process data operations and monitoring.

**Features**:
- Direct process data image access
- Per-slave data read/write operations
- Multiple output patterns (counter, toggle, sine wave)
- Working counter monitoring
- Detailed cycle time statistics
- Error handling and recovery
- Process data visualization

**Usage**:
```bash
# Run with default interface (eth0)
sudo ./build/bin/process_data_demo

# Run with specific interface
sudo ./build/bin/process_data_demo eth1
```

**Output**:
- Process data image details (sizes, addresses)
- Per-slave input/output data (hex dump)
- Periodic statistics (every 1 second)
- Detailed final statistics (cycle count, errors, timing, jitter)

**Duration**: Runs for 10,000 cycles (10 seconds at 1kHz) or until Ctrl+C

### 3. benchmark.c

**Purpose**: Measures detailed performance metrics for the EtherCAT Master Stack.

**Features**:
- Multiple test configurations (1kHz, 2kHz, 4kHz, 10kHz)
- Warmup phase before measurement
- Detailed timing statistics (min/max/avg/jitter)
- Latency measurement
- Resource usage tracking (memory, CPU)
- Throughput calculation
- Success rate and error rate tracking

**Usage**:
```bash
# Run benchmark with default interface (eth0)
sudo ./build/bin/benchmark

# Run with specific interface
sudo ./build/bin/benchmark eth1
```

**Output**:
- Test configuration details
- Cycle statistics (successful, failed, WKC errors)
- Timing statistics (min/max/avg cycle time, jitter, latency)
- Performance metrics (frequency, throughput)
- Resource usage (memory, CPU utilization)

**Duration**: Runs 100,000 cycles per test configuration (approximately 5-10 minutes total)

**Note**: For best results, run with real-time priority:
```bash
sudo chrt -f 80 ./build/bin/benchmark
```

## Requirements

### Hardware
- EtherCAT-capable network interface
- One or more EtherCAT slaves (optional - examples work with zero slaves)

### Software
- Linux operating system
- Root privileges (required for raw socket access)
- EtherCAT Master Stack library (libethercat.a)

### Permissions
The examples require root privileges to access raw Ethernet sockets:

```bash
# Option 1: Run with sudo
sudo ./build/bin/simple_cyclic

# Option 2: Set capabilities (Linux only)
sudo setcap cap_net_raw+ep ./build/bin/simple_cyclic
./build/bin/simple_cyclic
```

## Network Interface Configuration

Before running the examples, ensure your network interface is configured:

```bash
# Bring interface up
sudo ip link set eth0 up

# Verify interface is up
ip link show eth0

# Check for EtherCAT slaves (optional)
sudo ethercat slaves  # If ethercat tool is installed
```

## Example Output

### simple_cyclic.c Output:
```
=================================================
EtherCAT Simple Cyclic I/O Example
=================================================

Network Interface: eth0
Cycle Time:        1000 us (1000 Hz)
Run Duration:      10 seconds

Step 1: Initializing EtherCAT Master...
  Master initialized successfully

Step 2: Scanning EtherCAT network...
  Found 2 slave(s)

Slave Information:
  Slave 0:
    Station Address: 0x1000
    Alias Address:   0x0000
    Vendor ID:       0x00000002
    Product Code:    0x044C2C52
    Name:            EK1100 EtherCAT Coupler
    ...

Step 6: Running cyclic I/O loop...
  Cycles: 1000, WKC Errors: 0, Avg Cycle Time: 45 us
  Cycles: 2000, WKC Errors: 0, Avg Cycle Time: 46 us
  ...

Cyclic Operation Statistics:
  Total Cycles:    10000
  WKC Errors:      0
  Timeouts:        0
  Min Cycle Time:  42 us
  Max Cycle Time:  58 us
  Avg Cycle Time:  46 us
```

### process_data_demo.c Output:
```
=================================================
EtherCAT Process Data Demonstration
=================================================

Process Data Image:
  Logical Address: 0x00000000
  Input Size:      128 bytes
  Output Size:     128 bytes

  Output Data (first 16 bytes):
    00 00 00 00 00 00 00 00
    00 00 00 00 00 00 00 00

Cycle 1000: WKC=4, Errors=0, Avg Time=47 us
Cycle 2000: WKC=4, Errors=0, Avg Time=46 us
...

========================================
Process Data Statistics
========================================
Cycle Statistics:
  Total Cycles:       10000
  WKC Errors:         0 (0.00%)
  Timeouts:           0 (0.00%)

Timing Statistics:
  Min Cycle Time:     42 us
  Max Cycle Time:     61 us
  Avg Cycle Time:     47 us
  Jitter:             19 us
========================================
```

## Troubleshooting

### "Permission denied" Error
- Run with `sudo` or set capabilities: `sudo setcap cap_net_raw+ep <binary>`

### "No such device" Error
- Check interface name: `ip link show`
- Verify interface is up: `sudo ip link set <interface> up`

### "No slaves found" Warning
- This is normal if no EtherCAT slaves are connected
- Examples will run with zero slaves for demonstration
- Check physical connections and slave power

### High Cycle Time / Jitter
- Check system load: `top` or `htop`
- Disable power management: `sudo cpupower frequency-set -g performance`
- Use real-time kernel for better performance
- Isolate CPU cores for EtherCAT task

### Compilation Errors
- Ensure library is built: `make lib`
- Check include paths in Makefile
- Verify C11 compiler support: `gcc --version`

## Performance Tips

### For Real-Time Performance:
1. **Use Real-Time Kernel**: Install PREEMPT_RT patch
2. **Set Process Priority**: Use `chrt` to set real-time priority
3. **CPU Isolation**: Isolate CPU cores using `isolcpus` kernel parameter
4. **Disable Power Management**: Set CPU governor to `performance`
5. **Disable Interrupts**: Move IRQs away from EtherCAT CPU core

Example with real-time priority:
```bash
sudo chrt -f 80 ./build/bin/simple_cyclic
```

### For Low Jitter:
1. Minimize system load (close unnecessary applications)
2. Use dedicated network interface for EtherCAT
3. Disable network manager on EtherCAT interface
4. Use kernel with high-resolution timers (CONFIG_HIGH_RES_TIMERS)

## Extending Examples

### Adding Custom Application Logic

Modify the cyclic loop in `simple_cyclic.c`:

```c
/* In master_process_cycle() loop */

/* Read inputs from slaves */
uint8_t input_data[8];
master_read_slave_input(0, input_data, sizeof(input_data));

/* Your application logic here */
uint8_t output_data[8];
output_data[0] = input_data[0] + 1;  // Example: increment

/* Write outputs to slaves */
master_write_slave_output(0, output_data, sizeof(output_data));
```

### Adding CoE (CANopen) Access

```c
#include "ethercat/coe.h"

/* Read object dictionary entry */
uint32_t value;
coe_status_t status = coe_sdo_upload(
    station_address,
    0x1000,  /* Index */
    0x00,    /* Subindex */
    (uint8_t*)&value,
    sizeof(value),
    NULL,
    1000     /* Timeout ms */
);
```

### Adding Distributed Clocks

```c
#include "ethercat/dc.h"

/* Enable DC in master configuration */
config.enable_dc = true;

/* Configure DC after slave configuration */
dc_config_t dc_config = {
    .enable = true,
    .cycle_time_ns = 1000000,  /* 1ms */
    .shift_time_ns = 0
};
dc_configure_sync(&dc_config);
```

## Further Reading

- **Spec.md**: Technical specification of all APIs
- **Plan.md**: Implementation plan and architecture
- **README.md**: Project overview and build instructions
- **ETG1000 Series**: Official EtherCAT specifications (in `specs/` directory)

## License

This is a clean-room implementation based on publicly available ETG specifications.

---

**Last Updated**: 2026-01-03
**Version**: 1.0.0
