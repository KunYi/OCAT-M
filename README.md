# EtherCAT Master Stack

A clean-room implementation of an EtherCAT Master stack in C11 for embedded systems.

## Project Status

**Current Phase**: Phase 5.1 Complete - Process Data and Cyclic Operation ✅

### Completed Components

#### Phase 1 - Data Link Layer ✅ (100% Complete)

##### Phase 1.1-1.3 - Core Infrastructure ✅
- **DLL Types and Configuration**:
  - `include/ethercat/dll_types.h` - All DLL type definitions
  - `include/ethercat/dll_config.h` - Configuration interface
  - `include/ethercat/dll_errors.h` - Error handling
  - `include/ethercat/dll.h` - Main DLL interface

- **State Machine**:
  - `include/ethercat/dll_state.h` - State machine interface
  - `src/dll/dll_state.c` - Complete state machine implementation
  - 5 states with transition validation
  - **Test Results**: 28/28 tests passed ✅

- **Queue Management**:
  - `include/ethercat/dll_queue.h` - Queue interface
  - `src/dll/dll_queue.c` - Circular buffer with priority support
  - **Test Results**: 55/55 tests passed ✅

##### Phase 1.4-1.9 - EtherCAT Protocol ✅
- **Frame Protocol**:
  - `include/ethercat/frame.h` - Frame and datagram structures
  - `include/ethercat/frame_builder.h` - Frame builder interface
  - `include/ethercat/frame_parser.h` - Frame parser interface
  - `src/dll/frame.c`, `frame_builder.c`, `frame_parser.c` - Implementation
  - 15 datagram command types
  - 3 addressing modes (auto-increment, configured, logical)
  - **Test Results**: 53/53 tests passed ✅

- **DLL Core**:
  - `src/dll/dll_init.c` - Initialization and shutdown
  - `src/dll/dll_tx.c` - Frame transmission
  - `src/dll/dll_rx.c` - Frame reception
  - `src/dll/dll_control.c` - Control functions
  - **Test Results**: 29/29 integration tests passed ✅

##### Phase 1.10 - Hardware Abstraction Layer ✅
- **HAL Interface**:
  - `include/ethercat/hal_types.h` - HAL type definitions
  - `include/ethercat/hal.h` - HAL public API
  - `src/hal/hal_internal.h` - HAL internal definitions

- **Platform Implementations**:
  - `src/hal/hal.c` - HAL core implementation
  - `src/hal/hal_linux.c` - Linux raw socket implementation
  - `src/hal/hal_stub.c` - Stub implementation for testing

- **Features**:
  - Platform abstraction for frame send/receive
  - Support for multiple platforms (Linux, Windows, FreeRTOS, Bare-metal)
  - Frame buffer management
  - Statistics tracking

#### Phase 2 - Application Layer Services ✅ (100% Complete)

##### Phase 2.1 - AL Core Infrastructure ✅
- **AL Types and API**:
  - `include/ethercat/al_types.h` - AL type definitions
  - `include/ethercat/al.h` - AL public API
  - `src/al/al_internal.h` - AL internal definitions

- **AL States**:
  - Init, Pre-Operational, Bootstrap, Safe-Operational, Operational
  - 30+ AL status codes
  - State transition validation and execution

##### Phase 2.2 - State Machine ✅
- **Implementation**:
  - `src/al/al.c` - AL core implementation (412 lines)
  - `src/al/al_state.c` - State machine implementation (267 lines)

- **Features**:
  - 5 AL states with transition logic
  - State change callbacks
  - Timeout handling
  - Error recovery

#### Phase 3 - Application Layer Protocols ✅ (CoE Complete)

##### Phase 3.1 - CoE (CANopen over EtherCAT) ✅ (100% Complete)
- **CoE Types and API**:
  - `include/ethercat/coe.h` - CoE type definitions and public API
  - CoE status codes and service types
  - SDO command specifiers (CCS/SCS)
  - 30+ SDO abort codes with descriptions
  - Object Dictionary standard indices
  - SDO segment structures for large data transfer

- **CoE Implementation**:
  - `src/al/coe.c` - CoE core implementation (753 lines)

