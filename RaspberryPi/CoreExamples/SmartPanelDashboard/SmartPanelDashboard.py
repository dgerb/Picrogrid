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

panelAddress = 0x08
boardAddresses = [0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x05, 0x05]
readCommands = ["RVB","RSOC","RI1","RI2","RI3","RI4","RCH1","RCH2","RCH3","RCH4","RIA1","RIA7"]
readVals = [0,0,0,0,0,0,0,0,0,0,0,0]
rewriteAddresses = [-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,0x08,0x08] # write above read result to another board address. skip if -1 
rewriteCommands = ["","","","","","","","","","","WIEI","WIEO"] # command to rewrite above read result. skip if ""
CH1IND = 6
writeCommands = ["WCP1","WCP2","WCP3","WCP4"]
soc = 0 # battery state of charge global variable
anyChannelActive = 0 # global variable describing if any of the channels are active (1) or none (0)
imppt = 0 # global variable for MPPT current into battery

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

def resetI2CBus():
    print("resetting PicroBoards in order to clear the I2C bus...\n")
    for pin in [24, 25]:
        GPIO.setup(pin, GPIO.OUT)
        GPIO.output(pin, False)
        GPIO.output(pin, True)
        GPIO.setup(pin, GPIO.IN)
    sleep(1)

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
    # global readCommands
    # global boardAddresses
    failureCounter = 0
    for n in range(len(readCommands)):
        [outString, success] = sendI2CCommand(boardAddresses[n], readCommands[n]+":\n")
        parsedOutStr = parseValueStr(outString)
        if success and represents_int(parsedOutStr):
            readVals[n] = int(parsedOutStr)
        else:
            failureCounter = failureCounter + 1
        sleep(0.2)
        if success and rewriteCommands[n] != "":
            [outString, success] = sendI2CCommand(rewriteAddresses[n], rewriteCommands[n]+":"+str(readVals[n])+"\n")
            if not success:
                failureCounter = failureCounter + 1
            sleep(0.2)
    # If too many i2c communications exceptions, i2c bus is likely stuck and must be reset
    if failureCounter > len(readCommands)-2:
        resetI2CBus()
        for n in range(0,4):
            [outString, success] = sendI2CCommand(panelAddress, writeCommands[n]+":"+str(readVals[CH1IND+n])+"\n")

# Connect to MariaDB
def connect_to_db():
    return mysql.connector.connect(
        host="localhost",       # MariaDB server (use localhost for local setup)
        user="panelpi",            # MariaDB username
        password="panelpipw", # Replace with your MariaDB root password
        database="paneldb"
    )

# Insert a new row with timestamp and vb,soc,i1,i2,i3,i4,ch1,ch2,ch3,ch4,ia1,ia7
def insert_data():
    global soc
    global anyChannelActive
    db_connection = connect_to_db()
    cursor = db_connection.cursor()

    vb = int(readVals[0])/1000.0
    soc = int(readVals[1])
    i1 = int(readVals[2])/1000.0
    i2 = int(readVals[3])/1000.0
    i3 = int(readVals[4])/1000.0
    i4 = int(readVals[5])/1000.0
    ch1 = int(readVals[6])
    ch2 = int(readVals[7])
    ch3 = int(readVals[8])
    ch4 = int(readVals[9])
    anyChannelActive = ch1 or ch2 or ch3 or ch4
    ia1 = int(readVals[10])/1000.0
    imppt = ia1
    ia7 = int(readVals[11])/1000.0
    psolar = vb*ia1
    pload = -1*vb*(i1+i2+i3+i4+ia7)
    pbattery = psolar + pload

    newVals = ["{:.2f}".format(vb), str(soc), "{:.2f}".format(i1), "{:.2f}".format(i2),
        "{:.2f}".format(i3), "{:.2f}".format(i4), 
        str(ch1), str(ch2), str(ch3), str(ch4), # RCH1-4
        "{:.2f}".format(ia1), "{:.2f}".format(ia7), # RIA1, RIA7
        "{:.2f}".format(psolar), "{:.2f}".format(pbattery), "{:.2f}".format(pload)] # Psolar, Pbattery, Pload
    # Insert the data into the table
    cursor.execute("""
        INSERT INTO channelRead (vb, soc, i1, i2, i3, i4, ch1, ch2, ch3, ch4, ia1, ia7, psol, pbatt, pload)
        VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
    """, (newVals[0], newVals[1], newVals[2], newVals[3], newVals[4], \
        newVals[5], newVals[6], newVals[7], newVals[8], newVals[9], \
        newVals[10], newVals[11], newVals[12], newVals[13], newVals[14]))
    db_connection.commit()
    print(f"{datetime.now()}, vb:{newVals[0]},soc:{newVals[1]},i1:{newVals[2]},i2:{newVals[3]},i3:{newVals[4]}"+\
        f",i4:{newVals[5]},ch1:{newVals[6]},ch2:{newVals[7]},ch3:{newVals[8]},ch4:{newVals[9]}"+\
        f",ia1:{newVals[10]},ia7:{newVals[11]},psol:{newVals[12]},pbatt:{newVals[13]},pload:{newVals[14]}")

    cursor.close()
    db_connection.close()

def check_for_button():
    # global readVals
    # global CH1IND
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

# Check SOC, and turn off channels if SOC < 25%
#   or if channels are off and SOC > 30%, turn them all on
def checkSOCShutoff():
    success = False
    while not success:
        if anyChannelActive and soc < 23: # turn off all channels
            sleep(0.2)
            [outString, success] = sendI2CCommand(panelAddress, "WCP1:0\n")
            sleep(0.2)
            [outString, success] = sendI2CCommand(panelAddress, "WCP2:0\n")
            sleep(0.2)
            [outString, success] = sendI2CCommand(panelAddress, "WCP3:0\n")
            sleep(0.2)
            [outString, success] = sendI2CCommand(panelAddress, "WCP4:0\n")
            sleep(0.2)
        elif not anyChannelActive and soc > 30: # turn on all channels
            sleep(0.2)
            [outString, success] = sendI2CCommand(panelAddress, "WCP1:1\n")
            sleep(0.2)
            [outString, success] = sendI2CCommand(panelAddress, "WCP2:1\n")
            sleep(0.2)
            [outString, success] = sendI2CCommand(panelAddress, "WCP3:1\n")
            sleep(0.2)
            [outString, success] = sendI2CCommand(panelAddress, "WCP4:1\n")
            sleep(0.2)
        else:
            break
        if not success:
            resetI2CBus()
            sleep(1)

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

        # Check SOC, and turn off channels if SOC < 25% and no MPPT current
        #   or if channels are off and (SOC > 25% or MPPT current detected), turn them all on
        checkSOCShutoff()
        sleep(0.2)
