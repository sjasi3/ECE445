## 02/25/2026:
Began and completed kicad_pcb file. PCB layout was finished. Helped with KiCAD navigation and verification.
## 03/05/2026:
Did preliminary software and hardware tests for the ESP32 and motor drivers. We measured the noise values and inductive spike of the motors to be almost non-existent.
![[Pasted image 20260310154439.png|200]]![[Pasted image 20260310154530.png|200]]![[Pasted image 20260310154548.png|200]]![[Pasted image 20260310154608.png|200]]
## 03/09/2026:
Created the software portion of breadboard demo which added features including.

| Feature           | Description                                                                               | Portion Complete | Considerations                                                              |
| ----------------- | ----------------------------------------------------------------------------------------- | ---------------- | --------------------------------------------------------------------------- |
| Motor Control     | PWM signal to control the motor drivers and ultimately the speed of the motor (air pumps) | ~90%             | Motor backfeed into power lines causes servo to trigger random positions    |
| Solenoid Control  | GPIO HI and LO level signals to control solenoid MOSFETs to turn on or off solenoid.      | 100%             |                                                                             |
| Servo Control     | Standard 50Hz servo PWM signal to control servo position                                  | ~90%             | See motor control issue                                                     |
| Wireless AP       | Wireless network which allows a user to connect to the self-hosted WebUI                  | ~100%            | May want to make this more convenient, but can be considered fully finished |
| Self-Hosted WebUI | Website which allows for user interaction with the self-playing harmonica                 | ~25%             | Need to rework WebUI to use MIDI instead of just  direct user input         |
Notice that at each image below, the PWM slider value changes and the LED becomes brighter or dimmer depending on the slider value. This is a preliminary test that PWM is functional before hooking up the uC to the motor drivers.
See attached images below for breadboard demo WebUI
![[Pasted image 20260310155912.png|200]] ![[Pasted image 20260310155931.png|200]] ![[Pasted image 20260310155944.png|200]] ![[Pasted image 20260310155955.png|200]] 

We noticed that there are issues with the servo when the air pumps are powered. See attached images below. 
![[Pasted image 20260310155733.png|200]] 
## 03/09/2026 & 03/10/2026
We made several attempts at mitigating this messy signal with varying degrees of success.

| LC lowpass                                                                                        | RL lowpass                                                                                                                                      | Ferrite bead                                                                                                                                                | Ferrite choke                                                                                                                                            |
| ------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| This barely made a difference in the signal and the servo output when the air pump was turned on. | This made the biggest difference in the servo control when the air pump was turned on. However, the signal was just as messy as the LC lowpass. | This made the biggest difference in the servo signal when the air pump was turned on, but it did not largely affect servo control when the pump was turned. | This made the motor Driver unable to turn on. We suspect that the high frequency switching/PWM of the ESP32 is unable to pass through the ferrite choke. |
## 03/24/26
Met with TA and soldered PCB with reflow oven. Fixed bridging on buck converter IC.
## 03/26/26
Cleaned up soldering on buck converter IC. Fixed bridging and cleaned up solder for USB micro B connector. Did initial programming of board. Did motor testing, LED testing, and button input testing. 
## 04/07/26
Fixed issue where solenoids would not turn off in software.
Tested board revision 3 with no load, all lines and drivers appear to be working without issue. 
Tested board revision 3 with all solenoids, but no motor load. Everything appears to be working without issue
- TODO: test with load to see if any irregularities due to load
- NOTE: one solenoid felt really weak during testing
## 04/15/26
Replacing solenoid confirms one solenoid is particularly weak. All solenoid drivers seem fully functional.
## 04/23/26
Tested board fully with motors, servo, and solenoid. Airflow seems to be very restricted by the solenoids and not the motors which we initially thought was the issue. The solution we will be testing is to 3D print the solenoid coverings to widen the hole so more air can flow.
## 04/26/26
Tested the 3D printed solenoid coverings with notable success as the harmonica is able to play louder than in previous tests. This shows that the issues stem from both the initial motor and the solenoids that were chosen for the project.

Did latency testing. Latency is less than 100ms based on stopwatch reference and video analysis. Command send at 7.246 Command receive at 7.310 delta 64ms (64 ms was also the fastest the video could even react)
![[Pasted image 20260427105839.png]]
![[Pasted image 20260427105520.png]]