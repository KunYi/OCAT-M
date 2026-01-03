# EtherCAT Master Stack

A clean-room implementation of an EtherCAT Master stack in C11 for embedded systems.

## Project Status

**Current Phase**: Phase 4 Complete - Network Scanning and Master Control ✅

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
  Master:   2 files
  Total:    21 files

Lines of Code:
  DLL:      ~3,500 lines
  HAL:      ~800 lines
  AL:       ~1,340 lines
  CoE:      ~753 lines
  Master:   ~1,196 lines
  Total:    ~7,589 lines

Build Status: ✅ Success (0 errors, 0 warnings)
Output:       build/lib/libethercat.a
```

## Documentation

- **CLAUDE.md** - Project guidance and development workflow
- **Spec.md** - Technical specification (complete for Phase 1-2)
- **Plan.md** - Implementation plan (5 phases with milestones)

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
│   └── coe.h                    # CoE interface
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
├── tests/dll/                   # Unit tests
│   ├── test_dll_state.c         # State machine tests
│   ├── test_dll_queue.c         # Queue tests
│   ├── test_frame.c             # Frame tests
│   └── test_dll_integration.c   # Integration tests
├── build/                       # Build output
│   ├── lib/libethercat.a        # Static library
│   ├── bin/                     # Test binaries
│   └── obj/                     # Object files
├── specs/                       # ETG1000 specifications
│   ├── ETG1000_1_*.pdf          # Overview
│   ├── ETG1000_2_*.pdf          # Physical Layer
│   ├── ETG1000_3_*.pdf          # DLL Services
│   ├── ETG1000_4_*.pdf          # DLL Protocols
│   ├── ETG1000_5_*.pdf          # AL Services
│   └── ETG1000_6_*.pdf          # AL Protocols
└── examples/                    # Example applications (TBD)
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

# Build release version
make release

# Clean build artifacts
make clean

# Show build configuration
make info
```

## Next Steps

### Phase 4 - Network Scanning and Master Control (Mostly Complete ✅)

**Phase 4.1 - Network Scanning** ✅ Complete:
- ✅ Slave discovery using BRD (Broadcast Read)
- ✅ Station address assignment using APWR
- ✅ EEPROM (SII) reading with full state machine
- ✅ Slave identification (Vendor ID, Product Code, Revision, Serial)
- ✅ Slave name reading from EEPROM
- ✅ Topology detection via port descriptors
- ✅ Category-based EEPROM reading

**Phase 4.2 - Slave Configuration** (Next Priority - Recommended):
- Sync Manager configuration
- PDO mapping configuration
- Mailbox configuration
- FMMU configuration

**Phase 4.3 - Distributed Clocks** (Optional):
- DC synchronization
- Drift compensation
- DC monitoring

### Phase 5 - Cyclic Operation and Process Data (Recommended)
- LRW (Logical Read/Write) for process data
- Cyclic frame transmission
- Working counter validation
- Real-time performance optimization

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

---

**Last Updated**: 2026-01-03
**Version**: 3.1.0 (Phase 1-2 Complete, Phase 3.1 CoE Complete, Phase 4 Network Scanning Complete)
