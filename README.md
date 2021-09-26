## RP4 PWM Fan Controller

### Preview  
<p align="center">
  <img src="https://user-images.githubusercontent.com/11185794/134425460-cef34e39-98cc-4489-ba0a-975c09509652.png?raw=true" alt="RP4_github"/>
</p>

#

### Intro
Service to adjust RP4 PWM (Pulse Width Modulation) fan speed automatically based on CPU temperature. It will help in reducing fan noise and power consumption. It is written in C. **Main objective was to keep it fast and use minimum CPU and Memory resources**.
<br/>

If you just want PWM fan On/Off based on CPU temperature. Connect fan's `PWM, ground and +5V wires` directly to the GPIO pins. Enable fan **either** from raspi-config **or** UI. Set the PWM pin and CPU temperature in the setup. Lowest temperature you can specify from setup is 60°C. This limit can be bypassed by editing `/boot/config.txt` manually. Search for `dtoverlay=gpio-fan` entry and change the `temp=60000` value to your desired temperature. Fan will start at the specified CPU temperature and it will stop 10°C below that. The downside is fan will run at full speed and bit noisy, specially if you are using an open RP4 case.
<br/>

This service is specifically written for `Noctua 5V PWM` fan and `Raspberry Pi 4`. It may work for other PWM fans and RP models. You should know the intended fan's specifications, like max / min `RPM` and `target frequency`. Adjust these values in code and rebuild the binary. Raspberry Pi crystal oscillator clock frequency on RPi4B is 54MHz and on all earlier models it is 19.2MHz. `WiringPi` function `pwmSetClock` requires a divisor of clock frequency to set the target frequency of fan. The process is well documented in the fan-control service code.
<br/>

I connected Noctua fan's 3 wires directly to the RP4 GPIO pins. It is been over 6 months without any issue, your mileage may vary. If your fan does not support PWM or you want to safeguard hardware **either** build your own circuit **or** buy a pre-built PCB with transistor and diode like [EZ RP Fan Controller](https://www.tindie.com/products/jeremycook/ez-fan2-tiny-raspberry-pi-fan-controller/).  
***WARNING: I accept no responsibility if you damage your Raspberry Pi or fan.***
<br/>

#### Specs:
> • Noctua NF-A4x10 5V PWM Fan  
> • Raspberry Pi 4 Model B  
> • raspios-buster-arm64-lite  
> • WiringPi C Library
#

### Hardware Prep
* The default noctua fan connector will not connect directly to GPIO header. You need to do some modifications. There are multiple options:  
  > **1 - Dupont Jumper Wires Male to Female:**  
  > Noctua's existing wire is pretty long. Buy short jumper wires somewhere between 2-4" long. You need atleast 3 of them. Male part of jumper wire will connect to Noctua connector and Female part will connect to GPIO. This is what I used in the beginning for few months.

  > **2 - Dupont Female Pin Connectors 2.54mm Pitch:**  
  > This is the one I am using now, as shown in the preview. It is the cleanest option. It could be the most expensive option if you do not have all the tools. You need dupont Female pin connectors, crimping tool, wire cutter/stripper, heat shrink tube/heat gun **or** dupont connector housing.  
  > You can **either** buy connectors and housing kit **or** buy dupont connectors separately. Better options for dupont connectors are `Molex Crimp Terminals Series: 70058 Part No: 16020098` or `Harwin Series: M20 Part No: 1180042`. I used Molex connectors. For crimping I used IWISS `IWS-2820` crimping tool. Cover these connectors with **either** heat shrink tube (I used 3.00mm diameter tube) **or** dupont connector housing.  
  > If you go with this route. You have to cut the noctua fan wire to the required length. **Make sure you calculate required wire length properly before cutting**. If you cut it too short you will end up inserting wire joints. Strip wires (Practice stripping on the other end first). Attach dupont connectors to the stripped wires, crimp them with crimpping tool. Add heat shrink tubes to the connectors and shrink them with heatgun or attach connector housing.  

  > **3 - Use Wires from Old Fan:**  
  > If you have some old fan laying around that has dupont connector wires and you have no plan of using it. Cut the wires from that fan (You need 3 wires), cut the Noctua connector and do some wire joining (Google around if you do not know how to join electrical wires). Noctua fan comes with 4 OmniJoin adaptors, you can use that as well for joining wires.

