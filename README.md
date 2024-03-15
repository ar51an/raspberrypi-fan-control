## Raspberry Pi Fan Control
<div align="center">

![fancontrol](https://img.shields.io/badge/-fan‑control-D8BFD8?logo=katana&logoColor=3a3a3d)
&nbsp;&nbsp;[![release](https://img.shields.io/github/v/release/ar51an/raspberrypi-fan-control?display_name=release&logo=rstudio&color=90EE90&logoColor=8FBC8F)](https://github.com/ar51an/raspberrypi-fan-control/releases/latest/)
&nbsp;&nbsp;![downloads](https://img.shields.io/github/downloads/ar51an/raspberrypi-fan-control/total?color=orange&label=downloads&logo=github)
&nbsp;&nbsp;![visitors](https://img.shields.io/endpoint?color=4883c2&label=visitors&logo=github&url=https%3A%2F%2Fhits.dwyl.com%2Far51an%2Fraspberrypi-fan-control.json)
&nbsp;&nbsp;![lang](https://img.shields.io/badge/lang-C-5F9EA0?logo=conan&logoColor=8FBC8F)
&nbsp;&nbsp;![license](https://img.shields.io/badge/license-MIT-CED8E1)
</div>

### Preview  
<div align="center">
  <img src="https://user-images.githubusercontent.com/11185794/202368819-3a5722cf-78c7-4150-9398-c00ade0ece88.png?raw=true" alt="RP4-github"/>
</div>

---
<div align="justify">
  
### Intro
Raspberry Pi fan-control service to adjust PWM fan speed automatically based on CPU temperature. It will help in reducing fan noise and power consumption. It is written in C. **Main objective is to keep it fast and use minimum CPU and memory resources**.
<br/>

<div align="center">
  <img src="https://github.com/ar51an/temp/assets/11185794/964e5520-89ff-44f1-ab09-0eddf0d11706?raw=true" alt="resource-usage"/>
</div>
<br/>

If you just want PWM fan On/Off based on CPU temperature. Connect fan's `PWM, ground and +5V wires` directly to the GPIO pins. Enable fan **either** from raspi-config **or** UI. Set the PWM pin and CPU temperature in the setup. Lowest temperature limit of 60°C can be bypassed by editing `/boot/firmware/config.txt` manually. Find `dtoverlay=gpio-fan` entry and change the `temp=60000` to the desired temperature. Fan will start at the specified CPU temperature and it will stop 10°C below that. The downside is fan will run at full speed and bit noisy, specially if you are using an open RP4 case.
<br/>

This service is specifically written for `Noctua 5V PWM` fan and `Raspberry Pi 4`. It may work for other PWM fans and RP models. You should know the intended fan's specifications, like max / min `RPM` and `target frequency`. Adjust these values in code/config and rebuild the binary (if needed).
<br/>

I connected Noctua fan wires directly to the RP4 GPIO pins. It's been almost 3 years without any issue, your mileage may vary. If your fan does not support PWM or you want to safeguard hardware **either** build your own circuit **or** buy a pre-built PCB with transistor and diode like [EZ RP Fan Controller](https://www.tindie.com/products/jeremycook/ez-fan2-tiny-raspberry-pi-fan-controller/).  
> `⚠️` **WARNING:** ***I accept no responsibility if you damage your Raspberry Pi or fan.***

#### Specs:
> |Noctua Fan           |HW                      |OS                           |WiringPi|pigpio|
> |:--------------------|:-----------------------|:----------------------------|:-------|:-----|
> |`NF-A4x10 5V PWM Fan`|`Raspberry Pi 4 Model B`|`raspios-bookworm-arm64-lite`|`3.1`   |`79`  |
#

### Hardware Prep
* The default noctua fan connector will not connect directly to GPIO header. You need to do some modifications. There are multiple options:  
  > **1 - Dupont Jumper Wires Male to Female:**  
  > Noctua's existing wire is pretty long. Get short jumper wires somewhere between 2-4" long. Male part of jumper wire will connect to Noctua connector and Female part will connect to GPIO. I used it for few months.

  > **2 - Dupont Female Pin Connectors 2.54mm Pitch:**  
  > It is the cleanest option, as shown in the preview. You need dupont Female pin connectors, crimping tool, wire cutter & stripper, heat-shrink-tube & heat-gun **or** dupont connector housing.  
  > Better options for dupont connectors are `Molex Crimp Terminals Series: 70058 Part No: 16020098` or `Harwin Series: M20 Part No: 1180042`. I used Molex connectors and IWISS `IWS-2820` crimping tool. Cover these connectors with **either** heat shrink tube (3.00mm diameter tube) **or** dupont connector housing.  
  
  > **3 - Use Wires from Old Fan:**  
  > If you have some old unused fan laying around that has dupont connector wires. Cut the wires from that fan, cut the Noctua connector and do some wire joining. Noctua fan's OmniJoin adaptors can be used for joining wires.

* Complete specification of Noctua fan is available at [Noctua Whitepaper](https://noctua.at/pub/media/wysiwyg/Noctua_PWM_specifications_white_paper.pdf). Details of Raspberry Pi GPIO pin layout is available at [GPIO Pinout](https://pinout.xyz/). Screenshots attached for quick reference.

  |Fan Wires  |GPIO Layout|
  |:----------|:----------|
  |![noctua_pin_config](https://user-images.githubusercontent.com/11185794/134500714-1cd90f97-a63e-43d6-9f87-c1f6856ca83e.png)|![gpio_layout](https://user-images.githubusercontent.com/11185794/134590691-e4587fce-e01f-401a-a668-3a992159aa1f.png)|

* Fan wires connection to RP4 pins:  
  
  |Fan Wires       |RP4 Pins       |
  |:---------------|:--------------|
  |Yellow +5V      |Physical Pin 4 |
  |Black Ground    |Physical Pin 6 |
  |Blue PWM Signal |Physical Pin 12|
  |Green RPM Signal|Physical Pin 16|

  Fan's PWM signal wire is connected to the RP4 `Physical pin 12 - GPIO pin 18`. This fan-control code uses GPIO 18 as default. There are 4 pins on RP4 that support hardware PWM `GPIO 12/13/18/19`. If you are going to use a different GPIO pin make sure you change the `PWM_PIN` in `params.conf` with the one you use.  
The green tachometer wire on Noctua fan is used to calculate RPM. Connect the fan's RPM signal wire to the RP4 `Physical pin 16 - GPIO pin 23`. By default, tacho output is disabled in `params.conf`. (refer to `Points to Note`)

#
### Steps
#### ❯ Install C Library
* Install **either** `WiringPi` **or** `pigpio` C library.

  * **WiringPi**  
    [Download](https://github.com/WiringPi/WiringPi/releases/download/3.1/wiringpi_3.1_arm64.deb) and install `WiringPi` C library.
    > Install:  
    > `sudo dpkg -i wiringpi_3.1_arm64.deb`  

    > `ℹ️` **Note:**  
      > WiringPi has been revived. Latest release supports RasberryPi OS Bookworm. Check the github [repo](https://github.com/WiringPi/WiringPi) for updates.

  * **Pigpio**  
    [Download](https://codeload.github.com/joan2937/pigpio/zip/refs/heads/master) and install `pigpio` C library.
    > Install:  
    > `unzip -o pigpio-master.zip`  
    > `make`  
    > `sudo make install`

    > `ℹ️` **Note:**  
      > Uninstall Pigpio:  
      > If pigpio was installed using the previous step, manually remove all files under `/usr/local/*` mentioned [here](https://abyz.me.uk/rpi/pigpio/faq.html#Library_update_fails). To remove the distro provided pigpio package run cmd `sudo apt --purge autoremove pigpio`  

#### ❯ Install FanControl
* Download the latest fan-control [release](https://github.com/ar51an/raspberrypi-fan-control/releases) for the library that was installed in the previous step. Create folder `/opt/gpio/fan`. Copy `fan-control` and `params.conf` from the latest release under `build` folder to this newly created folder `/opt/gpio/fan`. Make sure both files are under the ownership of root and `fan-control` is executable. **Fan-control will work with default values without `params.conf`.**

  > **Create folder:**  
  > `sudo mkdir -p /opt/gpio/fan/`  

  > **Make binary executable [If needed]:**  
  > `sudo chmod +x /opt/gpio/fan/fan-control`  
  
  > **Change ownership [If needed]:**  
  > `sudo chown root:root /opt/gpio/fan/fan-control`  
  > `sudo chown root:root /opt/gpio/fan/params.conf`  

* Create service to automatically run the fan-control at startup. Copy `fan-control.service` from the latest release under `service` folder to `/etc/systemd/system/`. Make sure file is under the ownership of root. Enable the service.

  > **Change ownership [If needed]:**  
  > `sudo chown root:root /etc/systemd/system/fan-control.service`  

  > **Enable Service:**  
  > `sudo systemctl enable fan-control`  

  > **Start Service [If needed]:**  
  > System reboot will automatically start the service. To start without reboot.  
  > `sudo systemctl start fan-control`  

  > **Check Service Status:**  
  > `sudo systemctl status fan-control`  

  > **Check Journal Logs:**  
  > `sudo journalctl -u fan-control`  

#
### Points to Note
* Noctua's green RPM signal wire (aka tacho) connectivity is optional. It generates tacho output signal. It does not add any value in controlling fan speed. That is why it is disabled by default in `params.conf`. To retrieve RPM periodically through tacho output signal in the logs, connect the wire as mentioned under `Hardware Prep` and enable it in `params.conf`. Change `TACHO_ENABLED` to `1`.

* Fan runs at full speed when RP4 is booted. When fan-control service starts during boot process fan will **either** switch off **or** adjust its speed, depending on CPU temperature. Fan-control service runs the fan within the temperature range from `40–55°C` and above. Temperature range is configurable through `params.conf`. Noctua fan's recommeded minimum RPM is 1000. I kept the minimum RPM at 1500 in this service. Table below explains the fan's operation:  
  |`Temp`   |`RPM`  |
  |:--------|:------|
  |`<= 40°C`|`0`    |
  |`> 40°C` |`1500` |
  |`Temp++` |`RPM++`|
  |`>= 55°C`|`5000` |

* `params.conf` can be used to configure values of adjustable parameters. Fan-control service will work with default values without `params.conf`. Restart fan-control service after any change in `params.conf`. Table below gives an overview of all adjustable parameters.  
  <sub>**_* Do not change values if you do not know what you are doing_**</sub>
  |`Parameters`   |`Default`                              |`Info`                                       |
  |:--------------|:--------------------------------------|:--------------------------------------------|
  |`PWM_PIN`      |`18`                                   |HW PWM GPIO pins on RPi4B: 12, 13, 18, 19    |
  |`TACHO_PIN`    |`23`                                   |Tacho pin, enable through TACHO_ENABLED      |
  |`RPM_MAX`      |`5000`                                 |Fan's max speed. Noctua Specs: Max=5000      |
  |`RPM_MIN`      |`1500`                                 |Fan's min speed. Noctua Specs: Min=1000      |
  |`RPM_OFF`      |`0`                                    |Fan off                                      |
  |`TEMP_MAX`     |`55`                                   |Max temperature in °C to run fan at max RPM  |
  |`TEMP_LOW`     |`40`                                   |Min temperature in °C to start fan at min RPM|
  |`WAIT`         |`5000`                                 |Wait interval between adjusting RPM          |
  |`TACHO_ENABLED`|`0`                                    |Enable tacho, 0=Disable 1=Enable             |
  |`THERMAL_FILE` |`/sys/class/thermal/thermal_zone0/temp`|Path to RP4 thermal file                     |

* ***The 5V pins on RP4 i.e., physical pin 2 and 4 are not GPIO. They are connected to the 5V power supply and are always on.*** Those cannot be turned off without some form of circuit using mosfet or transistor. The point is if you run `shutdown command` from a shell or UI the fan will keep on running at full speed unless you unplug the RP4.

* Fan-control logs to the journal at startup and exit. It also logs periodically when fan is on. You can change `log level` to `MaxLevelStore=info` in `journald.conf` to reduce logging.

#
### Build
* Install `libsystemd-dev`. It is required for compiling fan-control source code.  
  > `sudo apt install libsystemd-dev`  

* Binary is available in the release. If for any reason you want to recompile.  
  > WiringPi:  
  > `sudo gcc -Wall -O2 fan-control-wiringpi.c -o fan-control -lwiringPi -lsystemd`  
  > Pigpio:  
  > `sudo gcc -Wall -O2 fan-control-pigpio.c -o fan-control -lpigpio -lsystemd`  
</div>
