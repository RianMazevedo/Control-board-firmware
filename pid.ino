#include <QuickPID.h>

QuickPID pwm_pid(&telemetry.power, &util.pwm_output, &util.pid_setpoint);

void init_pid() {
    pwm_pid.SetTunings(config.pid_p, config.pid_i, config.pid_d);
    pwm_pid.SetOutputLimits(MIN_PWM_ONTIME, MAX_PWM_ONTIME);
    pwm_pid.SetSampleTimeUs(SENSOR_UPDATE * 1000);
    pwm_pid.SetMode(pwm_pid.Control::automatic);
}

void update_pid() {
    pwm_pid.SetTunings(config.pid_p, config.pid_i, config.pid_d);
    pwm_pid.Compute();
}

void set_pid_off() {
    pwm_pid.SetMode(pwm_pid.Control::manual);
}

void set_pid_on() {
    pwm_pid.SetMode(pwm_pid.Control::automatic);
}

void reset_pid() {
    pwm_pid.SetMode(pwm_pid.Control::manual);
    pwm_pid.SetMode(pwm_pid.Control::automatic);
}