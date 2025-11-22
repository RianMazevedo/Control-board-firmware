#include "extEEPROM.h"

extEEPROM eeprom(kbits_8, 1, 16, 0x50);

bool init_eeprom(){
  return ~eeprom.begin(eeprom.twiClock100kHz);
}

void save_eeprom() {
  eeprom.write(0, (byte*)&config, sizeof(config));
}

void load_eeprom() {
  eeprom.read(0, (byte*)&config, sizeof(config));
  util.last_file_id = load_fileid();
}

void save_fileid(uint8_t id){
  util.last_file_id = id;
  eeprom.write(sizeof(config), (byte*)&util.last_file_id, sizeof(util.last_file_id));
}

uint8_t load_fileid(){
    uint8_t id = 0;
    eeprom.read(sizeof(config), (byte*)&id, sizeof(id));
    if (sd_is_empty()){
      return 0;
    }
    return id;
}