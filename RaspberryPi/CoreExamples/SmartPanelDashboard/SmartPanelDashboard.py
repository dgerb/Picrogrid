import mysql.connector
import random
import time
from datetime import datetime

lastCheckTime = None

# Connect to MariaDB
def connect_to_db():
    return mysql.connector.connect(
        host="localhost",       # MariaDB server (use localhost for local setup)
        user="panelpi",            # MariaDB username
        password="panelpipw", # Replace with your MariaDB root password
        database="paneldb"
    )

# Insert a new row with timestamp and three random numbers
def insert_random_data():
    db_connection = connect_to_db()
    cursor = db_connection.cursor()

    # Generate three random numbers
    num1 = random.randint(1, 100)
    num2 = random.randint(1, 100)
    num3 = random.randint(1, 100)

    # Insert the data into the table
    cursor.execute("""
        INSERT INTO channelRead (num1, num2, num3)
        VALUES (%s, %s, %s)
    """, (num1, num2, num3))

    db_connection.commit()
    print(f"Inserted new row: {datetime.now()} | num1: {num1}, num2: {num2}, num3: {num3}")

    cursor.close()
    db_connection.close()

def check_for_button():
    global lastCheckTime
    db_connection = connect_to_db()
    cursor = db_connection.cursor()
    query = """
        SELECT id, timestamp
        FROM channelWrite
        WHERE timestamp >= %s
        """
    cursor.execute(query, (lastCheckTime,))
    rows = cursor.fetchall()
    if rows:
        print(f"Detected {len(rows)} updated row(s).")
        for row in rows:
            print(row)

    lastCheckTime = datetime.now()

    cursor.close()
    db_connection.close()

if __name__ == "__main__":
    while True:
        check_for_button()
        insert_random_data()
        time.sleep(10)  # Wait for 10 seconds before inserting again
