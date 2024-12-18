
DROP USER IF EXISTS 'panelpi'@'localhost';
CREATE USER 'panelpi'@'localhost' IDENTIFIED BY 'panelpipw';
GRANT ALL PRIVILEGES ON paneldb.* TO 'panelpi'@'localhost';

USE paneldb;

-- Create the table with columns for timestamp and three random numbers

CREATE TABLE channelRead (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    vb INT,
    i1 INT,
    i2 INT,
    i3 INT,
    i4 INT,
    ch1 BOOL,
    ch2 BOOL,
    ch3 BOOL,
    ch4 BOOL
);

CREATE TABLE channelWrite (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    channel INT,
    state BOOL
);

-- Delete older entries

SET GLOBAL event_scheduler = ON;

DELIMITER $$

CREATE EVENT delete_old_rows
ON SCHEDULE EVERY 1 DAY
DO
BEGIN
    DELETE FROM channelRead
    WHERE timestamp < NOW() - INTERVAL 7 DAY;
    DELETE FROM channelWrite
    WHERE timestamp < NOW() - INTERVAL 1 DAY;
END$$

DELIMITER ;
