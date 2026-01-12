# python3 Picrogrid/RaspberryPi/BasicExamples/Pi_I2C_Shell.py

#!/usr/bin/env python
from time import sleep
import smbus2
bus = smbus2.SMBus(1)
address = 0x08

def StringToBytes(val):
    retVal = []
    for c in val:
        retVal.append(ord(c))
    return retVal

print("what is the I2C address? (0 to 127)")
addrStr = input(">>>>   ")
address = int(addrStr)

print("Remember to make sure I2C is enabled in raspi-config.")

# Give the I2C device time to settle
sleep(2)

while 1:
    print("\nInput format in COMMAND:VALUE")
    charstring = input(">>>>   ")
    byteValue = StringToBytes(charstring) # I2C requires bytes
    # Send the byte packet to the slave. This could be a set command or get command.
    #   For get commands, this will arm the TX register with the returned value, but not return it yet
    bus.write_i2c_block_data(address, 0x00, byteValue)
    # Send a register byte to slave with request flag.
    #   Calls onReceive() with the register byte, but this is useless to our code
    #   Also calls onRequest(), which is what returns the TX data back to the master
    data = bus.read_i2c_block_data(address, 0x00, 32)
    outString = ""
    for n in data:
        if n==255: # apparently this is the python char conversion result of '\0' in C++
            break
        outString = outString + chr(n) # assemble the byte result as a string to print
    print(outString)
    sleep(0.5)
    
    