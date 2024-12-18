
DROP USER IF EXISTS 'panelpi'@'localhost';
CREATE USER 'panelpi'@'localhost' IDENTIFIED BY 'panelpipw';
GRANT ALL PRIVILEGES ON paneldb.* TO 'panelpi'@'localhost';

USE paneldb;

-- Create the table with columns for timestamp and three random numbers

CREATE TABLE channelRead (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    num1 INT,
    num2 INT,
    num3 INT
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
    WHERE timestamp < NOW() - INTERVAL 1 DAY;
    DELETE FROM channelWrite
    WHERE timestamp < NOW() - INTERVAL 1 DAY;
END$$

DELIMITER ;
