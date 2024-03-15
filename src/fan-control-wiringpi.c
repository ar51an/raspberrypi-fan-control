/*
 * PWM Fan Controller:
 *   PWM fan controller is developed using WiringPi C library for RPi4. It is
 *   intended for "Noctua NF-A4x10 5V PWM" fan, it may work for any PWM fan
 *   with slight adjustment according to the fan specs.
 *   Copyright (c) 2021 - ar51an
 */

#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <wiringPi.h>
#include <systemd/sd-journal.h>

int PWM_PIN             = 18;   // HW PWM works at GPIO [12, 13, 18 & 19] on RPi4B
int TACHO_PIN           = 23;
int RPM_MAX             = 5000; // Noctua Specs: Max=5000
int RPM_MIN             = 1500; // Noctua Specs: Min=1000 [Kept 1500 as Min]
int RPM_OFF             = 0;
int TEMP_MAX            = 55;   // Above this temperature [FAN=ON At Max speed]
int TEMP_LOW            = 40;   // Below this temperature [FAN=OFF]
int WAIT                = 5000; // MilliSecs before adjusting RPM
int TACHO_ENABLED       = 0;    // TACHO Specific [Enable Tacho: 0=Disable 1=Enable]
const int PULSE         = 2;    // TACHO Specific [Noctua fan puts out 2 pulses per revolution]
volatile int intCount   = 0;    // TACHO Specific [Interrupt Counter]
int getRpmStartTime     = 0;    // TACHO Specific
int origPwmPinMode      = -1;
int origTachoPinMode    = -1;
float tempLimitDiffPct  = 0.0f;
char thermalFilename[]  = "/sys/class/thermal/thermal_zone0/temp";
static volatile sig_atomic_t keepRunning = 1;

void logConfParams () {
    char logFormat[] = "Config values loaded: PWM_PIN=%d | TACHO_PIN=%d | RPM_MAX=%d | RPM_MIN=%d | RPM_OFF=%d "
                       "| TEMP_MAX=%d | TEMP_LOW=%d | WAIT=%d | TACHO_ENABLED=%d | THERMAL_FILE=%s";
    sd_journal_print(LOG_INFO, logFormat, PWM_PIN, TACHO_PIN, RPM_MAX, RPM_MIN, RPM_OFF, \
                     TEMP_MAX, TEMP_LOW, WAIT, TACHO_ENABLED, thermalFilename);
}

void initFanControl () {
    /* Assign global vars with config file (if provided) values */
    FILE *confFile;
    char confFilename[]  = "/opt/gpio/fan/params.conf";
    confFile = fopen(confFilename, "r");
    if (confFile != NULL) {
        char confFormat[] = "PWM_PIN=%d TACHO_PIN=%d RPM_MAX=%d RPM_MIN=%d RPM_OFF=%d TEMP_MAX=%d "
                            "TEMP_LOW=%d WAIT=%d TACHO_ENABLED=%d THERMAL_FILE=%s";
        fscanf(confFile, confFormat, &PWM_PIN, &TACHO_PIN, &RPM_MAX, &RPM_MIN, &RPM_OFF, \
               &TEMP_MAX, &TEMP_LOW, &WAIT, &TACHO_ENABLED, thermalFilename);
        logConfParams();
        fclose(confFile);
    }
    else
        sd_journal_print(LOG_WARNING, "params.conf not found - Default values loaded");
    /* Calculate values of global vars */
    tempLimitDiffPct = (float) (TEMP_MAX-TEMP_LOW)/100;
    if (TACHO_ENABLED && TACHO_ENABLED != 1) TACHO_ENABLED = 0;
}

void initWiringPi () {
    /* Initialize wiringPi, calling 1 of 4 setup methods */
    wiringPiSetupGpio(); // Defaults to GPIO/BCM pin numbers
}

int getPinMode (int pin) {
    /* Mode Name Mapping: INPUT=0, OUTPUT=1, ALT0=4, ALT1=5, ALT2=6, ALT3=7, ALT4=3, ALT5=2 */
    return getAlt(pin);
}

void setFanSpeed (int pin, int speed) {
    pwmWrite(pin, speed);
}

int getCurrTemp () {
    int currTemp = 0;
    FILE *thermalFile;
    thermalFile = fopen(thermalFilename, "r");
    fscanf(thermalFile, "%d", &currTemp);
    fclose(thermalFile);
    currTemp = ((float) currTemp/1000)+0.5;
    return currTemp;
}

