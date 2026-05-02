# blinkpwd

A simple password entry system for the nRF52 DK demonstrating:
- GPIO input handling (button presses)
- LED control and timing
- State machine logic (locked/unlocked states)
- Basic embedded application structure in Zephyr

## Building

I used the VS Code Extension "nRF Connect for VS Code Extension Pack" to build this project.
The Nordic Academy has great instructions on how to build a simple project like this: https://academy.nordicsemi.com/courses/nrf-connect-sdk-fundamentals/.
The extensions also makes it extremely easy to flash the project to the controller.
I have only tested this project on a nRF52 Development Kit.

## Usage

Once the software is flashed on the chip, the "lock" will be locked and ready for an attempt to enter the password.
Press 4 of the buttons 1 through 4 to attempt to enter a password. The default password is "1324".
Once entered correctly, all 4 LEDs on the board will light up, indicating that that the password was entered correctly.
At this point the "lock" is "unlocked" and the keypad is ready for a new password to be entered.
Press 4 buttons in whichever order you like to create the new password. Buttons can be repeated, so the password could be "1111".
After the fourth button is pressed, the LEDs will turn off, indicating that the new password has been set and the lock is now locked.
You can now continue by entering the new password to unlock the lock again.

## What I learned

- Working with Zephyr RTOS
- GPIO configuration and interrupt handling
- Managing state in an embedded context
- Using Nordic's development tools and SDK

## Future improvements

- Lock input keypad while  LEDs are blinking
- Flash LED corresponding to the button that was just entered
- Explore improvements to be made to the asynchronous behaviour
- Debounce button presses for better input behaviour
- Configuration of initial password, LED timings etc. in #defines, or even config files
