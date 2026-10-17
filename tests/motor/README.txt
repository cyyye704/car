STC32G E14 continuous dual-motor toggle host logic tests

Run from D:\PROJECTS\car:
  powershell -NoProfile -ExecutionPolicy Bypass -File tests\motor\run.ps1

Default compiler: C:\msys64\ucrt64\bin\gcc.exe
Override with: -Compiler <gcc.exe absolute path>
Build output: .build\motor-logic\test_motor.exe
Last results and tested firmware SHA256: .build\motor-logic\latest.log

test_motor.c includes the real E14_motor_test/user/main.c and renames its
main function with a preprocessor macro. It does not copy the state machine.
Each named scenario runs in its own process to preserve real C static
initialization. The local header mocks GPIO/PWM/PIT/clock/interrupt APIs.
Initialization returns to the harness when global interrupts would be enabled;
the harness then invokes the registered Timer0 callback once per simulated tick.

Checks:
- PWM starts at zero; 17 kHz, 5 ms PIT and Timer0 priority 3 configured.
- M1/P76 starts LOW and M2/P77 HIGH; GPIO writes at runtime only target LED.
- Boot lockout lasts 1 second. Holding P32 at startup cannot start motors.
- A fresh 100 ms stable release is required before the first valid press.
- Presses shorter than 20 ms do not start the motors.
- A valid start sets both channels to the percentage configured by
  MOTOR_DUTY_PERCENT in main.c; changing that percentage does not require
  rewriting test expectations. This does not verify physical wheel speed.
- Both a held startup press and a released button keep the motors running
  for more than 60 seconds; the previous 300 ms timeout no longer applies.
- Running must observe 100 ms stable release before another press can stop.
- After that release, the first low sample stops both channels.
- Long stop presses and contact bounce do not restart the motors.
- Restart needs 100 ms fresh release and 20 ms press, without an additional
  1 second cooldown. Repeated start/stop sequences remain stable.
- The running LED stays lit; stopped/ready has a 100 ms per second heartbeat;
  boot and release-lock states are dark.
- At each callback boundary both channels are equal. Intermediate sequential
  API writes can briefly differ and are intentionally not claimed atomic.
- Four fixed-seed random input sequences check those invariants independently
  through mock input/output histories.

Limits:
These are host tests of application logic, not an emulator of Keil C251, the
STC32G registers, PWM hardware, interrupt scheduling or the DRV8701E. They do
not measure electrical startup/reset levels, actual 5 ms timing, PWM waveforms,
wiring, motor direction/current or physical braking. The separate Keil build
and subsequent driver-off startup / controlled bench test remain necessary.
