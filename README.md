# EtherCAT Master Stack

A clean-room implementation of an EtherCAT Master stack in C11 for embedded systems.

## Project Status

**Current Phase**: Phase 1 - Data Link Layer Services + Protocol (60% Complete)

### Completed Components

#### Phase 1.1 - Core Data Structures ✅
- **Header Files**:
  - `include/ethercat/dll_types.h` - All DLL type definitions (enums, structs, callbacks)
  - `include/ethercat/dll_errors.h` - Error handling interface
  - `include/ethercat/dll_config.h` - Configuration interface and defaults
  - `include/ethercat/dll.h` - Main DLL interface

- **Implementation Files**:
  - `src/dll/dll_error.c` - Error handling with callback support
  - `src/dll/dll_config.c` - Configuration validation and parameter access

- **Test Results**: All data structures compile successfully

#### Phase 1.2 - State Machine ✅
- **Header Files**:
  - `include/ethercat/dll_state.h` - State machine interface

- **Implementation Files**:
  - `src/dll/dll_state.c` - Complete state machine with transition validation

- **Features**:
  - 5 states: UNINITIALIZED, INITIALIZED, READY, RUNNING, ERROR
  - Transition validation table
  - State change callbacks
  - Thread-safe state access

- **Test Results**: 28/28 tests passed ✅

#### Phase 1.3 - Queue Management ✅
- **Header Files**:
  - `include/ethercat/dll_queue.h` - Queue management interface

- **Implementation Files**:
  - `src/dll/dll_queue.c` - Circular buffer with priority support

- **Features**:
  - Circular buffer implementation
  - Priority queue support for TX
  - FIFO ordering for RX
  - Queue statistics (count, capacity, empty/full checks)
  - Peek and flush operations

- **Test Results**: 55/55 tests passed ✅

#### Phase 2 - EtherCAT Frame Protocol ✅
- **Header Files**:
  - `include/ethercat/frame.h` - Frame and datagram structures
  - `include/ethercat/frame_builder.h` - Frame builder interface
  - `include/ethercat/frame_parser.h` - Frame parser interface

- **Implementation Files**:
  - `src/dll/frame.c` - Frame utility functions
  - `src/dll/frame_builder.c` - Frame builder implementation
  - `src/dll/frame_parser.c` - Frame parser implementation

- **Features**:
  - Complete EtherCAT frame structure
  - 15 datagram command types
  - 3 addressing modes (auto-increment, configured, logical)
  - Working counter handling
  - CRC32 calculation and verification
  - Frame builder and parser

- **Test Results**: 53/53 tests passed ✅

#### Build System ✅
- **Makefile** with targets:
  - `make lib` - Build static library
  - `make test` - Build and run unit tests
  - `make debug` - Debug build
  - `make release` - Release build
  - `make clean` - Clean build artifacts
  - `make info` - Show build configuration

- **Build Output**: `build/lib/libethercat.a`

### Test Summary

| Module | Tests | Passed | Failed | Status |
|--------|-------|--------|--------|--------|
| State Machine | 28 | 28 | 0 | ✅ PASS |
| Queue Management | 55 | 55 | 0 | ✅ PASS |
| Frame Protocol | 53 | 53 | 0 | ✅ PASS |
| **Total** | **136** | **136** | **0** | **✅ PASS** |

## Documentation

- **CLAUDE.md** - Project guidance and development workflow
- **Spec.md** - Technical specification (functions, structures, state machines)
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
│   ├── dll_types.h              # Type definitions
│   ├── dll_config.h             # Configuration
│   ├── dll_errors.h             # Error handling
│   ├── dll_state.h              # State machine
│   ├── dll_queue.h              # Queue management
│   ├── frame.h                  # Frame structures
│   ├── frame_builder.h          # Frame builder
│   └── frame_parser.h           # Frame parser
├── src/dll/                     # DLL implementation
│   ├── dll_error.c              # Error handling
│   ├── dll_config.c             # Configuration
│   ├── dll_state.c              # State machine
│   ├── dll_queue.c              # Queue management
│   ├── frame.c                  # Frame utilities
│   ├── frame_builder.c          # Frame builder
│   └── frame_parser.c           # Frame parser
├── tests/dll/                   # Unit tests
│   ├── test_dll_state.c         # State machine tests
│   ├── test_dll_queue.c         # Queue tests
│   └── test_frame.c             # Frame tests
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

### Phase 1.4 - Initialization and Configuration (Next)
- Implement `dl_init()` and `dl_shutdown()`
- Implement parameter get/set functions
- Integrate state machine and queues
- Add resource management

### Phase 1.5 - Frame Transmission
- Implement `dl_send_req()`
- Implement send confirmation callbacks
- Integrate with TX queue

### Phase 1.6 - Frame Reception
- Implement frame reception handler
- Implement receive indication callbacks
- Integrate with RX queue

### Phase 1.7 - Control Functions
- Implement `dl_start()`, `dl_stop()`, `dl_reset()`
- Complete state machine integration

### Phase 1.8 - Statistics and Diagnostics
- Implement statistics collection
- Add timing measurements

### Phase 1.9 - Error Handling
- Complete error recovery logic
- Add error logging

### Phase 1.10 - Hardware Abstraction Layer
- Define HAL interface
- Implement platform-specific drivers

## Technical Specifications

### Based On
- **ETG1000.3** - EtherCAT Data Link Layer Services (Version 1.0.4)

### Standards
- **Language**: C11
- **Target**: Embedded systems
- **Architecture**: Modular, layered design
- **Memory**: Static allocation preferred (embedded-friendly)

### Key Features
- Real-time capable
- Deterministic execution
- Low memory footprint
- Comprehensive error handling
- Extensive unit testing

## License

This is a clean-room implementation based on publicly available ETG specifications.

## References

- ETG1000 Series Specifications (Version 1.0.4)
- EtherCAT Technology Group (www.ethercat.org)

---

**Last Updated**: 2026-01-03
**Version**: 1.0.0 (Phase 1 - Partial)
