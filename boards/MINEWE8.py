#!/bin/false
# This file is part of Espruino, a JavaScript interpreter for Microcontrollers
#
# Copyright (C) 2013 Gordon Williams <gw@pur3.co.uk>
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# ----------------------------------------------------------------------------------------
# This file contains information for a specific board - the available pins, and where LEDs,
# Buttons, and other in-built peripherals are. It is used to build documentation as well
# as various source and header files for Espruino.
# ----------------------------------------------------------------------------------------

import pinutils;

info = {
 'name' : "Minew E8 Beacon",
 'link' :  [ "https://www.minew.com/bluetooth-beacons/e8-tag-beacon.html" ],
 'espruino_page_link' : 'MinewE8',
 'default_console' : "EV_SERIAL1",
 'default_console_tx' : "D2",
 'default_console_rx' : "D3",
 'default_console_baudrate' : "9600",
 'variables' : 2500, # How many variables are allocated for Espruino to use. RAM will be overflowed if this number is too high and code won't compile.
 'bootloader' : 1,
 'binary_name' : 'espruino_%v_minewe8.hex',
 'build' : {
   'optimizeflags' : '-Os',
   'libraries' : [
     'BLUETOOTH',
   ],
   'makefile' : [
     'DEFINES+=-DHAL_NFC_ENGINEERING_BC_FTPAN_WORKAROUND=1', # Looks like proper production nRF52s had this issue
     'DEFINES+=-DBLUETOOTH_NAME_PREFIX=\'"Minew"\'',
     'DEFINES+=-DNFC_DEFAULT_URL=\'"https://www.espruino.com/ide"\'',
     'DEFINES+=-DDUMP_IGNORE_VARIABLES=\'"Minew\\0"\'',
     'DFU_PRIVATE_KEY=targets/nrf5x_dfu/dfu_private_key.pem',
     'DFU_SETTINGS=--application-version 0xff --hw-version 52 --sd-req 0x8C',
     'INCLUDE += -I$(ROOT)/libs/minew_e8',
     'WRAPPERSOURCES += libs/minew_e8/jswrap_minew_e8.c',
     'JSMODULESOURCES += libs/js/LIS3DH.min.js',
     'JSMODULESOURCES+=libs/js/minew/e8.min.js'
   ]
 }
};


chip = {
  'part' : "NRF52832",
  'family' : "NRF52",
  'package' : "QFN48",
  'ram' : 64,
  'flash' : 512,
  'speed' : 64,
  'usart' : 1,
  'spi' : 1,
  'i2c' : 1,
  'adc' : 1,
  'dac' : 0,
  'saved_code' : {
    'address' : ((118 - 10) * 4096), # Bootloader takes pages 120-127, FS takes 118-119
    'page_size' : 4096,
    'pages' : 10,
    'flash_available' : 512 - ((31 + 8 + 2 + 10)*4) # Softdevice uses 31 pages of flash, bootloader 8, FS 2, code 10. Each page is 4 kb.
  },
};

'''
P0.07 - Accelerometer INT1
P0.08 - Accelerometer INT2
P0.11 - Accelerometer SCL
P0.12 - Accelerometer SDA
P0.17 - LED1, DTM port
P0.18 - DTM port 
'''

devices = {
  'LED1' : { 'pin' : 'D17' }, # Pin negated in software
  'LIS3DH' : {'pin_scl' : 'D11', 'pin_sda' : 'D12', 'pin_int1' : 'D7', 'pin_res' : 'D6', 'pin_int2' : 'D8'},
};

# left-right, or top-bottom order
board = {
  'left' : [ 'VDD', 'VDD', 'RESET', 'VDD','5V','GND','GND','PD3','PD4','PD28','PD29','PD30','PD31'],
  'right' : [ 'PD27', 'PD26', 'PD2', 'GND', 'PD25','PD24','PD23', 'PD22','PD20','PD19','PD18','PD17','PD16','PD15','PD14','PD13','PD12','PD11','PD10','PD9','PD8','PD7','PD6','PD5','PD21','PD1','PD0'],
};
board["_css"] = """
""";

def get_pins():
  # 32 General Purpose I/O Pins, 16 'virtual' Port Expanded pins
  pins = pinutils.generate_pins(0,31,"D") + pinutils.generate_pins(0,15,"V");
  pinutils.findpin(pins, "PD0", True)["functions"]["XL1"]=0;
  pinutils.findpin(pins, "PD1", True)["functions"]["XL2"]=0;
  pinutils.findpin(pins, "PD5", True)["functions"]["RTS"]=0;

  pinutils.findpin(pins, "PD11", True)["functions"]["LIS3DH_SCL"]=0;
  pinutils.findpin(pins, "PD12", True)["functions"]["LIS3DH_SDA"]=0;
  pinutils.findpin(pins, "PD7", True)["functions"]["LIS3DH_INT1"]=0;
  pinutils.findpin(pins, "PD8", True)["functions"]["LIS3DH_INT2"]=0;

  # everything is non-5v tolerant
  for pin in pins:
    pin["functions"]["3.3"]=0;

  #The boot/reset button will function as a reset button in normal operation. Pin reset on PD21 needs to be enabled on the nRF52832 device for this to work.
  return pins
