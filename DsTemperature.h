#ifndef DsTemperature_h
#define DsTemperature_h

#include <stdint.h>
#include <inttypes.h>
#include <OneWire.h>

#ifndef REQUIRESNEW
#define REQUIRESNEW false
#endif

class DsTemperature
{
public:

    DsTemperature(OneWire * const wireBus, 
                  uint8_t const * const addr);

    bool ChangeResolution(byte const resolution);

    bool ReadPowerSupply(void);

    // return temperature in degree Celcius
    float GetTemperature(void);

private:
    OneWire       * m_wireBus;
    uint8_t const * m_address;
    uint8_t         m_precision;
    bool            m_parasite;
};

#endif
