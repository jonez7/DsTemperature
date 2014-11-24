#include "DsTemperature.h"


#define DS18S20MODEL    0x10  // also DS1820
#define DS18B20MODEL    0x28
#define DS1822MODEL     0x22
#define DS1825MODEL     0x3B

#define STARTCONVO      0x44
#define COPYSCRATCH     0x48
#define READSCRATCH     0xBE
#define WRITESCRATCH    0x4E
#define RECALLSCRATCH   0xB8
#define READPOWERSUPPLY 0xB4
#define ALARMSEARCH     0xEC

#define TEMP_9_BIT  0x1F
#define TEMP_10_BIT 0x3F
#define TEMP_11_BIT 0x5F
#define TEMP_12_BIT 0x7F

// Error Codes
#define DEVICE_DISCONNECTED_C    -127
#define DEVICE_DISCONNECTED_RAW  -2032

// Scratchpad locations
typedef enum DallasScratchpads_e {
    DallasScratchpads_TEMP_LSB        = 0,
    DallasScratchpads_TEMP_MSB        = 1,
    DallasScratchpads_HIGH_ALARM_TEMP = 2,
    DallasScratchpads_LOW_ALARM_TEMP  = 3,
    DallasScratchpads_CONFIGURATION   = 4,
    DallasScratchpads_INTERNAL_BYTE   = 5,
    DallasScratchpads_COUNT_REMAIN    = 6,
    DallasScratchpads_COUNT_PER_C     = 7,
    DallasScratchpads_SCRATCHPAD_CRC  = 8,
    DallasScratchpads_MAX
} DallasScratchpads_e;

DsTemperature::DsTemperature(OneWire * const wireBus, 
                             uint8_t const * const addr) {
    m_precision = 9;
    m_wireBus   = wireBus;
    m_address   = addr;
    m_parasite  = HasParasitePowerSupply();
}


bool DsTemperature::HasParasitePowerSupply(void) {
    bool ret = false;
    m_wireBus->reset();
    m_wireBus->select(m_address);
    m_wireBus->write(READPOWERSUPPLY);
    ret = (m_wireBus->read_bit() == 0);
    m_wireBus->reset();
    m_parasite = ret;
    return ret;
}

bool DsTemperature::ChangeResolution(uint8_t const precision) {
    uint8_t data[12];

    memset(data, 0, sizeof(data));

    if (m_address[0] != DS18S20MODEL) {
        switch (precision) {
            case 12:
                data[DallasScratchpads_CONFIGURATION] = TEMP_12_BIT;
                break;
            case 11:
                data[DallasScratchpads_CONFIGURATION] = TEMP_11_BIT;
                break;
            case 10:
                data[DallasScratchpads_CONFIGURATION] = TEMP_10_BIT;
                break;
            case 9:
            default:
                data[DallasScratchpads_CONFIGURATION] = TEMP_9_BIT;
                break;
        }

        m_wireBus->reset();
        m_wireBus->select(m_address);
        m_wireBus->write(WRITESCRATCH);
        m_wireBus->write(data[DallasScratchpads_HIGH_ALARM_TEMP]);
        m_wireBus->write(data[DallasScratchpads_LOW_ALARM_TEMP]);
        
        if (m_address[0] != DS18S20MODEL) {
            m_wireBus->write(data[DallasScratchpads_CONFIGURATION]);
        }   
        m_wireBus->reset();
        m_wireBus->select(m_address); 
        // save the newly written values to eeprom
        m_wireBus->write(COPYSCRATCH, m_parasite);
        delay(20);
        if (m_parasite) {
            delay(10);
        }
        m_wireBus->reset();
    }
    m_precision = precision;
    return true;
}

void DsTemperature::StartTemperatureMeas() {
    m_wireBus->reset();
    m_wireBus->select(m_address);

    m_wireBus->write(STARTCONVO, m_parasite); /*Start temperature conversion*/
    m_convStarted = ((uint16_t)millis()) & 0x7FFF;
}

float DsTemperature::GetTemperature() {
    uint8_t data[12];

    uint16_t const now = ((uint16_t)millis()) & 0x7FFF;
    uint16_t const diff = (now - m_convStarted) & 0x7FFF;
    uint16_t const neededTime = 94 * (1 << (m_precision - 9));

    if ( diff < neededTime) {
        delay(neededTime - diff);
    }
    
    m_wireBus->reset();
    m_wireBus->select(m_address);
    m_wireBus->write(READSCRATCH); // Read Scratchpad

    for (uint8_t i = 0; i < DallasScratchpads_MAX; i++) {
        data[i] = m_wireBus->read();
    }
    m_wireBus->reset();

    int16_t rawTemp =
        (((int16_t)data[DallasScratchpads_TEMP_MSB]) << 11) |
        (((int16_t)data[DallasScratchpads_TEMP_LSB]) << 3);

    if (m_address[0] == DS18S20MODEL) {
        rawTemp =
            ((rawTemp & 0xfff0) << 3) - 16 +
            (
                ((data[DallasScratchpads_COUNT_PER_C] - data[DallasScratchpads_COUNT_REMAIN]) << 7) /
                data[DallasScratchpads_COUNT_PER_C]
             );
    }

    if (rawTemp <= DEVICE_DISCONNECTED_RAW) {
        return DEVICE_DISCONNECTED_C;
    } else {
        return ((float)rawTemp * 0.0078125);
    }
}
