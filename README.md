# OT Power Plant Security Testbed with Agentic AI

An Operational Technology (OT) testbed simulating a critical infrastructure power plant using the Purdue 3-Layer Architecture. This project combines physical process simulation with industrial protocols (Modbus/TCP) and integrates an Agentic AI Security Node to evaluate autonomous threat detection, continuous testing, and automated incident response in an air-gapped Virtual Machine environment.

---

## System Architecture

Our environment follows the standard industrial Purdue Model, deployed across isolated Virtual Machines (VMs) communicating via unencrypted Modbus/TCP.

## 1. Power Plant Integration Process

The foundational objective is creating a stable Digital Twin of a power plant's physical environment and control logic across separate Linux Virtual Machines:

1. **Level 1 (Physics Engine):**
   A continuous process script written in Python/C++ running simulated thermodynamic equations (e.g., thermal accumulation, pressure buildup, emergency relief valve cooling dynamics).

2. **Level 2 (PLC Emulation):**
   A `pyModbusTCP` server acting as the industrial controller. It holds physical metrics in Modbus holding registers:
   * **Register 0000:** Boiler Temperature (C)
   * **Register 0001:** Internal System Pressure (PSI)
   * **Register 0002:** Emergency Relief Valve State (0 = Closed, 1 = Open)

3. **Level 3 (HMI Dashboard):**
   An operator station that polls Level 2 via Modbus/TCP, presenting real-time telemetry to human operators and issuing control flags upon threshold breaches.

4. **Network Synchronization:**
   Virtual Machines are connected via a dedicated host-only virtual network, isolating industrial traffic and allowing raw packet interception and protocol inspection (e.g., using Wireshark).

---

## 2. Agentic AI Implementation

Once system integration across Level 1-3 is verified, an Agentic AI Security Layer is attached to the OT environment. Unlike traditional static monitoring systems, the AI agent operates on an autonomous reasoning loop:

Observe -> Think -> Act -> Adapt

### Key Components:
* **Telemetry Monitoring (Observe):** The agent continuously reads Modbus registers from the PLC and parses system logs.
* **Autonomous Reasoning (Think):** Powered by an LLM engine (e.g., Gemini API or local model), the agent evaluates whether physical shifts stem from normal operations or an adversarial attack (e.g., Modbus register spoofing).
* **Tool Execution / Function Calling (Act):** The agent is granted native tool access (Function Calling). If boiler temperatures cross critical safety limits, the AI can autonomously write Modbus payloads to open emergency valves or micro-segment network traffic.
* **Human-in-the-Loop Guardrails:** High-risk actions require human operator validation, enforcing Least Privilege principles to prevent catastrophic AI hallucination risks or prompt injection threats.

---

## Quickstart & Setup
