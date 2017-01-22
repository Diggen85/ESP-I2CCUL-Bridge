### ESP-I2CCUL-Bridge

Proof-of-Concept to connect many CUL devices over Wifi and Ínterface them like a SCC device, writen in Arduino C.
Needs the I2CCUL firmware with an unique Address (see a-culfw fork)

##I2CCUL Device definition
```
#define CULCOUNT       3
const char CULADDR[CULCOUNT] = {0x7e, 0x7d, 0x7c};
```

##Wifi Credentials
```
const char* WifiSSID      = "****";
const char* WifiPassword  = "****";
```



#### License
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


