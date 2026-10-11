#!/bin/bash
set -e

echo "=============================================================================="
echo " BUILDING OT POWER PLANT TESTBED (LAYERS 1, 2, AND 3 - C++)"
echo "=============================================================================="

echo "[1/3] Building Layer 1: Physics Digital Twin..."
cd Layer_1
g++ -std=c++17 -Wall -Wextra -O2 -Iinclude -I../common -o physics_engine \
    src/main.cpp src/BoilerSystem.cpp src/CombustionSystem.cpp \
    src/TurbineSystem.cpp src/GeneratorSystem.cpp src/CondenserFeedwaterSystem.cpp \
    src/DigitalTwinPlant.cpp -lpthread
cd ..

echo "[2/3] Building Layer 2: Industrial PLC Controller..."
cd Layer_2
g++ -std=c++17 -Wall -Wextra -O2 -Iinclude -I../common -o plc_controller \
    src/main.cpp src/PlcDataBank.cpp src/PlcSafetyInterlocks.cpp \
    src/PlcScanEngine.cpp src/ModbusTcpServer.cpp -lpthread
cd ..

echo "[3/3] Building Layer 3: SCADA / HMI Operator Workstation..."
cd Layer_3
g++ -std=c++17 -Wall -Wextra -O2 -Iinclude -I../common -o scada_hmi \
    src/main.cpp src/ScadaPoller.cpp src/HttpScadaServer.cpp -lpthread
cd ..

echo ""
echo "=============================================================================="
echo " BUILD COMPLETE! All binaries compiled successfully:"
echo "  - Layer_1/physics_engine"
echo "  - Layer_2/plc_controller"
echo "  - Layer_3/scada_hmi"
echo "=============================================================================="
