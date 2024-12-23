#!/usr/bin/env python

# first enable I2C: sudo raspi-config
# also install smbus2: pip3 install smbus2

# optionally install i2c-tools: sudo apt install i2c-tools
# check I2C connections with: sudo i2cdetect -y 1
# note that the Atverters won't show up, but this can test the health of the I2C bus

# to start a session through ssh and log off:
# sudo apt install tmux
# tmux
# python3 ~/Picrogrid/RaspberryPi/CoreExamples/SmartPanelDashboard/SmartPanelDashboard.py &
# <ctrl+b and $ (to name session)>
# <ctrl+b and d (to exit tmux)>

# to get back into your session:
# tmux ls
# tmux attach-session -t <session-name>
# ps
# kill -9 <python3 process name>
# <ctrl+b and d (to exit tmux)>
# tmux kill-session -t <session-name>

from time import sleep
import sys
from datetime import datetime
import csv
import smbus2
import os
import RPi.GPIO as GPIO
import mysql.connector
import random

# vb = 54
# i1 = 1
# i2 = 2
# i3 = 3
# i4 = 4
# ch1 = True
# ch2 = True
# ch3 = True
# ch4 = True

panelAddress = 0x08
readCommands = ["RVB","RI1","RI2","RI3","RI4","RCH1","RCH2","RCH3","RCH4"]
readVals = [0,0,0,0,0,0,0,0,0]
I1IND = 1
CH1IND = 5
writeCommands = ["WCP1","WCP2","WCP3","WCP4"]

def StringToBytes(val):
    retVal = []
    for c in val:
        retVal.append(ord(c))
    return retVal

def parseValueStr(message):
    splitArr = message.split(':')
    if len(splitArr) < 2 or splitArr[0] == "Error":
        return "Error"
    else:
        valueStr = splitArr[1]
        if valueStr[0] == '=':
            return "Error"
        valueStr = valueStr.replace('\n','')
        valueStr = valueStr.replace('\r','')
        return valueStr

def represents_int(s):
    try: 
        int(s)
    except ValueError:
        return False
    else:
        return True

def sendI2CCommand(address, command):
    try:
        byteValue = StringToBytes(command) # I2C requires bytes
        bus.write_i2c_block_data(address, 0x00, byteValue) # Send the byte packet to the slave.
        sleep(0.2)
        data = bus.read_i2c_block_data(address, 0x00, 32) # Send a register byte to slave with request flag.
        outString = ""
        for n in data:
            if n==255: # apparently this is the python char conversion result of '\0' in C++
                break
            outString = outString + chr(n) # assemble the byte result as a string to print
        return [outString, True]
    except:
        outString = '???'
        return [outString, False]

def readPicroBoard():
    global readVals
    global readCommands
    global panelAddress
    failureCounter = 0
    for n in range(len(readCommands)):
        [outString, success] = sendI2CCommand(panelAddress, readCommands[n]+":\n")
        parsedOutStr = parseValueStr(outString)
        if success and represents_int(parsedOutStr):
            readVals[n] = int(parsedOutStr)
        else:
            failureCounter = failureCounter + 1
        sleep(0.2)
    # If too many i2c communications exceptions, i2c bus is likely stuck and must be reset
    if failureCounter > len(readCommands)-2:
        print("resetting PicroBoards in order to clear the I2C bus...\n")
        pin = 24
        GPIO.setup(pin, GPIO.OUT)
        GPIO.output(pin, False)
        GPIO.output(pin, True)
        GPIO.setup(pin, GPIO.IN)

# Connect to MariaDB
def connect_to_db():
    return mysql.connector.connect(
        host="localhost",       # MariaDB server (use localhost for local setup)
        user="panelpi",            # MariaDB username
        password="panelpipw", # Replace with your MariaDB root password
        database="paneldb"
    )

# Insert a new row with timestamp and vb,i1,i2,i3,i4,ch1,ch2,ch3,ch4
def insert_data():
    db_connection = connect_to_db()
    cursor = db_connection.cursor()

    newVals = ["{:.2f}".format(int(readVals[0])/1000.0), \
        "{:.2f}".format(int(readVals[1])/1000.0), "{:.2f}".format(int(readVals[2])/1000.0), \
        "{:.2f}".format(int(readVals[3])/1000.0), "{:.2f}".format(int(readVals[4])/1000.0), \
        str(int(readVals[5])), str(int(readVals[6])), str(int(readVals[7])), str(int(readVals[8]))]
    # Insert the data into the table
    cursor.execute("""
        INSERT INTO channelRead (vb, i1, i2, i3, i4, ch1, ch2, ch3, ch4)
        VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s)
    """, (newVals[0], newVals[1], newVals[2], newVals[3], newVals[4], \
        newVals[5], newVals[6], newVals[7], newVals[8]))
    db_connection.commit()
    print(f"{datetime.now()}, vb:{newVals[0]}, i1:{newVals[1]},i2:{newVals[2]},i3:{newVals[3]}"+\
        f",i4:{newVals[4]}, ch1:{newVals[5]},ch2:{newVals[6]},ch3:{newVals[7]},ch4:{newVals[8]}")

    cursor.close()
    db_connection.close()

def check_for_button():
    global readVals
    global CH1IND
    db_connection = connect_to_db()
    cursor = db_connection.cursor(dictionary=True)
    cursor.execute("""SELECT * FROM channelWrite""")
    rows = cursor.fetchall()
    if rows:
        for row in rows:
            chSelect = row['channel']
            print("Channel " + str(chSelect) + " button pressed")
            if chSelect == 1 or chSelect == 2 or chSelect == 3 or chSelect == 4:
                readVals[CH1IND + chSelect - 1] = int(not readVals[CH1IND + chSelect - 1])
                writeCommand = writeCommands[chSelect - 1]+":"+str(readVals[CH1IND + chSelect - 1])+"\n"
                [outString, success] = sendI2CCommand(panelAddress, writeCommand)
    cursor.execute("""DELETE FROM channelWrite""")
    db_connection.commit()  # Commit the changes to the database
    cursor.close()
    db_connection.close()
    return (len(rows) > 0)



if __name__ == "__main__":
    print("Remember to make sure I2C is enabled in raspi-config.")
    GPIO.setmode(GPIO.BCM)
    bus = smbus2.SMBus(1)
    sleep(2) # Give the I2C device time to settle
    while True:
        # Read data from the PicroBoard
        readPicroBoard()
        sleep(0.5) # Give a nice and long delay to make sure things got read and recorded properly

        # Check if button pressed in Grafana dashboard, send write channel command if so
        if check_for_button():
            sleep(0.5) # Give a nice and long delay to make sure things got written properly

        # Store data in DB for Grafana to read
        insert_data()
        sleep(0.2)

