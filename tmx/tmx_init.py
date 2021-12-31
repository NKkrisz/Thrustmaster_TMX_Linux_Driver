#!/usr/bin/env python3
import usb1
from os import path
import time

with usb1.USBContext() as context:
#   Prep for 1st write
    handle = context.openByVendorIDAndProductID(
        0x44f,
        0xb67e,
        skip_on_error=True
    )
    if handle is None:
        print("No such device")
#   Write 1st control packet
    try:
        handle.setAutoDetachKernelDriver(True)
        handle.claimInterface(0)
        handle.controlWrite(
                0x41,
                83,
                0x0001,
                0x0000,
                b''            
        )
    except:
        print("Error (But it might be ok)")
        pass
    
    time.sleep(5)

#   Prep for 2nd write
    handle = context.openByVendorIDAndProductID(
        0x44f,
        0xb65d,
        skip_on_error=True
    )
    if handle is None:
        print("No such device")
#   Write 2nd control packet
    try:
        handle.setAutoDetachKernelDriver(True)
        handle.claimInterface(0)              
        handle.controlWrite(
                0x41,
                83,
                0x0007,
                0x0000,
                b''        
        )
    except:
        print("Error (But it might be ok)")
        pass