- **Features**:
  - CoE initialization and shutdown
  - **SDO Download (expedited)** - Fast write (1-4 bytes) ✅
  - **SDO Download (segmented)** - Large data write (>4 bytes) ✅
  - **SDO Upload (expedited)** - Fast read (1-4 bytes) ✅
  - **SDO Upload (segmented)** - Large data read (>4 bytes) ✅
  - Mailbox integration for CoE communication
  - **Timeout mechanism** - Precise timeout control with HAL time functions ✅
  - **Toggle bit verification** - Ensures data integrity in segmented transfers ✅
  - Comprehensive error handling with abort codes
  - Object Dictionary access functions

- **API Functions**:
  - `coe_init()` / `coe_shutdown()` - Module lifecycle
  - `coe_sdo_download()` - Write to Object Dictionary (expedited + segmented)
  - `coe_sdo_download_expedited()` - Fast write (1-4 bytes)
  - `coe_sdo_upload()` - Read from Object Dictionary (expedited + segmented)
  - `coe_sdo_upload_expedited()` - Fast read (1-4 bytes)
  - `coe_get_abort_code_string()` - Error descriptions
  - `coe_get_version()` - Version information

- **HAL Time Functions** (Added for timeout support):
  - `hal_get_time_ns()` - Get current time in nanoseconds
  - `hal_get_time_ms()` - Get current time in milliseconds
  - `hal_sleep_ms()` - Sleep for milliseconds
  - `hal_sleep_us()` - Sleep for microseconds
  - Platform implementations: Linux (POSIX) and Stub

#### Phase 4 - Network Scanning and Master Control ✅ (Complete)

##### Phase 4.1 - Master Control API ✅
- **Master Types and API**:
  - `include/ethercat/master.h` - Master control public API
  - Master status codes and operational states
  - Slave information structures
  - Network topology information
  - Master configuration structure

- **Master Implementation**:
  - `src/master/master.c` - Master core implementation (461 lines)
  - `src/master/master_internal.h` - Internal definitions

- **Features**:
  - Master initialization and shutdown
  - Network scanning orchestration
  - Slave configuration management
  - State management (IDLE, INIT, SCANNING, CONFIGURING, PREOP, SAFEOP, OP)
  - Cyclic operation control
  - Integration with DLL, HAL, AL, and CoE layers

- **API Functions**:
  - `master_init()` / `master_shutdown()` - Master lifecycle
  - `master_scan_network()` - Discover and configure slaves
  - `master_get_slave_count()` - Get number of slaves
  - `master_get_slave_info()` - Get slave information
  - `master_get_topology()` - Get network topology
  - `master_configure_slaves()` - Configure all slaves
  - `master_request_state()` - Request AL state change
  - `master_start_cyclic()` / `master_stop_cyclic()` - Cyclic operation
  - `master_process_cycle()` - Process one cycle

##### Phase 4.2 - Network Scanning Implementation ✅
- **Scan Types and API**:
  - `include/ethercat/scan.h` - Network scanning public API
  - SII (EEPROM) category definitions
  - Slave discovery structures
  - Port descriptor definitions

- **Scan Implementation** (Full Version):
  - `src/master/scan.c` - Network scanning implementation (735 lines)
  - Complete implementation with all EtherCAT commands
  - Full frame_builder/parser integration

- **Implemented Features**:
  - ✅ Slave discovery using BRD (Broadcast Read)
  - ✅ Station address assignment using APWR (Auto-increment Physical Write)
  - ✅ Register reading using APRD (Auto-increment Physical Read)
  - ✅ Register access using FPRD/FPWR (Configured Physical Read/Write)
  - ✅ EEPROM (SII) reading via ESC registers with full state machine
  - ✅ Slave identification (Vendor ID, Product Code, Revision, Serial Number)
  - ✅ Slave name reading from EEPROM
  - ✅ Topology detection via port descriptors
  - ✅ Category-based EEPROM reading

- **API Functions**:
  - `scan_init()` / `scan_shutdown()` - Scan module lifecycle
  - `scan_discover_slaves()` - Discover all slaves using BRD
  - `scan_get_discovery_info()` - Get discovery information for a slave
  - `scan_assign_station_addresses()` - Assign station addresses using APWR
  - `scan_read_eeprom_word()` - Read EEPROM word with SII state machine
  - `scan_read_slave_id()` - Read slave identification
  - `scan_read_slave_name()` - Read slave name from STRINGS category
  - `scan_read_eeprom_category()` - Read EEPROM category data
  - `scan_detect_topology()` - Detect network topology

