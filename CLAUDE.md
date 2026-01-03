# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This repository contains EtherCAT (Ethernet for Control Automation Technology) protocol specification documents from the EtherCAT Technology Group (ETG). EtherCAT is an industrial Ethernet protocol designed for real-time control systems with high performance and deterministic communication.

## Specification Documents

The `specs/` directory contains the official ETG1000 specification series (Version 1.0.4):

1. **ETG1000_1** - Overview: Introduction to EtherCAT technology, basic concepts, and system architecture
2. **ETG1000_2** - Physical Layer: Hardware specifications, electrical characteristics, and physical topology
3. **ETG1000_3** - Data Link Layer Services: DLL service primitives and interfaces
4. **ETG1000_4** - Data Link Layer Protocols: Frame structure, addressing, and communication mechanisms
5. **ETG1000_5** - Application Layer Services: AL service primitives and state machine
6. **ETG1000_6** - Application Layer Protocols: CoE, FoE, SoE, VoE, and mailbox protocols

## Architecture

EtherCAT uses a master-slave architecture with the following key concepts:

- **Master**: Controls the network and initiates all communication
- **Slaves**: Field devices that process frames on-the-fly without buffering
- **Process Data**: Real-time cyclic data exchanged via logical addressing
- **Mailbox Communication**: Acyclic data exchange for configuration and diagnostics
- **Distributed Clocks**: Synchronization mechanism for precise timing across devices

The protocol stack consists of:
- Physical Layer (100BASE-TX Ethernet)
- Data Link Layer (EtherCAT frame processing)
- Application Layer (CoE/Canopen over EtherCAT, FoE/File access, SoE/Servo profiles, VoE/Vendor specific)

## Working with Specifications

When implementing EtherCAT functionality or answering questions about the protocol:

1. Reference the appropriate specification document by number (ETG1000_X)
2. The specifications build on each other - start with Overview (ETG1000_1) for context
3. Protocol implementation requires understanding both DLL (specs 3-4) and AL (specs 5-6) layers
4. Pay attention to state machines, timing requirements, and frame structures defined in the specs

## Development Project: Clean-Room EtherCAT Master Implementation

This repository aims to develop a clean-room implementation of an EtherCAT Master stack based on C11 for embedded systems.

### Development Workflow

The implementation follows a structured approach with two key documentation files:

1. **Spec.md** - Technical Specification Document
   - Defines all functions, structures, unions, enums, and state machines
   - Documents timing diagrams, sequence diagrams, and action flows
   - Specifies API interfaces and data structures for each protocol layer
   - Includes state machine definitions with transitions and conditions
   - Contains timing requirements and constraints from ETG specifications

2. **Plan.md** - Implementation Plan Document
   - Breaks down development into phases and milestones
   - Defines implementation order and dependencies between components
   - Specifies testing strategy for each module
   - Tracks progress and completion status

### Implementation Process

When working on the EtherCAT Master implementation:

1. **Analysis Phase**: Read and analyze relevant ETG1000 specifications
2. **Specification Phase**: Update Spec.md with detailed technical definitions
3. **Planning Phase**: Update Plan.md with implementation steps and dependencies
4. **Implementation Phase**: Write C11 code following the specifications
5. **Validation Phase**: Verify against timing diagrams and state machine definitions

### Technical Requirements

- **Language**: C11 standard
- **Target**: Embedded systems (resource-constrained environments)
- **Architecture**: Modular, layered design following EtherCAT protocol stack
- **Documentation**: All functions, structures, and state machines must be defined in Spec.md before implementation
- **Diagrams**: Timing diagrams, sequence diagrams, and state machine diagrams should be created in Mermaid format or ASCII art

### Key Components to Specify

- Frame processing and parsing (DLL)
- State machine implementation (Init, Pre-Op, Safe-Op, Op states)
- Mailbox protocols (CoE, FoE, SoE, VoE)
- Distributed Clock synchronization
- Process Data Objects (PDO) mapping
- Service Data Objects (SDO) access
- Network scanning and topology detection
- Error handling and diagnostics
