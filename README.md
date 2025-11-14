This is a keypad I built for my CNC setup.  It sends GCode to the controller directly, allowing for PC-less local control of a stand-alone CNC controller.

![Picture of the first rev keypad](/about/hero.jpg)

* It's mostly custom coded for my needs, each button having a defined program for sending its gcode.
* It has some functionality for button groups (such as selected the X, Y, Z, or A Axis in a modal fashion), as well as jog increments.
* It has functionality for command queueing
* Multiple presses of the jog button are smooth and can be cycle-counted. ie: 5 presses of 0.001", goes 0.005"
* Checks status for alarm, and alerts.

The Keycaps I used were simple ones that allow you to place a paper label under a plastic lid.  When programming, the button order is as shown on my svg label template.

![Label Template & Button Order](/about/key_labels.png)