- **Implementation Details**:
  - **BRD Command**: Broadcasts Type register read, Working Counter = slave count
  - **APRD/APWR**: Auto-increment addressing with negative position `-(i+1)`
  - **FPRD/FPWR**: Configured addressing with station address (0x1000+)
  - **SII State Machine**: Write Address → Write Control → Poll BUSY → Read Data
  - **Address Format**: `(address_high << 16) | register_offset`
  - **Timeout Handling**: Uses HAL time functions for precise timeout control
  - **Frame Management**: Proper RX buffer allocation and deallocation

##### Phase 2.3 - Mailbox Communication ✅
- **Implementation**:
  - `src/al/al_mailbox.c` - Mailbox implementation (267 lines)

- **Features**:
  - Mailbox send/receive
  - Protocol type support (CoE, FoE, SoE, VoE, EoE, AoE)
  - Mailbox state machine
  - Protocol support checking

##### Phase 2.4 - Sync Manager & Register Access ✅
- **Implementation**:
  - `src/al/al_reg.c` - Register access implementation (394 lines)

- **Features**:
  - AL Control/Status register access
  - Sync Manager configuration (16 SMs)
  - 8/16/32-bit register read/write
  - Block data transfer
  - SII (EEPROM) interface (reserved)

#### Phase 5 - Process Data and Cyclic Operation ✅ (Phase 5.1 Complete)

##### Phase 5.1 - Process Data Core Infrastructure ✅
- **Process Data Types and API**:
  - `include/ethercat/process_data.h` - Process data API definitions (310 lines)
  - Process data status codes
  - Process data image structure
  - Slave mapping structure
  - Statistics structure
  - Redundancy types (for Phase 5.2)

- **Process Data Implementation**:
  - `src/master/process_data.c` - Process data implementation (470 lines)

- **Features**:
  - Process data image allocation/deallocation
  - Slave mapping validation
  - LRW (Logical Read/Write) command for cyclic data exchange
  - Working counter validation
  - Cycle time statistics (min/max/avg)
  - Error tracking (WKC errors, timeouts)
  - Redundancy stubs for Phase 5.2

- **API Functions**:
  - `pd_init()` / `pd_shutdown()` - Module lifecycle
  - `pd_allocate_image()` / `pd_free_image()` - Image management
  - `pd_map_slave()` - Slave mapping
  - `pd_exchange()` - LRW data exchange
  - `pd_validate_wkc()` - Working counter validation
  - `pd_get_statistics()` / `pd_reset_statistics()` - Statistics

##### Phase 5.1 - Master Integration ✅
- **Master Process Data API**:
  - `master_allocate_process_data()` - Allocate PD buffers
  - `master_free_process_data()` - Free PD buffers
  - `master_get_process_data_image()` - Get PD image pointer
  - `master_write_slave_output()` - Write to slave output
  - `master_read_slave_input()` - Read from slave input
  - `master_get_cyclic_statistics()` - Get cycle statistics

- **Features**:
  - Process data integration into master lifecycle
  - Cyclic operation with LRW command
  - Working counter validation in cycle processing
  - Statistics tracking for performance monitoring

### Test Summary

| Module | Tests | Passed | Failed | Status |
|--------|-------|--------|--------|--------|
| DLL State Machine | 28 | 28 | 0 | ✅ PASS |
| DLL Queue Management | 55 | 55 | 0 | ✅ PASS |
| DLL Frame Protocol | 53 | 53 | 0 | ✅ PASS |
| DLL Integration | 29 | 29 | 0 | ✅ PASS |
| **Total** | **165** | **165** | **0** | **✅ PASS** |

### Build Statistics

```
Source Files:
  DLL:      11 files
  HAL:      3 files
  AL:       5 files
  Master:   5 files (master.c, scan.c, config.c, dc.c, process_data.c)
  Examples: 3 files (simple_cyclic.c, process_data_demo.c, benchmark.c)
  Total:    24 library files + 3 example files

Lines of Code:
  DLL:      ~3,500 lines
  HAL:      ~800 lines
  AL:       ~1,340 lines
  CoE:      ~753 lines
  Master:   ~3,028 lines (master.c 461 + scan.c 735 + config.c 668 + dc.c 694 + process_data.c 470)
  Examples: ~1,320 lines (simple_cyclic.c 380 + process_data_demo.c 450 + benchmark.c 490)
  Total:    ~10,741 lines

Build Status: ✅ Success (0 errors, 0 warnings)
Output:
  Library:  build/lib/libethercat.a (498KB)
  Examples: build/bin/simple_cyclic (245KB)
            build/bin/process_data_demo (241KB)
            build/bin/benchmark (249KB)
```

