CREATE TABLE cities
(
    id           BYTEA     NOT NULL
        PRIMARY KEY,
    id_bi        BYTEA     NOT NULL,
    id_joinkey   BYTEA     NOT NULL,
    name         BYTEA     NOT NULL,
    name_bi      BYTEA     NOT NULL,
    kladr_id     BYTEA     NOT NULL,
    priority     BYTEA,
    created_at   BYTEA     NOT NULL,
    updated_at   BYTEA     NOT NULL,
    timezone     BYTEA
);

CREATE TABLE city2region
(
    id           BYTEA     NOT NULL
        PRIMARY KEY,
    id_joinkey   BYTEA     NOT NULL,
    region       BYTEA     NOT NULL
);

CREATE UNIQUE INDEX cities_id_bi_idx ON cities (id_bi);
CREATE INDEX cities_id_joinkey_idx ON cities (id_joinkey);
CREATE UNIQUE INDEX cities_name_bi_idx ON cities (name_bi);

CREATE INDEX city2region_id_joinkey_idx ON city2region (id_joinkey);
