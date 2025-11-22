#include <SPI.h>
#include <SD.h>

#define SD_MOSI_PIN   PB15
#define SD_MISO_PIN   PB14
#define SD_SCLK_PIN   PB13
#define SD_CS_PIN     PB12

File sd_file;

bool init_sdcard(){

  SPI.setMISO(SD_MISO_PIN);
  SPI.setMOSI(SD_MOSI_PIN);
  SPI.setSCLK(SD_SCLK_PIN);
  
  return SD.begin(SD_CS_PIN);
}

void sd_create_file(){
    uint8_t id = util.last_file_id + 1;
    if(id > SD_MAX_FILES) id = 1;

    char filename[SD_MAX_FILENAME];
    sprintf(filename, "VOO_%02d.TXT", id);
    if(SD.exists(filename)){
        SD.remove(filename);
    }

    sd_file = SD.open(filename, FILE_WRITE);
    if(sd_file){
        sd_file.println("Tempo(S);Tensao(V);Corrente(A);Potencia(W);Acel_receptor(%);Acel_efetiva(%)");
    }

    util.last_file_id = id;
    save_fileid(id);
}

void sd_data_write(float time, float voltage, float current, float power, uint8_t rcv_throttle, uint8_t eft_throttle){
    if(sd_file){
        sd_file.print(time); sd_file.print(";");
        sd_file.print(voltage, 2); sd_file.print(";");
        sd_file.print(current, 2); sd_file.print(";");
        sd_file.print(power, 2); sd_file.print(";");
        sd_file.print(rcv_throttle); sd_file.print(";");
        sd_file.println(eft_throttle);
    }
}

bool sd_is_empty() {
    File root = SD.open("/");
    if (!root) return true;
    if (!root.isDirectory()) return true;

    File entry;
    while ((entry = root.openNextFile())) {
        if (!entry.isDirectory()) {
            entry.close();
            root.close();
            return false;
        }
        entry.close();
    }

    root.close();
    return true;
}

void sd_close_file() {
    if(sd_file) sd_file.close();
}