* Complete specification of Noctua fan is available at [Noctua Whitepaper](https://noctua.at/pub/media/wysiwyg/Noctua_PWM_specifications_white_paper.pdf). Details of Raspberry Pi GPIO pin layout is available at [GPIO Pinout](https://pinout.xyz/). Screenshots attached for quick reference.

  | Fan Wires     |  GPIO Layout  |
  |:--------------|:--------------|
  | ![noctua_pin_config](https://user-images.githubusercontent.com/11185794/134500714-1cd90f97-a63e-43d6-9f87-c1f6856ca83e.png) | ![gpio_layout](https://user-images.githubusercontent.com/11185794/134590691-e4587fce-e01f-401a-a668-3a992159aa1f.png) |

* Fan wires connection to RP4 pins:  
  
  | Fan Wires       | RP4 Pins              |
  | :---            | :---                  |
  | Yellow +5V      | Physical Pin 4  |
  | Black Ground    | Physical Pin 6  |
  | Blue PWM Signal | Physical Pin 12 |

  Fan's PWM signal wire is connected to the RP4 `Physical/Board pin 12 - GPIO/BCM pin 18`. This fan-control code is targeted at GPIO pin 18. There are 4 pins on RP4 that supports hardware PWM `GPIO 12/13/18/19` You can use any one of those for this fan-control code. Make sure you replace the pin number in code with the one you use.  
The Green tachometer wire on Noctua fan is used to calculate RPM. I wrote the code for calculating RPM during development. It is fully functional code and uses `Physical/Board pin 16 - GPIO/BCM pin 23`. If you noticed that green wire is not connected in the preview. **I do not use this tachometer wire after final deployment**. Final code and binary in this repo have those function calls commented out. If you are interested in retrieving RPM, connect the Green Noctua fan wire to GPIO pin 23, uncomment `setupTacho` and `getFanRpm` function calls and rebuild binary.
#
### Steps
#### ⮞ Install Packages
* Install `WiringPi C` library. You can download it from [WiringPi](https://github.com/WiringPi/WiringPi). Installation instructions are available at [Install](https://github.com/WiringPi/WiringPi/blob/master/INSTALL).

  > **Quick Reference:**  
  > Unzip `sudo unzip -o WiringPi-master.zip -d WiringPi`  
  > Build and Install `sudo ./build`  

* Install `libsystemd-dev` package. This is needed for logging to `systemd journal`. This program prints to the journal at startup, exit and when fan is on (at 5 seconds interval). You can change the `log level` if you want to reduce logging. If you **do not** want journal logging at all from this fan-control code you can skip `libsystemd-dev` package installation and remove journal logging from code, explained in the `Build` section below.

  > **Install Package:**  
  > `sudo apt install libsystemd-dev`  
  
  > **Check Journal Logs:**  
  > `sudo journalctl -u fan-control`  

#### ⮞ Install FanControl
* Create a folder `/opt/gpio/fan` for fan-control binary (You can use your preferred location). Copy/Paste `fan-control` binary from this repo folder `build` to this newly created folder `/opt/gpio/fan` Make sure file is under the user:group of root and it is executable.

  > **Create folder:**  
  > `sudo mkdir -p /opt/gpio/fan/`  

  > **Make file executable:**  
  > `chmod +x /opt/gpio/fan/fan-control`  
  
  > **Change ownership of file [If needed]:**  
  > `sudo chown root:root /opt/gpio/fan/fan-control`  

* Create service to automatically run the fan-control at startup. Copy/Paste `fan-control.service` from this repo folder `service` to `/etc/systemd/system/`. Make sure file is under the user:group of root. Enable this service. This service starts `fan-control` as early as possible during startup without any warnings in the log.  

  > **Change ownership of file [If needed]:**  
  > `sudo chown root:root /etc/systemd/system/fan-control.service`  

  > **Enable Service:**  
  > `sudo systemctl enable fan-control`  

  > **Start Service [If needed]:**  
  > System reboot will automatically start this service. If you want to start it instantly without rebooting.  
  > `sudo systemctl start fan-control`  

  > **Check Service Status:**  
  > `sudo systemctl status fan-control`  

#
### Points to Note
* Fan will run at full speed when RP4 is booted. After few seconds when fan-control will be loaded **either** it will switch off **or** adjusts its speed, depending on CPU temperature.

* Fan-control service runs the fan within the temperature range from `48–58°C` and above. This is optimal range in my environment. Feel free to change the range according to your desired value and rebuild the binary. Noctua fan's recommeded minimum RPM is 1000. I kept the minimum RPM at 1500 in this service. Table below explains the fan's operation:

  |`Temp`|`RPM`|
  |:---|:---|
  |`<= 48°C`|`0`|
  |`> 48°C`|`1500`|
  |`Temp++`|`RPM++`|
  |`>= 58°C`|`5000`|  

* ***The 5V pins on RP4 i.e., physical pin 2 and 4 are not GPIO. They are connected to the 5V power supply and are always on.*** Those cannot be turned off without some form of circuit using mosfet or transistor. The point is if you run `shutdown command` from a shell or UI the fan will keep on running at full speed unless you unplug the RP4. I run my RP4 24x7, do a reboot from shell once in a while and unplug it if a shutdown is really needed for any reason.

#
### Build
* Binaries are available under repo folder `build`. It was built on `raspios-buster-arm64-lite` OS. If for any reason you want to rebuild.  
  > **Build command:**  
  > `sudo gcc -lsystemd -lwiringPi fan-control.c -o fan-control`  

* Test binary after build.  
  > **Run binary:**  
  > `sudo ./fan-control`  
  >  Ctrl+C to exit  

* If you are not interested in journal logging. Comment out the include header `sd-journal.h` and journal logging lines starting with `sd_journal_print`  
  > **Build command without journal logging:**  
  > `sudo gcc -lwiringPi fan-control.c -o fan-control`  
