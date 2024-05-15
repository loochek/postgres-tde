CREATE TABLE cities_acra
(
    id           BYTEA     NOT NULL
        PRIMARY KEY,
    name         BYTEA     NOT NULL,
    kladr_id     BYTEA     NOT NULL,
    priority     BYTEA,
    created_at   BYTEA     NOT NULL,
    updated_at   BYTEA     NOT NULL,
    timezone     BYTEA
);

CREATE TABLE city2region_acra
(
    id           BYTEA     NOT NULL
        PRIMARY KEY,
    region BYTEA NOT NULL
);

CREATE UNIQUE INDEX cities_acra_id_bi_idx ON cities_acra (SUBSTR(id, 1, 33));
CREATE UNIQUE INDEX cities_acra_name_bi_idx ON cities_acra (SUBSTR(name, 1, 33));
CREATE UNIQUE INDEX city2region_acra_id_bi_idx ON city2region_acra (SUBSTR(id, 1, 33));
