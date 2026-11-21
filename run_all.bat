@echo off
echo ==============================================================================
echo  STARTING OT POWER PLANT TESTBED (LAYERS 1, 2, AND 3)
echo ==============================================================================
echo [1/3] Starting Layer 2: Industrial PLC Controller (Modbus Port 5020)...
start "Layer 2: Industrial PLC" /D "%~dp0Layer_2" plc_controller.exe
timeout /t 2 /nobreak >nul

echo [2/3] Starting Layer 1: Physics Digital Twin...
start "Layer 1: Physics Engine" /D "%~dp0Layer_1" physics_engine.exe
timeout /t 1 /nobreak >nul

echo [3/3] Starting Layer 3: SCADA / HMI Operator Workstation (HTTP Port 8085)...
start "Layer 3: SCADA Workstation" /D "%~dp0Layer_3" scada_hmi.exe
timeout /t 1 /nobreak >nul

echo.
echo All layers started!
echo Open your web browser at: http://localhost:8085
echo ==============================================================================
