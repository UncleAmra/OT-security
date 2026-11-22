#ifndef PLC_DATA_BANK_HPP
#define PLC_DATA_BANK_HPP

#include <vector>
#include <mutex>
#include <cstdint>
#include "../../common/PlantRegisters.hpp"

/*
 * The PLC's 64 holding registers, protected by a mutex so the scan loop
 * and the Modbus server threads can share them.
 */

class PlcDataBank {
public:
    PlcDataBank();

    void resetToDefaults();

    // Register access
    uint16_t getHoldingRegister(uint16_t address) const;
    void     setHoldingRegister(uint16_t address, uint16_t value);

    std::vector<uint16_t> getHoldingRegisters(uint16_t startAddr, uint16_t count) const;
    void                  setHoldingRegisters(uint16_t startAddr, const std::vector<uint16_t> &values);

    // Snapshot for logic engine
    void getFullSnapshot(std::vector<uint16_t> &outRegisters) const;
    void applySnapshot(const std::vector<uint16_t> &inRegisters);
    // Write only registers where `after` differs from `before`, so Modbus writes
    // that arrived while the scan was running are not overwritten.
    void applyChanges(const std::vector<uint16_t> &before, const std::vector<uint16_t> &after);

private:
    mutable std::mutex m_mutex;
    std::vector<uint16_t> m_registers;
};

#endif // PLC_DATA_BANK_HPP
