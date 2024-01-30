#!/usr/bin/env python

import sys
import csv 

MINUTEINDEX = 4

fileName = sys.argv[1]

data = []
data1min = []

minute = -1

with open(fileName) as file_obj: 
    reader_obj = csv.reader(file_obj) 
    for row in reader_obj: 
        data.append(row)

averageAcc = [0] * len(data[0])
averageN = [0] * len(data[0])

for n in range(1,len(data)):
    if not minute == data[n][MINUTEINDEX]:
        minute = data[n][MINUTEINDEX]
        line1min = []
        for m in range(0,len(data[0])):
            if averageN[m] > 0:
                line1min.append(round(averageAcc[m]/averageN[m]))
            else:
                line1min.append(averageAcc[m])
        data1min.append(line1min)
        averageAcc = [0] * len(data[n])
        averageN = [0] * len(data[n])
    for m in range(0,len(data[0])):
        if data[n][m] == '???' or data[n][m] == 'Error':
            data[n][m] = data[n-1][m]
        else:
            averageAcc[m] = averageAcc[m] + int(data[n][m])
            averageN[m] = averageN[m] + 1

data1min[0] = data[0]

with open("output_clean.csv","w+") as my_csv:
    csvWriter = csv.writer(my_csv,delimiter=',')
    csvWriter.writerows(data)

with open("output_1min.csv","w+") as my_csv:
    csvWriter = csv.writer(my_csv,delimiter=',')
    csvWriter.writerows(data1min)