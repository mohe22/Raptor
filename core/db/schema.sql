PRAGMA journal_mode = WAL;
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS logLevels (
    id    INTEGER PRIMARY KEY,
    level TEXT    NOT NULL CHECK (
        level IN ('DEBUG', 'INFO', 'WARN', 'ERROR', 'FATAL')
    )
);

INSERT OR IGNORE INTO logLevels(id, level) VALUES
    (0, 'DEBUG'),
    (1, 'INFO'),
    (2, 'WARN'),
    (3, 'ERROR'),
    (4, 'FATAL');

CREATE TABLE IF NOT EXISTS logCategories (
    id       INTEGER PRIMARY KEY,
    category TEX TNOT NULL CHECK (
        category IN ('SYSTEM', 'SERVER', 'AGENT', 'SESSION')
    )
);

INSERT OR IGNORE INTO logCategories(id, category) VALUES
    (0, 'SYSTEM'),
    (1, 'SERVER'),
    (2, 'AGENT'),
    (3, 'SESSION');

CREATE TABLE IF NOT EXISTS logs (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    ts         INTEGER NOT NULL DEFAULT (unixepoch()),
    levelId    INTEGER NOT NULL REFERENCES logLevels(id),
    categoryId INTEGER NOT NULL REFERENCES logCategories(id),
    event      TEXT    NOT NULL,
    message    TEXT    NOT NULL,
    meta       TEXT
);

CREATE INDEX IF NOT EXISTS idx_logs_ts       ON logs(ts);
CREATE INDEX IF NOT EXISTS idx_logs_level    ON logs(levelId);
CREATE INDEX IF NOT EXISTS idx_logs_category ON logs(categoryId);
CREATE INDEX IF NOT EXISTS idx_logs_event    ON logs(event);
