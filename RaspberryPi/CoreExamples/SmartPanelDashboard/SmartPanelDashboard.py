import mysql.connector
import random
import time
from datetime import datetime

lastCheckTime = None

vb = 54
i1 = 1
i2 = 2
i3 = 3
i4 = 4
ch1 = True
ch2 = True
ch3 = True
ch4 = True

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

    vb = 54 + random.randrange(-5,5)
    i1 = 1 + random.randrange(-1,2)
    i2 = 2 + random.randrange(-1,2)
    i3 = 3 + random.randrange(-1,2)
    i4 = 4 + random.randrange(-1,2)

    # Insert the data into the table
    cursor.execute("""
        INSERT INTO channelRead (vb, i1, i2, i3, i4, ch1, ch2, ch3, ch4)
        VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s)
    """, (vb, i1, i2, i3, i4, ch1, ch2, ch3, ch4))

    db_connection.commit()
    print(f"{datetime.now()},vb:{vb},i1:{i1},i2:{i2},i3:{i3},i4:{i4},ch1:{ch1},ch2:{ch2},ch3:{ch3},ch4:{ch4}")

    cursor.close()
    db_connection.close()

def check_for_button():
    global lastCheckTime
    global ch1
    global ch2
    global ch3
    global ch4
    db_connection = connect_to_db()
    cursor = db_connection.cursor(dictionary=True)
    query = """
        SELECT id, timestamp, channel
        FROM channelWrite
        WHERE timestamp >= %s
        """
    cursor.execute(query, (lastCheckTime,))
    rows = cursor.fetchall()
    if rows:
        for row in rows:
            chSelect = row['channel']
            print("Channel " + str(chSelect) + " button pressed")
            if chSelect == 1:
                ch1 = not ch1
            elif chSelect == 2:
                ch2 = not ch2
            elif chSelect == 3:
                ch3 = not ch3
            elif chSelect == 4:
                ch4 = not ch4
    lastCheckTime = datetime.now()
    cursor.close()
    db_connection.close()
    return (len(rows) > 0)

if __name__ == "__main__":
    insert_data()
    while True:
        for n in range(0,10):
            if check_for_button():
                insert_data()
            time.sleep(0.5)
        insert_data()