void setupPwm () {
    /* Set pwm fan freq=25kHz (Noctua whitepaper stated as Intel's recommendation for PWM FANs)
     * PWM crystal oscillator clock base frequency: RPI3=19.2MHz & RPI4=54MHz. pwmSetClock()
     * takes a divisor of base fequency: "19200000/768=25kHz". It adjusts the divisor for RPI4
     * 54MHz in the code itself as "divisor=540*divisor/192"
     */
    int pwmClock = 768;
    origPwmPinMode = getPinMode(PWM_PIN);
    pinMode(PWM_PIN, PWM_OUTPUT);
    // Using default balanced mode instead of mark:space mode
    //pwmSetMode(PWM_MODE_MS);
    pwmSetClock(pwmClock);
    pwmSetRange(RPM_MAX);          // Set PWM range to Max RPM
    setFanSpeed(PWM_PIN, RPM_OFF); // Set Fan speed to 0 initially
    //printf("[PWM] GPIO:Mode | %d:%d\n", PWM_PIN, origPwmPinMode);
    return;
}

void interruptHandler () {
    /* Number of interrupts generated between call to getFanRpm */
    intCount++;
    return;
}

void setupTacho () {
    origTachoPinMode = getPinMode(TACHO_PIN);
    pinMode(TACHO_PIN, INPUT);
    pullUpDnControl(TACHO_PIN, PUD_UP);
    getRpmStartTime = time(NULL);
    wiringPiISR(TACHO_PIN, INT_EDGE_FALLING, interruptHandler);
    return;
}

void getFanRpm () {
    int duration = 0;
    float frequency = 0.0f;
    int rpm = RPM_OFF;
    duration = (time(NULL)-getRpmStartTime);
    frequency = (intCount/duration);
    rpm = (int) (frequency*60)/PULSE;
    getRpmStartTime = time(NULL);
    intCount = 0;
    //if (rpm) printf("\e[30;38;5;67m[TACHO] Frequency: %.1fHz | RPM: %d\n\e[0m", frequency, rpm);
    if (rpm) sd_journal_print(LOG_DEBUG, "[TACHO] Frequency: %.1fHz | RPM: %d", frequency, rpm);
    return;
}

void setFanRpm () {
    int rpm = RPM_OFF;
    static int lastRpm = 0;
    float currTempDiffPct = 0.0f;
    int currTemp = getCurrTemp();
    int tempDiff = (currTemp-TEMP_LOW);
    if (tempDiff > 0) {
        currTempDiffPct = (tempDiff/tempLimitDiffPct);
        rpm = (int) (currTempDiffPct*RPM_MAX)/100;
        rpm = rpm < RPM_MIN ? RPM_MIN : rpm > RPM_MAX ? RPM_MAX : rpm;
        //printf("\e[30;38;5;139m[PWM] Temp: %d | TempDiff: %.1f%% | RPM: %d\n\e[0m", currTemp, currTempDiffPct, rpm);
        sd_journal_print(LOG_DEBUG, "[PWM] Temp: %d | TempDiff: %.1f%% | RPM: %d", currTemp, currTempDiffPct, rpm);
    }
    if (lastRpm != rpm) setFanSpeed(PWM_PIN, rpm);
    lastRpm = rpm;
    return;
}

void start () {
    if (TACHO_ENABLED) {
        setupTacho();
        while (keepRunning) { setFanRpm(); getFanRpm(); delay(WAIT); }
    }
    else {
        while (keepRunning) { setFanRpm(); delay(WAIT); }
    }
    return;
}

static void signalHandler (int _) {
    // Exit controller on Ctrl+C
    (void)_;
    keepRunning = 0;
    //printf("\r");
}

void cleanup () {
    // PWM pin cleanup
    setFanSpeed(PWM_PIN, RPM_OFF);
    pinMode(PWM_PIN, origPwmPinMode);
    //pullUpDnControl(PWM_PIN, PUD_DOWN);
    // TACHO pin cleanup
    if (TACHO_ENABLED) pinMode(TACHO_PIN, origTachoPinMode);
    //pullUpDnControl(TACHO_PIN, PUD_DOWN);
    sd_journal_print(LOG_INFO, "Cleaned up - Exiting ...");
    return;
}

int main (void)
{
    struct sigaction act = {0};
    act.sa_handler = signalHandler;
    sigaction(SIGINT, &act, NULL);
    initFanControl();
    initWiringPi();
    setupPwm();
    sd_journal_print(LOG_INFO, "Initialized and running ...");
    start();
    cleanup();
    return 0;
}
