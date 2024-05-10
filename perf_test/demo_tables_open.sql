CREATE TABLE cities_open
(
    id         UUID      NOT NULL
        PRIMARY KEY,
    name       VARCHAR   NOT NULL,
    kladr_id   VARCHAR   NOT NULL,
    priority   INTEGER,
    created_at TIMESTAMP NOT NULL,
    updated_at TIMESTAMP NOT NULL,
    timezone   VARCHAR
);

CREATE TABLE city2region_open
(
    id         UUID      NOT NULL
        PRIMARY KEY,
    region VARCHAR NOT NULL
);

CREATE INDEX cities_name_idx ON cities_open (name);
