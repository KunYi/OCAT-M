# EtherCAT Redundancy Support - Design Document

## Document Information
- **Version**: 1.0.0
- **Date**: 2026-01-04
- **Status**: Design Phase
- **Implementation**: Stub/Future Work

---

## Table of Contents
1. [Overview](#overview)
2. [Redundancy Modes](#redundancy-modes)
3. [Architecture](#architecture)
4. [API Design](#api-design)
5. [Implementation Plan](#implementation-plan)
6. [Testing Strategy](#testing-strategy)
7. [Performance Considerations](#performance-considerations)

---

## Overview

### Purpose

EtherCAT redundancy support enables fault-tolerant communication through multiple network paths. This ensures continuous operation even when network cables are damaged or disconnected.

### Use Cases

1. **Critical Industrial Systems**: Manufacturing lines that cannot tolerate downtime
2. **Safety Systems**: Emergency shutdown systems requiring 99.999% availability
3. **High-Availability Automation**: Systems where cable breaks must not cause failures
4. **Ring Topology Networks**: Large installations with ring-based cable routing

### Requirements

- **Hardware**: Two network interfaces (NICs) on the master
- **Topology**: Ring topology with slaves connected in a loop
- **Performance**: Minimal overhead (<5% CPU increase)
- **Failover Time**: <10ms for automatic port switching
- **Backward Compatibility**: Must work with existing single-port code

---

## Redundancy Modes

### 1. Cable Redundancy (Ring Topology)

**Description**: Slaves are connected in a ring. Master sends frames on primary port, receives on secondary port (or vice versa).

**Topology**:
```
    Master
   /      \
  P0      P1
  |        |
  S1 ---- S2
  |        |
  S3 ---- S4
```

**Operation**:
- Normal: Frames sent on P0, received on P1 (or opposite)
- Cable break: Automatically switch to working port
- Both ports active: Ring is complete

**Advantages**:
- No frame duplication
- Efficient bandwidth usage
- Automatic failover

**Disadvantages**:
- Requires ring topology
- Single point of failure (master)

### 2. Frame Redundancy (Dual Send)

**Description**: Master sends identical frames on both ports simultaneously. First received frame is used, duplicate is discarded.

**Topology**:
```
    Master
   /      \
  P0      P1
  |        |
  S1      S2
  |        |
  S3      S4
```

**Operation**:
- Send frame on both P0 and P1
- Receive from both ports
- Use first valid response
- Discard duplicate

**Advantages**:
- Works with any topology
- Instant failover (no switching delay)
- Maximum reliability

**Disadvantages**:
- Double bandwidth usage
- Frame filtering required
- Higher CPU overhead

### 3. Hot Connect

**Description**: Support for connecting/disconnecting network cables during operation without stopping cyclic communication.

**Operation**:
- Monitor link status continuously
- Detect cable connect/disconnect
- Automatically switch to active port
- Resume communication without errors

**Advantages**:
- Maintenance without downtime
- Flexible cable management

**Disadvantages**:
- Requires link monitoring
- Brief communication interruption

---

## Architecture

### Component Overview

```
┌─────────────────────────────────────────────────┐
│                Master Application               │
└────────────────────┬────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────┐
│            Process Data Layer (PD)              │
│  - pd_exchange_port()                           │
│  - pd_switch_port()                             │
│  - pd_check_port_health()                       │
└────────────────────┬────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────┐
│      Hardware Abstraction Layer (HAL)           │
│  - hal_init_multiport()                         │
│  - hal_send_frame_port()                        │
│  - hal_receive_frame_port()                     │
│  - hal_is_port_link_up()                        │
└────────────────────┬────────────────────────────┘
                     │
         ┌───────────┴───────────┐
         ▼                       ▼
    ┌────────┐             ┌────────┐
    │ Port 0 │             │ Port 1 │
    │ (eth0) │             │ (eth1) │
    └────────┘             └────────┘
```

### Data Structures

#### Redundancy Configuration
```c
typedef struct {
    pd_redundancy_mode_t mode;          // NONE, CABLE, FRAME, HOT_CONNECT
    pd_port_select_t active_port;       // PRIMARY, SECONDARY, AUTO
    bool auto_switch;                   // Enable automatic failover
    uint32_t switch_threshold_ms;       // Time before switching (default: 100ms)
    uint32_t health_check_interval_ms;  // Health check frequency (default: 1000ms)
} pd_redundancy_config_t;
```

#### Port Status
```c
typedef struct {
    bool link_up;                       // Physical link status
    bool active;                        // Port is currently active
    uint64_t frames_sent;               // Total frames sent
    uint64_t frames_received;           // Total frames received
    uint64_t errors;                    // Error count
    uint32_t last_wkc;                  // Last working counter
    uint64_t last_success_time_ns;      // Last successful exchange
} pd_port_status_t;
```

### State Machine

```
┌─────────────┐
│   INIT      │
└──────┬──────┘
       │ hal_init_multiport()
       ▼
┌─────────────┐
│ SINGLE_PORT │◄──────────────┐
└──────┬──────┘                │
       │ Both ports OK         │ Port failure
       ▼                       │
┌─────────────┐                │
│  DUAL_PORT  │────────────────┘
└──────┬──────┘
       │ Cable break detected
       ▼
┌─────────────┐
│  FAILOVER   │
└──────┬──────┘
       │ Switch complete
       ▼
┌─────────────┐
│ SINGLE_PORT │
└─────────────┘
```

---

## API Design

### HAL Multi-Port Functions

```c
// Initialize with two ports
hal_status_t hal_init_multiport(
    const hal_config_t* primary_config,
    const hal_config_t* secondary_config
);

// Send on specific port
hal_status_t hal_send_frame_port(
    hal_frame_buffer_t* buffer,
    uint8_t port  // 0=primary, 1=secondary
);

// Receive from specific port
hal_status_t hal_receive_frame_port(
    hal_frame_buffer_t** buffer,
    uint8_t port
);

// Check port link status
bool hal_is_port_link_up(uint8_t port);

// Get port statistics
hal_status_t hal_get_port_statistics(
    uint8_t port,
    hal_statistics_t* stats
);

// Get number of ports
uint8_t hal_get_port_count(void);
```

### Process Data Redundancy Functions

```c
// Exchange on specific port
pd_status_t pd_exchange_port(
    pd_image_t* image,
    pd_port_select_t port,
    uint16_t* working_counter,
    uint32_t timeout_ms
);

// Switch active port
pd_status_t pd_switch_port(
    pd_image_t* image,
    pd_port_select_t new_port
);

// Check port health
pd_status_t pd_check_port_health(
    pd_image_t* image,
    pd_port_select_t port,
    bool* healthy
);

// Get port status
pd_status_t pd_get_port_status(
    pd_image_t* image,
    pd_port_select_t port,
    pd_port_status_t* status
);
```

---

## Implementation Plan

### Phase 1: HAL Multi-Port Support

**Files to Modify**:
- `src/hal/hal.c` - Add multi-port functions
- `src/hal/hal_linux.c` - Implement dual-interface support
- `src/hal/hal_stub.c` - Add stub multi-port functions

**Implementation Steps**:
1. Update `hal_context_t` to support multiple ports (DONE)
2. Implement `hal_init_multiport()` - Initialize two interfaces
3. Implement `hal_send_frame_port()` - Send on specific port
4. Implement `hal_receive_frame_port()` - Receive from specific port
5. Implement `hal_is_port_link_up()` - Check link status per port
6. Implement `hal_get_port_statistics()` - Get stats per port
7. Update platform implementations (Linux, stub)

**Estimated Effort**: 8-12 hours

### Phase 2: Process Data Redundancy Logic

**Files to Modify**:
- `src/master/process_data.c` - Add redundancy logic

**Implementation Steps**:
1. Implement `pd_exchange_port()` - Exchange on specific port
2. Implement `pd_switch_port()` - Switch active port
3. Implement `pd_check_port_health()` - Health monitoring
4. Implement `pd_get_port_status()` - Status retrieval
5. Add automatic failover logic
6. Add frame duplication for FRAME mode
7. Add frame filtering for duplicate detection
8. Update statistics tracking

**Estimated Effort**: 6-8 hours

### Phase 3: Cable Redundancy Mode

**Implementation Steps**:
1. Detect ring topology
2. Send on primary, receive on secondary
3. Monitor working counter on both ports
4. Implement automatic port switching
5. Handle cable break scenarios

**Estimated Effort**: 4-6 hours

### Phase 4: Frame Redundancy Mode

**Implementation Steps**:
1. Duplicate frames on both ports
2. Receive from both ports
3. Filter duplicate responses
4. Use first valid response
5. Track statistics per port

**Estimated Effort**: 4-6 hours

### Phase 5: Hot Connect Support

**Implementation Steps**:
1. Continuous link monitoring
2. Detect connect/disconnect events
3. Automatic port switching
4. Resume communication seamlessly

**Estimated Effort**: 3-4 hours

### Phase 6: Testing and Validation

**Testing Requirements**:
- Unit tests for all redundancy functions
- Integration tests with dual interfaces
- Cable break simulation
- Performance benchmarks
- Long-duration stability tests

**Estimated Effort**: 8-12 hours

**Total Estimated Effort**: 33-48 hours

---

## Testing Strategy

### Unit Tests

```c
// Test port initialization
void test_multiport_init(void);

// Test port switching
void test_port_switch(void);

// Test health monitoring
void test_port_health(void);

// Test frame duplication
void test_frame_redundancy(void);
```

### Integration Tests

1. **Dual Port Initialization**
   - Initialize with two interfaces
   - Verify both ports operational
   - Check statistics per port

2. **Cable Break Simulation**
   - Start cyclic operation
   - Disconnect primary cable
   - Verify automatic failover
   - Reconnect cable
   - Verify recovery

3. **Frame Redundancy**
   - Enable frame redundancy mode
   - Send frames on both ports
   - Verify duplicate filtering
   - Check performance overhead

4. **Long Duration**
   - Run for 24+ hours
   - Monitor port switches
   - Check for memory leaks
   - Verify statistics accuracy

### Hardware Requirements

- Two network interfaces on master PC
- EtherCAT slaves with dual ports
- Network cables for ring topology
- Cable disconnect mechanism for testing

---

## Performance Considerations

### Overhead Analysis

| Mode | CPU Overhead | Bandwidth | Latency Impact |
|------|--------------|-----------|----------------|
| Single Port | 0% (baseline) | 1x | 0us |
| Cable Redundancy | +2-5% | 1x | +5-10us |
| Frame Redundancy | +5-10% | 2x | +10-20us |
| Hot Connect | +1-2% | 1x | +2-5us |

### Optimization Strategies

1. **Zero-Copy**: Use packet mmap for Linux
2. **Batch Processing**: Process multiple frames together
3. **Lock-Free**: Use lock-free queues for port switching
4. **Inline Functions**: Inline hot path functions
5. **SIMD**: Use SIMD for frame comparison (duplicate detection)

### Memory Usage

- Additional per port: ~2MB (buffers)
- Total overhead: ~4MB for dual-port
- Acceptable for most systems

---

## Current Implementation Status

### Completed ✅

1. **API Design**: All functions defined in headers
2. **Data Structures**: Redundancy types defined
3. **Stub Functions**: Basic stubs in process_data.c
4. **Documentation**: This design document

### Stub Implementation ✅

The following functions are currently stubs (return success but don't implement full logic):

```c
// In process_data.c
pd_status_t pd_exchange_port(...) {
    // Currently calls pd_exchange()
    // TODO: Implement port-specific exchange
}

pd_status_t pd_switch_port(...) {
    // Currently just updates current_port
    // TODO: Implement actual port switching
}

pd_status_t pd_check_port_health(...) {
    // Currently always returns healthy
    // TODO: Implement health checking
}

pd_status_t pd_get_port_status(...) {
    // Currently returns stored status
    // TODO: Implement real-time status
}
```

### To Be Implemented ⏳

1. **HAL Multi-Port Functions**: Full implementation in hal.c
2. **Platform Support**: Linux and stub implementations
3. **Redundancy Logic**: Complete process_data.c implementation
4. **Automatic Failover**: Port switching logic
5. **Frame Duplication**: For frame redundancy mode
6. **Duplicate Filtering**: Frame deduplication
7. **Health Monitoring**: Continuous port health checks
8. **Statistics**: Per-port statistics tracking
9. **Example Application**: Redundancy demonstration
10. **Testing**: Comprehensive test suite

---

## Future Enhancements

### Advanced Features

1. **Adaptive Switching**: Machine learning for optimal port selection
2. **Load Balancing**: Distribute traffic across ports
3. **Quality of Service**: Priority-based port selection
4. **Predictive Maintenance**: Detect degrading cables before failure
5. **Multi-Master**: Support for redundant masters

### Protocol Extensions

1. **Redundancy Negotiation**: Auto-detect slave redundancy capabilities
2. **Synchronized Switching**: Coordinate port switches across network
3. **Redundancy Status Frames**: Dedicated frames for redundancy info
4. **Topology Discovery**: Automatic ring detection

---

## References

1. **ETG1000 Series**: EtherCAT specification
2. **IEC 61784-2**: Industrial communication networks - Profiles
3. **IEEE 802.3**: Ethernet standards
4. **Linux Packet MMAP**: Zero-copy packet processing

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-01-04 | Claude Code | Initial design document |

---

**Status**: Design Complete - Ready for Implementation

**Next Steps**:
1. Implement HAL multi-port functions
2. Update platform implementations
3. Implement redundancy logic in process_data.c
4. Create redundancy example application
5. Comprehensive testing with hardware

**Priority**: LOW - Optional feature for high-availability systems
