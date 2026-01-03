# Phase 5 - Process Data and Cyclic Operation Design

## Document Information
- **Version**: 1.0.0
- **Date**: 2026-01-03
- **Status**: Design Complete - Ready for Implementation Review

---

## Executive Summary

Phase 5 implements the core cyclic operation functionality for the EtherCAT Master, enabling real-time process data exchange with slaves. The design is split into two sub-phases:

- **Phase 5.1 (HIGH Priority)**: Basic process data exchange using LRW command
- **Phase 5.2 (LOW Priority)**: Optional redundancy support for high-availability systems

---

## Design Overview

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Master Application                        │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│              Master API (master.h)                           │
│  - master_allocate_process_data()                           │
│  - master_write_slave_output()                              │
│  - master_read_slave_input()                                │
│  - master_process_cycle()                                   │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│         Process Data Layer (process_data.h)                  │
│  - pd_allocate_image()                                      │
│  - pd_map_slave()                                           │
│  - pd_exchange()          [LRW Command]                     │
│  - pd_validate_wkc()                                        │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│              Frame Builder/Parser                            │
│  - Build LRW datagram                                       │
│  - Parse response                                           │
│  - Extract working counter                                  │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                   DLL Layer                                  │
│  - dl_send_req()                                            │
│  - Frame transmission/reception                             │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                   HAL Layer                                  │
│  - hal_send_frame()                                         │
│  - hal_receive_frame()                                      │
└─────────────────────────────────────────────────────────────┘
```

---

## Phase 5.1 - Basic Process Data (HIGH Priority)

### Objectives

1. Implement LRW (Logical Read/Write) command for process data exchange
2. Provide process data image management
3. Enable cyclic operation with working counter validation
4. Integrate with existing master control

### Key Components

#### 1. Process Data Image

```c
typedef struct {
    uint8_t* input_data;        // Input from slaves
    uint32_t input_size;        // Input size in bytes
    uint8_t* output_data;       // Output to slaves
    uint32_t output_size;       // Output size in bytes
    uint32_t logical_address;   // Logical memory address (0x00000000)
} pd_image_t;
```

**Purpose**: Represents the complete I/O memory space for all slaves.

#### 2. Slave Mapping

```c
typedef struct {
    uint16_t station_address;
    uint32_t input_offset;      // Offset in input_data
    uint32_t input_size;
    uint32_t output_offset;     // Offset in output_data
    uint32_t output_size;
} pd_slave_mapping_t;
```

**Purpose**: Maps each slave's data to specific offsets in the process data image.

#### 3. LRW Command

**Frame Structure**:
```
LRW Datagram:
├── Command: 0x0C (LRW)
├── Index: Frame index
├── Address: 0x00000000 (logical address)
├── Length: Total data size
├── Data: Output data (written by master, read by slaves)
└── WKC: Working Counter (incremented by slaves)
```

**Operation**:
1. Master sends LRW with output data
2. Each slave reads output, writes input, increments WKC by 2
3. Master receives frame with input data and WKC
4. Master validates WKC (expected = slave_count × 2)

#### 4. API Functions

**Core Functions**:
- `pd_init()` / `pd_shutdown()` - Module lifecycle
- `pd_allocate_image()` / `pd_free_image()` - Memory management
- `pd_map_slave()` - Configure slave mappings
- `pd_exchange()` - Execute LRW command
- `pd_validate_wkc()` - Validate working counter
- `pd_get_statistics()` - Get cycle statistics

**Master Integration**:
- `master_allocate_process_data()` - Allocate based on slave config
- `master_write_slave_output()` - Write to specific slave
- `master_read_slave_input()` - Read from specific slave
- `master_process_cycle()` - Execute one cycle

### Implementation Steps

1. **Define API** (process_data.h) - 1 day
2. **Implement image management** - 1 day
3. **Implement LRW command** - 2 days
4. **Implement WKC validation** - 1 day
5. **Implement statistics** - 1 day
6. **Master integration** - 2 days
7. **Testing and validation** - 2 days

**Total Estimated Effort**: 10 days

### Testing Strategy

- Unit tests for image allocation/deallocation
- Unit tests for slave mapping
- Unit tests for WKC validation
- Integration tests with mock slaves
- Performance tests (cycle time, jitter)

---

## Phase 5.2 - Redundancy Support (LOW Priority - Optional)

### Objectives

1. Support multiple network interfaces (dual-port)
2. Implement cable redundancy (ring topology)
3. Implement frame redundancy (dual send)
4. Support hot connect/disconnect

### Key Components

#### 1. Redundancy Modes

```c
typedef enum {
    PD_REDUNDANCY_NONE = 0,      // Single port (default)
    PD_REDUNDANCY_CABLE,         // Ring topology with auto-recovery
    PD_REDUNDANCY_FRAME,         // Dual send, first response wins
    PD_REDUNDANCY_HOT_CONNECT    // Dynamic slave connection
} pd_redundancy_mode_t;
```

#### 2. Multi-Port HAL

**New HAL Functions**:
- `hal_init_multiport()` - Initialize multiple interfaces
- `hal_send_frame_port()` - Send on specific port
- `hal_receive_frame_port()` - Receive from specific port
- `hal_is_port_link_up()` - Check port status
- `hal_get_port_statistics()` - Per-port statistics

#### 3. Port Management

**Features**:
- Automatic port health monitoring
- Automatic failover on port failure
- Manual port switching
- Per-port statistics

### Implementation Steps

1. **Define redundancy types** - 1 day
2. **Implement multi-port HAL** - 3 days
3. **Implement port switching** - 2 days
4. **Implement automatic failover** - 2 days
5. **Testing and validation** - 2 days

**Total Estimated Effort**: 10 days

### Testing Strategy

- Test single port mode (backward compatibility)
- Test dual port initialization
- Test cable break scenarios
- Test automatic failover
- Test manual port switching

---

## Implementation Priority

### Recommended Approach

**Step 1: Implement Phase 5.1 (HIGH Priority)**
- Essential for functional EtherCAT master
- Required for real-time applications
- Foundation for all cyclic operations
- **Start immediately after Phase 4 completion**

**Step 2: Validate with Real Slaves**
- Test with actual EtherCAT slaves
- Measure performance (cycle time, jitter)
- Verify working counter behavior
- Identify any issues

**Step 3: Decide on Phase 5.2 (LOW Priority)**
- Evaluate application requirements
- Determine if redundancy is needed
- Consider cost/benefit trade-off
- **Only implement if high-availability is required**

### Decision Criteria for Phase 5.2

**Implement Phase 5.2 if**:
- Application requires high availability (>99.9% uptime)
- System must tolerate cable breaks
- Hot-swapping of slaves is needed
- Dual network interfaces are available

**Skip Phase 5.2 if**:
- Single port is sufficient
- High availability not critical
- Cost/complexity not justified
- Can be added later if needed

---

## Performance Targets

### Phase 5.1 Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Cycle Time | <100μs | For typical setup (4-8 slaves) |
| Jitter | <10μs | Standard deviation |
| Latency | <50μs | Master to slave round-trip |
| CPU Usage | <5% | At 1kHz cycle rate |
| Memory | <1MB | For process data buffers |

### Phase 5.2 Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Failover Time | <10ms | Port switch time |
| Redundancy Overhead | <10% | Additional CPU/memory |
| Port Switch Count | <100/day | Under normal operation |

---

## Risk Assessment

### Phase 5.1 Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| LRW timing issues | Medium | High | Extensive testing with real slaves |
| WKC validation errors | Low | Medium | Implement robust error handling |
| Performance not meeting targets | Low | High | Profile and optimize early |
| Integration issues | Low | Medium | Incremental integration approach |

### Phase 5.2 Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Multi-port HAL complexity | High | Medium | Start with simple implementation |
| Port switching instability | Medium | High | Extensive failover testing |
| Backward compatibility issues | Low | High | Maintain single-port mode |
| Platform-specific issues | Medium | Medium | Abstract platform differences |

---

## Dependencies

### Phase 5.1 Dependencies

**Required (Already Complete)**:
- ✅ Phase 1: DLL and HAL
- ✅ Phase 2: Application Layer
- ✅ Phase 3.1: CoE
- ✅ Phase 4: Master Control, Scanning, Configuration, DC

**No Blockers**: Phase 5.1 can start immediately

### Phase 5.2 Dependencies

**Required**:
- ✅ Phase 5.1 complete
- ⏳ Multi-interface hardware available
- ⏳ Platform support for multiple NICs

**Potential Blockers**:
- Hardware availability
- Platform limitations
- Driver support

---

## Documentation Updates

### Files to Update

1. **Spec.md** - ✅ Complete
   - Added Process Data section (5.1-5.8)
   - Defined all API functions
   - Documented LRW command
   - Added redundancy specifications

2. **Plan.md** - ✅ Complete
   - Added Phase 5 detailed plan
   - Defined milestones (5.1.1-5.1.14, 5.2.1-5.2.10)
   - Added implementation steps
   - Updated revision history

3. **README.md** - ⏳ To be updated after implementation
   - Update project status
   - Add Phase 5 to completed sections
   - Update build statistics

---

## Conclusion

### Summary

Phase 5 design is complete and documented in Spec.md and Plan.md. The design provides:

1. **Clear API definition** for process data operations
2. **Detailed implementation plan** with milestones
3. **Priority guidance** (Phase 5.1 HIGH, Phase 5.2 LOW)
4. **Testing strategy** for validation
5. **Performance targets** for evaluation

### Recommendation

**Proceed with Phase 5.1 implementation immediately**:
- All dependencies are satisfied
- Design is well-defined and documented
- Implementation path is clear
- Essential for functional master

**Defer Phase 5.2 until after Phase 5.1 validation**:
- Not required for basic operation
- Can be added later if needed
- Requires additional hardware/testing
- Adds complexity

### Next Steps

1. ✅ Review and approve Phase 5 design (this document)
2. ⏳ Begin Phase 5.1 implementation
3. ⏳ Create process_data.h with API definitions
4. ⏳ Implement process_data.c core functionality
5. ⏳ Integrate with master.c
6. ⏳ Test with real EtherCAT slaves
7. ⏳ Evaluate need for Phase 5.2

---

**Document Status**: Ready for Review and Implementation
