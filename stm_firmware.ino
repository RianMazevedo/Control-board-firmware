#include <Servo.h> 

#define SERIAL_BAUDRATE 115200
#define SERIAL_TIMEOUT 25
#define LED_RDY PA4
#define LED_COM PA3
#define LED_MFC PA5
#define ADC_VSENSOR PA1
#define ADC_ASENSOR PA2
#define PWM_IN PB3
#define PWM_OUT PB4

#define TIMEOUT_COMMUNICATION 1000
#define SENSOR_UPDATE 50
#define TIMEOUT_THROTTLE 100
#define MIN_PWM_ONTIME 1000
#define MAX_PWM_ONTIME 2000
#define AVARAGE_COUNT 5

#define SD_MAX_FILES 10
#define SD_MAX_FILENAME 11
#define SD_DATA_INTERVAL 500
#define SD_STOP_INTERVAL 5000
#define SD_START_INTERVAL 200
#define SD_MIN_POWER 3

struct __attribute__((packed)) CONFIG_DATA {
    float pid_p = 0.0;
    float pid_i = 0.0;
    float pid_d = 0.0;
    float vlt_offset = 0.0;
    float cur_offset = 0.0;
    uint16_t max_power = 0;
    uint16_t average = 0;
};

CONFIG_DATA config;

struct __attribute__((packed)) TELEMETRY_DATA {
    float voltage = 0.0;
    float current = 0.00;
    float power = 0.0;
    uint8_t receiver_throttle = 0;
    uint8_t effective_throttle = 0;
};

TELEMETRY_DATA telemetry;

struct __attribute__((packed)) TIMERS {
    unsigned long sensor_previous = 0;
    unsigned long timeout_previous = 0;
    unsigned long interrupt_previous = 0;
    unsigned long sdc_previous = 0;
    float sdc_time_counter = 0;
};

TIMERS timers;

struct __attribute__((packed)) UTIL {
    float voltage_history[AVARAGE_COUNT] = {0};
    float current_history[AVARAGE_COUNT] = {0};
    uint8_t v_index = 0;
    uint8_t c_index = 0;
    uint8_t last_file_id = 0;
    volatile unsigned long pwm_receiver = MIN_PWM_ONTIME;
    float pwm_output = MIN_PWM_ONTIME;
    float pid_setpoint = 0;
};

UTIL util;
Servo esc;

void setup(){
   Serial.begin(SERIAL_BAUDRATE);
   Serial.setTimeout(SERIAL_TIMEOUT);

   analogReadResolution(12);

   GPIOA->CRL &= ~((0b1111 << 12) | (0b1111 << 16) | (0b1111 << 20));
   GPIOA->CRL |= ((0b0011 << 12) | (0b0011 << 16) | (0b0011 << 20));
   GPIOB->CRL &= ~(0b1111 << 12);
   GPIOB->CRL |= (0b0100 << 12);

   esc.attach(PWM_OUT);
   attachInterrupt(digitalPinToInterrupt(PWM_IN), calculate_pulse_width, CHANGE);
  
   if(init_eeprom() != 0x00 && init_sdcard() != 0x00){
     GPIOA->ODR |= (0b1 << 4);
   }else while(true){
     GPIOA->ODR |= (0b1 << 5);
     delay(1000);
     GPIOA->ODR &= ~(0b1 << 5);
     delay(1000);
   }
    load_eeprom();
    init_pid();
}

void get_sensors(){
  if(millis() - timers.sensor_previous >= SENSOR_UPDATE){
    
    if(config.average){
      telemetry.voltage = calculate_average(analogRead(ADC_VSENSOR) * (8.0E-3 * config.vlt_offset), AVARAGE_COUNT, util.voltage_history, &util.v_index);
      telemetry.current = calculate_average(analogRead(ADC_ASENSOR) * (14.0E-3 * config.cur_offset), AVARAGE_COUNT, util.current_history, &util.c_index);
    }else{
      telemetry.voltage = analogRead(ADC_VSENSOR) * (8.0E-3 * config.vlt_offset);
      telemetry.current = analogRead(ADC_ASENSOR) * (14.0E-3 * config.cur_offset);
    }

      telemetry.power = telemetry.voltage * telemetry.current;

    timers.sensor_previous = millis();
  }
}

void esc_calib(){
    esc.writeMicroseconds(util.pwm_receiver);
}

float calculate_average(float newValue, uint8_t sample_count, float* history, uint8_t* index) {
    history[*index] = newValue;
    *index = (*index + 1) % sample_count;

    float sum = 0;
    for (int i = 0; i < sample_count; i++) {
        sum += history[i];
    }

    return sum / sample_count;
}

void calculate_pulse_width() {
    if (GPIOB->IDR & (0b1 << 3)) {
      timers.interrupt_previous = micros();
    }else{
      unsigned long temp = micros() - timers.interrupt_previous;
      
      if(temp <= MIN_PWM_ONTIME) temp = MIN_PWM_ONTIME;
      if(temp >= MAX_PWM_ONTIME) temp = MAX_PWM_ONTIME;
      
      util.pwm_receiver = temp;
      telemetry.receiver_throttle = map(temp, MIN_PWM_ONTIME, MAX_PWM_ONTIME, 0, 100);
      timers.interrupt_previous = micros();
    }
}

void generate_esc_pwm() {
   util.pid_setpoint = map(util.pwm_receiver, MIN_PWM_ONTIME, MAX_PWM_ONTIME, 0, config.max_power);
   update_pid();
   esc.writeMicroseconds(util.pwm_output);
   telemetry.effective_throttle = map(util.pwm_output, MIN_PWM_ONTIME, MAX_PWM_ONTIME, 0, 100);
}

void register_telemetry() {

    static bool recording = false;
    static bool is_on = false;
    static unsigned long below_power_time = 0;
    static unsigned long above_power_time = 0;
    static bool file_created = false;
    unsigned long now = millis();

    if (telemetry.power >= SD_MIN_POWER) {
        below_power_time = 0;
        if (above_power_time == 0) above_power_time = now;

        else if (!recording && (now - above_power_time >= SD_START_INTERVAL)) {
            recording = true;
            file_created = false;
            timers.sdc_time_counter = 0;
        }
    }

    else {
        above_power_time = 0;
        if (recording) {
            if (below_power_time == 0) below_power_time = now;
            else if (now - below_power_time >= SD_STOP_INTERVAL) {
                recording = false;
                sd_close_file();
            }
        }
    }

    if (recording && !file_created) {
        sd_create_file();
        file_created = true;
    }

    if (recording && (now - timers.sdc_previous >= SD_DATA_INTERVAL)) {
        timers.sdc_time_counter += (SD_DATA_INTERVAL / 1000.0);
        sd_data_write(timers.sdc_time_counter,
                      telemetry.voltage, telemetry.current,
                      telemetry.power, telemetry.receiver_throttle,
                      telemetry.effective_throttle);
        timers.sdc_previous = now;

        if (!is_on) {
            GPIOA->ODR |= (0b1 << 5);
            is_on = true;
        } else {
            is_on = false;
            GPIOA->ODR &= ~(0b1 << 5);
        }
    }
}

void loop(){
    app_communication();
    get_sensors();
    generate_esc_pwm();
    register_telemetry();

    //esc_calib();
}