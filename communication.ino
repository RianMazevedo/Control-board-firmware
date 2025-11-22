#define STX 0x02
#define ETX 0x03
#define MAX_DATA 64

#define CMD_COM_TEST    0x04
#define CMD_GET_DATA    0x05
#define CMD_DWL_EEPROM  0x06
#define CMD_UPL_EEPROM  0x07

void app_communication() {
    static enum {WAIT_STX, WAIT_CMD, WAIT_LEN, READ_DATA, WAIT_CHECKSUM, WAIT_ETX} state = WAIT_STX;
    static byte cmd = 0;
    static byte len = 0;
    static byte index = 0;
    static byte checksum = 0;
    static byte received_checksum = 0;
    static byte data[MAX_DATA];
    
    while (Serial.available()) {

        byte b = Serial.read();

        switch (state) {
            case WAIT_STX:
                if (b == STX) state = WAIT_CMD;
                break;

            case WAIT_CMD:
                cmd = b;
                checksum = b;
                state = WAIT_LEN;
                break;

            case WAIT_LEN:
                len = b;
                checksum ^= b;
                if (len > MAX_DATA) state = WAIT_STX;
                else { index = 0; state = READ_DATA; }
                break;

            case READ_DATA:
                data[index++] = b;
                checksum ^= b;
                if (index >= len) state = WAIT_CHECKSUM;
                break;

            case WAIT_CHECKSUM:
                received_checksum = b;
                state = WAIT_ETX;
                break;

            case WAIT_ETX:
                if (b == ETX && received_checksum == checksum) {
                    handle_command(cmd, data, len);
                }
                state = WAIT_STX;
                break;
        }
        GPIOA->ODR |= (0b1 << 3);
        timers.timeout_previous = millis();
    }

    if(millis() - timers.timeout_previous >= TIMEOUT_COMMUNICATION){
        GPIOA->ODR &= ~(0b1 << 3);
    }
    
}

void handle_command(byte cmd, byte* data, byte len) {
    switch(cmd) {
        case CMD_COM_TEST:
            send_message("CONNECTED", cmd);
            break;

        case CMD_GET_DATA:
            send_message((uint8_t*)&telemetry, sizeof(telemetry), cmd);
            break;

        case CMD_DWL_EEPROM:
            send_message((uint8_t*)&config, sizeof(config), cmd);
            break;

        case CMD_UPL_EEPROM:
            handle_upload_eeprom(data, len);
            break;
    }
    
}


void handle_upload_eeprom(byte* data, byte len) {
    if(len != sizeof(CONFIG_DATA)) return;
    
    memcpy(&config, data, sizeof(CONFIG_DATA));
    save_eeprom();
    send_message("EEPROM SAVED", CMD_UPL_EEPROM);
}

void send_message(const uint8_t* buffer, byte len, byte cmd) {
    byte checksum = cmd;
    checksum ^= len;
    for (int i = 0; i < len; i++) checksum ^= buffer[i];

    Serial.write(STX);
    Serial.write(cmd);
    Serial.write(len);
    Serial.write(buffer, len);
    Serial.write(checksum);
    Serial.write(ETX);
}

void send_message(String str, byte cmd) {
    send_message((const uint8_t*)str.c_str(), str.length(), cmd);
}