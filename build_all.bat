@echo off
echo ==============================================================================
echo  BUILDING OT POWER PLANT TESTBED (LAYERS 1, 2, AND 3 - C++)
echo ==============================================================================

echo [1/3] Building Layer 1: Physics Digital Twin...
cd Layer_1
g++ -std=c++17 -Wall -Wextra -O2 -Iinclude -I../common -o physics_engine.exe src/main.cpp src/BoilerSystem.cpp src/CombustionSystem.cpp src/TurbineSystem.cpp src/GeneratorSystem.cpp src/CondenserFeedwaterSystem.cpp src/DigitalTwinPlant.cpp -lws2_32
if %errorlevel% neq 0 (
    echo [ERROR] Layer 1 compilation failed!
    exit /b %errorlevel%
)
cd ..

echo [2/3] Building Layer 2: Industrial PLC Controller...
cd Layer_2
g++ -std=c++17 -Wall -Wextra -O2 -Iinclude -I../common -o plc_controller.exe src/main.cpp src/PlcDataBank.cpp src/PlcSafetyInterlocks.cpp src/PlcScanEngine.cpp src/ModbusTcpServer.cpp -lws2_32
if %errorlevel% neq 0 (
    echo [ERROR] Layer 2 compilation failed!
    exit /b %errorlevel%
)
cd ..

echo [3/3] Building Layer 3: SCADA / HMI Operator Workstation...
cd Layer_3
g++ -std=c++17 -Wall -Wextra -O2 -Iinclude -I../common -o scada_hmi.exe src/main.cpp src/ScadaPoller.cpp src/HttpScadaServer.cpp -lws2_32
if %errorlevel% neq 0 (
    echo [ERROR] Layer 3 compilation failed!
    exit /b %errorlevel%
)
cd ..

echo.
echo ==============================================================================
echo  BUILD COMPLETE! All binaries compiled successfully:
echo   - Layer_1/physics_engine.exe
echo   - Layer_2/plc_controller.exe
echo   - Layer_3/scada_hmi.exe
echo ==============================================================================
