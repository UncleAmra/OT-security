# OT Security Testbed: Virtual Machine Deployment Guide

This guide details the step-by-step instructions for deploying, configuring, and running the modular physical level simulator across an isolated Virtual Machine (VM) network based on the Purdue Model.

---

## 1. Network Architecture Setup

To simulate a realistic air-gapped industrial environment, you must configure a dedicated virtual network.

### Step 1: Create a Host-Only Virtual Network
1. Open your hypervisor manager (e.g., VirtualBox, VMware).
2. Go to **Network Settings / Virtual Network Editor**.
3. Create a new **Host-Only Network** (e.g., `vboxnet0` or `VMnet1`).
4. **Disable DHCP** on this network. Industrial control systems rely on static IP address assignments.
5. Set the subnet range (e.g., `192.168.56.0/24` with subnet mask `255.255.255.0`).

### Step 2: Configure Virtual Machine Adapters
For each virtual machine in your testbed, edit the settings:
1. Navigate to **Network Settings**.
2. Set **Adapter 1** to **Host-Only Adapter** (select the network created in Step 1).
3. Under *Advanced*, set **Promiscuous Mode** to **Allow All** (this allows VM 4 or security tools to sniff raw Modbus TCP packets using Wireshark or Scapy).

---

## 2. Static IP Address Configuration

Assign static IPs to each VM by modifying their local network configurations.

### For Ubuntu/Debian (using Netplan)
Edit the configuration file `/etc/netplan/01-netcfg.yaml` (or similar):

```yaml
network:
  version: 2
  renderer: networkd
  ethernets:
    enp0s3:
      dhcp4: no
      addresses:
        - 192.168.56.11/24 # Change this per VM
```

Apply the changes:
```bash
sudo netplan apply
```

### IP Allocations Map
| VM Name | Purdue Layer | IP Address | Run Command |
|---|---|---|---|
| **VM-Physics** | Layer 1 (Physical Process) | `192.168.56.11` | `./physics_engine 192.168.56.12 5020` |
| **VM-PLC** | Layer 2 (Industrial Controller) | `192.168.56.12` | *Starts the Modbus server* |
| **VM-HMI** | Layer 3 (Operator Stations) | `192.168.56.13` | *Starts SCADA/HMI dashboard* |
| **VM-AI-Security** | Security Node (Intrusion Detection) | `192.168.56.14` | *Sniffs interface or polls PLC* |

---

## 3. Layer 1 Deployment and Compilation

On the **Layer 1 VM (VM-Physics)**, perform the following steps:

1. Install compiler tools:
   ```bash
   sudo apt update
   sudo apt install -y build-essential
   ```
2. Copy the contents of the `/Layer_1` directory to this VM.
3. Open a terminal in the folder and compile the source:
   ```bash
   make
   ```
4. Verify the executable `physics_engine` is built.

---

## 4. Run Sequence

To prevent initialization errors, launch the components in the following sequence:

1. **Phase 1: PLC Server (VM-PLC)**
   Start the Modbus server listening on `192.168.56.12:5020`.
2. **Phase 2: Physics Twin (VM-Physics)**
   Start the simulation, pointing it to the PLC server IP:
   ```bash
   ./physics_engine 192.168.56.12 5020
   ```
   *Note: If the PLC is not yet ready, the physics engine will enter an automated connection retry loop.*
3. **Phase 3: Telemetry Polling (VM-HMI)**
   Launch the HMI client connecting to the PLC server.
4. **Phase 4: Passive Monitoring (VM-AI-Security)**
   Start security sniffing tools on the host-only interface.

---

## 5. Verification Checklist

To verify that the network loops are running correctly:
- Run a ping command from the Physics VM: `ping 192.168.56.12` to ensure connectivity.
- Verify that register outputs are updating on the PLC server console as the Physics Engine prints telemetry updates.
- Open Wireshark on the AI-Security VM and listen to the interface. You should see incoming **Modbus TCP packets** showing:
  - Function Code `03` (Read Holding Registers)
  - Function Code `16` (Write Multiple Registers)
