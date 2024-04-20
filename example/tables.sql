CREATE TABLE cities
(
    id           UUID      NOT NULL
        PRIMARY KEY,
    name         BYTEA     NOT NULL,
    name_bi      BYTEA     NOT NULL,
    name_joinkey BYTEA     NOT NULL,
    region       BYTEA     NOT NULL,
    kladr_id     BYTEA     NOT NULL,
    priority     BYTEA,
    created_at   BYTEA     NOT NULL,
    updated_at   BYTEA     NOT NULL,
    timezone     BYTEA
);

CREATE TABLE name2region
  (
      name         BYTEA     NOT NULL,
      name_joinkey BYTEA     NOT NULL,
      region       BYTEA     NOT NULL
  );
