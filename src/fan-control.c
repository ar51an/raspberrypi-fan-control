/*
 * PWM Fan Controller:
 *   PWM fan controller is developed using WiringPi C library for RPi4. It is
 *   intended for "Noctua NF-A4x10 5V PWM" fan, but it may work for any PWM
 *   fan with slight adjustment according to the fan specs.
 *   Copyright (c) 2021 - ar51an
 */

#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <wiringPi.h>
#include <systemd/sd-journal.h>

#define	LOG_ERR		3
#define	LOG_WARN	4
#define	LOG_INFO	6
const int PWM_PIN     = 18;    // HW PWM works at GPIO 12, 13, 18 & 19 on RPi4B
const int TACHO_PIN   = 23;
const int RPM_MAX     = 5000;  // Noctua Specs: Max=5000
const int RPM_MIN     = 1500;  // Noctua Specs: Min=1000. I used 1500 as Min
const int RPM_OFF     = 0;
const int TEMP_MAX    = 58;    // Above this temperature, FAN=ON » Max speed
const int TEMP_LOW    = 48;    // Below this temperature, FAN=OFF
const int WAIT        = 5000;  // MilliSecs before adjusting RPM
const int PULSE       = 2;     // TACHO_PIN - Noctua fan puts out 2 pluses per revolution
volatile int intCount = 0;     // TACHO_PIN - Interrupt Counter
int getRpmStartTime   = 0;     // TACHO_PIN
int origPwmPinMode    = -1;
int origTachoPinMode  = -1;
static volatile sig_atomic_t keepRunning = 1;
const float tempLimitDiffPct = (float) (TEMP_MAX-TEMP_LOW)/100;

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
    float currTempF = 0;
    FILE *thermalFile;
    thermalFile = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    fscanf (thermalFile, "%f", &currTempF);
    fclose (thermalFile);
    currTempF = currTempF/1000;
    currTemp = (int) (currTempF + 0.5);
    //printf("CurrTempF: %f, CurrTempI Rounded: %d\n", currTempF, currTemp);
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
    //printf("PWM_PIN_ORIG » Num:Mode » %d:%d\n", PWM_PIN, origPwmPinMode);
    return;
}

void interruptHandler () {
    /* Number of interrupts generated between call to getFanRpm */
    intCount++;
    return;
}

void setupTacho () {
    origTachoPinMode = getPinMode(TACHO_PIN);
    pinMode (TACHO_PIN, INPUT);
    pullUpDnControl (TACHO_PIN, PUD_UP);
    getRpmStartTime = time(NULL);
    wiringPiISR(TACHO_PIN, INT_EDGE_FALLING, interruptHandler);
    return;
}

int getFanRpm () {
    int duration = 0;
    float frequency = 0;
    int rpm = RPM_OFF;
    duration = (time(NULL)-getRpmStartTime);
    frequency = (intCount/duration);
    rpm = (int) (frequency*60)/PULSE;
    getRpmStartTime =  time(NULL);
    intCount = 0;
    //printf("\e[30;38;5;67m » TACHO-PIN › Frequency: %.1fHz | RPM: %d\n\e[0m", frequency, rpm);
    return rpm;
}

void setFanRpm () {
    int rpm = RPM_OFF;
    float currTempDiffPct = 0;
    int currTemp = getCurrTemp();
    int tempDiff = (currTemp-TEMP_LOW);
    if (tempDiff > 0) {
        currTempDiffPct = (tempDiff/tempLimitDiffPct);
        rpm = (int) (currTempDiffPct*RPM_MAX)/100;
        rpm = rpm < RPM_MIN ? RPM_MIN : rpm > RPM_MAX ? RPM_MAX : rpm;
        //printf("\e[30;38;5;139m » PWM-PIN › Temp: %d | TempDiff: %.1f% | RPM: %d\n\e[0m", currTemp, currTempDiffPct, rpm);
        sd_journal_print(LOG_INFO, "PWM-PIN › Temp: %d | TempDiff: %.1f% | RPM: %d", currTemp, currTempDiffPct, rpm);
    }
    setFanSpeed(PWM_PIN, rpm);
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
    //pullUpDnControl(TACHO_PIN, PUD_DOWN);
    //pinMode(TACHO_PIN, origTachoPinMode);
    //printf("\e[30;38;5;74m » Cleaned up, exiting ...\n\e[0m");
    sd_journal_print(LOG_INFO, "» Cleaned up, exiting ...");
    return;
}

int main (void)
{
    //printf("\e[30;38;5;74m » PWM fan controller started ...\n\e[0m");
    sd_journal_print(LOG_INFO, "» PWM fan controller started ...");
    signal(SIGINT, signalHandler);
    initWiringPi();
    setupPwm();
    //setupTacho();
    while (keepRunning)	{
        setFanRpm();
        //getFanRpm();
        delay(WAIT);
    }
    cleanup();
    return 0;
}
