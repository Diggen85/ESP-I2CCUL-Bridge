# ESP-I2CCUL-Bridge
Adapter um mehrere CULs über I2C an einem ESP im Wifi bereitzustellen.

Connect many CUL devices over Wifi and Ínterface them like a SCC device, writen in Arduino C.
Needs the I2CCUL firmware with an unique Address (see heliflieger/a-culfw )

## I2CCUL Device Addresses
```
#define CULCOUNT       4
const char CULADDR[CULCOUNT] = {0x70, 0x71, 0x7c2, 0x73;
```

## Hardware Breadboard
![Breadboard](https://raw.githubusercontent.com/Diggen85/ESP-I2CCUL-Bridge/master/Breadboard.jpg)

## FHEM Definition - <http://www.fhem.de>
```
define CUL_ESP CUL X.X.X.X:23 0000
define CUL_ESP2 STACKABLE_CC CUL_ESP
define CUL_ESP3 STACKABLE_CC CUL_ESP2
define CUL_ESP4 STACKABLE_CC CUL_ESP3
```

# License
```
This program is free software; you can redistribute it and/or modify it under  
the terms of the GNU General Public License as published by the Free Software  
Foundation; either version 2 of the License, or (at your option) any later  
version.

This program is distributed in the hope that it will be useful, but  
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY  
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for  
more details.

You should have received a copy of the GNU General Public License along with  
this program; if not, write to the  
Free Software Foundation, Inc.,  
51 Franklin St, Fifth Floor, Boston, MA 02110, USA
```
=======


