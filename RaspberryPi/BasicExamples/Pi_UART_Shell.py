
#!/usr/bin/python3

# sudo raspi-config > interfaces > serial, disable serial login but keep serial port enabled
# ex: python3 bms_shell.py -b 9600

import sys, getopt
import serial
from time import sleep

serialPort = "/dev/ttyUSB0"
argv = sys.argv[1:];
usage = "Usage : bms_shell [-b baudrate] "
opts, args = getopt.getopt(argv,"hb:",["baud="])

baud = 9600
for opt, arg in opts:
    if opt in ("-b", "--baud"):
         baud = arg
    if opt == '-h':
         print (usage)
         print ("Enter commands without quotes and '\\n's. eg:R010000")
         sys.exit()

print (usage)
print ("Port : [" + serialPort + "] Baud : [" + str(baud) + "]")
# print ("Enter commands without quotes and '\\n's. eg:R010000")

try:
    ser = serial.Serial (port=serialPort, baudrate = baud,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        bytesize=serial.EIGHTBITS,
        timeout=1)
except serial.SerialException as ex:
    print ("Could not open /dev/ttyUSB0. Is it enabled it in the kernel?")
    sys.exit(-1)

while True:
    # Get the request sting
    print("\nInput format in COMMAND:VALUE")
    val = input(">>>>   ")
    if val == "bye":
        sys.exit(0)
    val += '\n'

    # Send request
    ser.write(val.encode('utf_8'))
    received_data = ser.readline()
    print ("Response> : " + received_data.decode('utf-8'))