## Documentation

- **CLAUDE.md** - Project guidance and development workflow
- **Spec.md** - Technical specification (complete for Phase 1-2)
- **Plan.md** - Implementation plan (5 phases with milestones)
- **TESTING.md** - Testing and benchmarking guide
- **REDUNDANCY.md** - Redundancy support design document

## Project Structure

```
ethercat-master/
├── CLAUDE.md                    # Project guidance
├── Spec.md                      # Technical specification
├── Plan.md                      # Implementation plan
├── Makefile                     # Build system
├── README.md                    # This file
├── include/ethercat/            # Public headers
│   ├── dll.h                    # Main DLL interface
│   ├── dll_types.h              # DLL type definitions
│   ├── dll_config.h             # DLL configuration
│   ├── dll_errors.h             # DLL error handling
│   ├── dll_state.h              # DLL state machine
│   ├── dll_queue.h              # DLL queue management
│   ├── frame.h                  # Frame structures
│   ├── frame_builder.h          # Frame builder
│   ├── frame_parser.h           # Frame parser
│   ├── hal_types.h              # HAL type definitions
│   ├── hal.h                    # HAL interface
│   ├── al_types.h               # AL type definitions
│   ├── al.h                     # AL interface
│   ├── coe.h                    # CoE interface
│   ├── master.h                 # Master control interface
│   ├── scan.h                   # Network scanning interface
│   ├── config.h                 # Slave configuration interface
│   ├── dc.h                     # Distributed Clocks interface
│   └── process_data.h           # Process data interface
├── src/dll/                     # DLL implementation
│   ├── dll_init.c               # Initialization
│   ├── dll_tx.c                 # Transmission
│   ├── dll_rx.c                 # Reception
│   ├── dll_control.c            # Control functions
│   ├── dll_error.c              # Error handling
│   ├── dll_config.c             # Configuration
│   ├── dll_state.c              # State machine
│   ├── dll_queue.c              # Queue management
│   ├── frame.c                  # Frame utilities
│   ├── frame_builder.c          # Frame builder
│   ├── frame_parser.c           # Frame parser
│   └── dll_internal.h           # Internal definitions
├── src/hal/                     # HAL implementation
│   ├── hal.c                    # HAL core
│   ├── hal_linux.c              # Linux platform
│   ├── hal_stub.c               # Stub platform
│   └── hal_internal.h           # Internal definitions
├── src/al/                      # AL implementation
│   ├── al.c                     # AL core
│   ├── al_state.c               # State machine
│   ├── al_mailbox.c             # Mailbox communication
│   ├── al_reg.c                 # Register access
│   ├── al_internal.h            # Internal definitions
│   └── coe.c                    # CoE implementation
├── src/master/                  # Master implementation
│   ├── master.c                 # Master control core
│   ├── master_internal.h        # Master internal definitions
│   ├── scan.c                   # Network scanning
│   ├── config.c                 # Slave configuration
│   ├── dc.c                     # Distributed Clocks
│   └── process_data.c           # Process data and cyclic operation
├── tests/dll/                   # Unit tests
│   ├── test_dll_state.c         # State machine tests
│   ├── test_dll_queue.c         # Queue tests
│   ├── test_frame.c             # Frame tests
│   └── test_dll_integration.c   # Integration tests
├── build/                       # Build output
│   ├── lib/libethercat.a        # Static library
│   ├── bin/                     # Test and example binaries
│   └── obj/                     # Object files
├── specs/                       # ETG1000 specifications
│   ├── ETG1000_1_*.pdf          # Overview
│   ├── ETG1000_2_*.pdf          # Physical Layer
│   ├── ETG1000_3_*.pdf          # DLL Services
│   ├── ETG1000_4_*.pdf          # DLL Protocols
│   ├── ETG1000_5_*.pdf          # AL Services
│   └── ETG1000_6_*.pdf          # AL Protocols
└── examples/                    # Example applications
    ├── README.md                # Example documentation
    ├── simple_cyclic.c          # Simple cyclic I/O example
    ├── process_data_demo.c      # Process data demonstration
    └── benchmark.c              # Performance benchmark tool
```

## Building

