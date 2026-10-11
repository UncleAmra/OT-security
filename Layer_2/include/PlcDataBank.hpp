#ifndef PLC_DATA_BANK_HPP
#define PLC_DATA_BANK_HPP

#include <vector>
#include <mutex>
#include <cstdint>
#include "../../common/PlantRegisters.hpp"

/**
 * ==============================================================================
 *  PLC DATA BANK (ATOMIC THREAD-SAFE INDUSTRIAL REGISTER TABLE)
 * ==============================================================================
 * Holds holding registers with thread safety across multi-client Modbus TCP
 * network connections and real-time PLC logic scan cycles.
 * ==============================================================================
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

private:
    mutable std::mutex m_mutex;
    std::vector<uint16_t> m_registers;
};

#endif // PLC_DATA_BANK_HPP
