# XPS-9570-Keyboard-Trackpad
Reverse engineering the keyboard and trackpad from a Dell XPS 9570 to repurpose as a USB keyboard combo thing.

Based around a Xiao RP2040 for USB-HID interface.

Keyboard: ECE1117 IC, used for keymap switching and backlight PWM control
Mouse: Synaptics IC, taking advantage of existing I2C-to-HID that is native to this part to support multitouch gestures

Leveraged motherboard schematic found here: https://drive.google.com/file/d/1tWQWGv8rafo04u2aQCC3-7ZbxCyPSDt3/view (not a virus I swear)