### Requirements
- GCC with C11 support
- Make
- POSIX-compliant system (Linux, macOS, etc.)

### Build Commands

```bash
# Build library (debug mode)
make lib

# Build and run tests
make test

# Build example applications
make examples

# Build release version
make release

# Clean build artifacts
make clean

# Show build configuration
make info
```

## Next Steps

### Phase 5.1 - Process Data and Cyclic Operation ✅ Complete

**Phase 5.1 - Basic Process Data** ✅ Complete:
- ✅ Process data types and API defined (process_data.h)
- ✅ Process data core implementation (process_data.c)
- ✅ Image allocation/deallocation with dynamic memory
- ✅ Slave mapping validation
- ✅ LRW (Logical Read/Write) command implementation
- ✅ Frame building/parsing for LRW using existing infrastructure
- ✅ Data exchange function with timeout handling
- ✅ Working counter validation
- ✅ Statistics and monitoring (cycle time, WKC errors, timeouts)
- ✅ Master integration (6 new API functions)
- ✅ Basic cyclic operation working

### Phase 5.2 - Redundancy Support (Design Complete)
- ✅ Redundancy architecture designed (REDUNDANCY.md)
- ✅ Multi-port HAL API defined (hal.h)
- ✅ HAL stub functions implemented (hal.c)
- ✅ Process data redundancy API ready (process_data.h)
- ⏳ Full implementation (TODO - requires dual network interfaces)
- ⏳ Cable redundancy mode (TODO)
- ⏳ Frame redundancy mode (TODO)
- ⏳ Hot connect support (TODO)
- ⏳ Testing with hardware (TODO - requires ring topology)

### Phase 5.3 - Examples and Testing ✅ Complete
- ✅ Simple cyclic I/O example (simple_cyclic.c - 380 lines)
- ✅ Process data demonstration (process_data_demo.c - 450 lines)
- ✅ Performance benchmark tool (benchmark.c - 490 lines)
- ✅ Example documentation (examples/README.md)
- ✅ Testing documentation (TESTING.md)
- ✅ Build integration (Makefile)
- ✅ Performance benchmarks documented
- ⏳ System testing with real hardware (TODO - requires hardware)

### Phase 3 - Remaining Application Layer Protocols (All Optional)

**Phase 3.2 - FoE (File over EtherCAT)**:
- File read/write
- Firmware update
- Progress callbacks

**Phase 3.3 - SoE (Servo over EtherCAT)**:
- IDN access
- Servo drive parameters

**Phase 3.4 - EoE (Ethernet over EtherCAT)**:
- Ethernet frame tunneling
- IP-based communication
- Fragment handling

**Phase 3.5 - AoE (ADS over EtherCAT)**:
- TwinCAT ADS protocol
- Read/Write commands
- Device notifications

**Phase 3.6 - VoE (Vendor specific over EtherCAT)**:
- Vendor-specific protocols

## Technical Specifications

### Based On
- **ETG1000.3** - EtherCAT Data Link Layer Services (Version 1.0.4)
- **ETG1000.4** - EtherCAT Data Link Layer Protocols (Version 1.0.4)
- **ETG1000.5** - EtherCAT Application Layer Services (Version 1.0.4)
- **ETG1000.6** - EtherCAT Application Layer Protocols (Version 1.0.4)

### Standards
- **Language**: C11
- **Target**: Embedded systems
- **Architecture**: Modular, layered design
- **Memory**: Static allocation preferred (embedded-friendly)

### Key Features
- Real-time capable
- Deterministic execution
- Low memory footprint
- Platform abstraction (HAL)
- Comprehensive error handling
- Extensive unit testing
- State machine-based design

## License

This is a clean-room implementation based on publicly available ETG specifications.

## References

- ETG1000 Series Specifications (Version 1.0.4)
- EtherCAT Technology Group (www.ethercat.org)

## Running Examples

See `examples/README.md` for detailed documentation on running the example applications.

Quick start:
```bash
# Build examples
make examples

# Run simple cyclic I/O example (requires root)
sudo ./build/bin/simple_cyclic

# Run process data demonstration (requires root)
sudo ./build/bin/process_data_demo
```

---

**Last Updated**: 2026-01-04
**Version**: 5.4.0 (Phase 1-2 Complete, Phase 3.1 CoE Complete, Phase 4 Complete, Phase 5.1 Complete, Phase 5.2 Design Complete, Phase 5.3 Complete)
