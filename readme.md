# ECEN 5623 REAL-TIME EMBEDDED SYSTEMS FINAL PROJECT #
# REAL-TIME AUDIO ENCODING AND TRANSMISSION #

This firmware package is intended to provide a starting point for creating a FreeRTOS 10.2 based project in Code Composer Studio for the Tiva TM4C1294 launchpad, which is currently not supplied by either FreeRTOS or Texas Instruments. 
This firmware package has been further modified by Hemanth Nandish and Pranav .M. Bharadwaj to support the functionalities of ECEN 5623 Final Project

## Required Components ##
* Code Composer Studio (12.2 used for this project)
* [TM4C1294 Connected Launchpad](http://www.ti.com/tool/ek-tm4c1294xl)
* [Sparkfun SEN 12642 sound detector card](https://www.sparkfun.com/products/12642)
* [Artistic Style](http://astyle.sourceforge.net/astyle.html) (optional for formatted code)

## Expected Output ##
* Code will build with no errors (ignore the warnings)
* The sound detector card shall record audio at a sampling rate of 8 KHz
* The onboard ADC shall sample the analog data and digitize it. 
* G.711 encoding shall be implemented in accordance to [here](https://www.itu.int/rec/T-REC-G.711-198811-I/en)
* Serial port via Stellaris Virtual Serial Port will output "Hello from ECEN 5623 Final Project demo!"
