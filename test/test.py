import psycopg2
import pytest
from datetime import datetime

HOST = "localhost"
ENCRYPTED_HOST = "localhost"

@pytest.fixture
def cursor():
    conn = psycopg2.connect(dbname="postgres", host=HOST, user="postgres", password="postgres", port="5432")
    conn.autocommit = True

    with conn.cursor() as cursor:
        yield cursor

    conn.close()

@pytest.fixture
def enc_cursor():
    enc_conn = psycopg2.connect(dbname="postgres", host=ENCRYPTED_HOST, user="postgres", password="postgres", port="5433")
    enc_conn.autocommit = True

    with enc_conn.cursor() as cursor:
        yield cursor

    enc_conn.close()

@pytest.fixture
def prepare_schema(cursor):
    try:
        cursor.execute(open('cleanup.sql', 'r').read())
    except psycopg2.errors.UndefinedTable:
        pass

    cursor.execute(open('demo_tables.sql', 'r').read())


# Ensure that TDE doesn't alter the data
def test_data_integrity(prepare_schema, enc_cursor):
    enc_cursor.execute("INSERT INTO cities (id, name, kladr_id, priority, created_at, updated_at, timezone) VALUES ('08a3f421-cf10-4dc9-855a-7b7e8565f2b1', 'Test city 1',                                                                      '1900000400000', 1,    '2023-11-02 10:30:02.490527', '2023-12-20 00:00:52.932486', '+0700');")
    enc_cursor.execute("INSERT INTO cities (id, name, kladr_id, priority, created_at, updated_at, timezone) VALUES ('c07b21de-c660-46b0-bffd-b1e6272141a9', 'Test city second',                                                                 '1900000100000', null, '2023-11-02 10:30:02.490527', '2023-12-20 00:00:52.932486', null);")
    enc_cursor.execute("INSERT INTO cities (id, name, kladr_id, priority, created_at, updated_at, timezone) VALUES ('33008eec-464e-4022-a6c4-90c7cc70612e', 'Test city No 3',                                                                   '5605500100000', 2,    '2023-11-02 10:30:02.490527', '2023-12-20 00:00:52.932486', null);")
    enc_cursor.execute("INSERT INTO cities (id, name, kladr_id, priority, created_at, updated_at, timezone) VALUES ('74608ce8-68cb-4299-a556-d7a1556a72e2', '??',                                                                               '2300200100000', null, '2023-11-02 10:30:02.490527', '2023-12-20 00:00:52.932486', null);")
    enc_cursor.execute("INSERT INTO cities (id, name, kladr_id, priority, created_at, updated_at, timezone) VALUES ('89c1e189-3cc0-4cd6-b4db-3b556f945344', 'Test city which name is quite long aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '0200000200000', 5,    '2023-11-02 10:30:02.490527', '2023-12-20 00:00:52.932486', null);")

    enc_cursor.execute("SELECT c.id, c.name, c.kladr_id, c.priority, c.created_at, c.updated_at, c.timezone FROM cities c")

    assert sorted(enc_cursor.fetchall()) == [
        ('08a3f421-cf10-4dc9-855a-7b7e8565f2b1', 'Test city 1',                                                                      '1900000400000', 1,    datetime(2023, 11, 2, 10, 30, 2, 490527), datetime(2023, 12, 20, 0, 0, 52, 932486), '+0700'),
        ('33008eec-464e-4022-a6c4-90c7cc70612e', 'Test city No 3',                                                                   '5605500100000', 2,    datetime(2023, 11, 2, 10, 30, 2, 490527), datetime(2023, 12, 20, 0, 0, 52, 932486), None),
        ('74608ce8-68cb-4299-a556-d7a1556a72e2', '??',                                                                               '2300200100000', None, datetime(2023, 11, 2, 10, 30, 2, 490527), datetime(2023, 12, 20, 0, 0, 52, 932486), None),
        ('89c1e189-3cc0-4cd6-b4db-3b556f945344', 'Test city which name is quite long aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', '0200000200000', 5,    datetime(2023, 11, 2, 10, 30, 2, 490527), datetime(2023, 12, 20, 0, 0, 52, 932486), None),
        ('c07b21de-c660-46b0-bffd-b1e6272141a9', 'Test city second',                                                                 '1900000100000', None, datetime(2023, 11, 2, 10, 30, 2, 490527), datetime(2023, 12, 20, 0, 0, 52, 932486), None),
    ]


def test_update(prepare_schema, enc_cursor):
    # Blind index and join key must be updated by UPDATE 
    enc_cursor.execute("INSERT INTO cities (id, name, kladr_id, priority, created_at, updated_at, timezone) VALUES ('08a3f421-cf10-4dc9-855a-7b7e8565f2b1', 'Test city 1', '1900000400000', 1, '2023-11-02 10:30:02.490527', '2023-12-20 00:00:52.932486', '+0700');")
    enc_cursor.execute("INSERT INTO city2region (id, region) VALUES ('33008eec-464e-4022-a6c4-90c7cc70612e', 'Region 1');")

    enc_cursor.execute("UPDATE cities SET id = '33008eec-464e-4022-a6c4-90c7cc70612e' WHERE cities.id = '08a3f421-cf10-4dc9-855a-7b7e8565f2b1';")

    enc_cursor.execute("SELECT c.id, c.name FROM cities c WHERE c.id = '33008eec-464e-4022-a6c4-90c7cc70612e';")
    assert sorted(enc_cursor.fetchall()) == [('33008eec-464e-4022-a6c4-90c7cc70612e', 'Test city 1')]

    enc_cursor.execute("SELECT c.id AS c_id, c2r.id AS c2r_id, c.name, c2r.region FROM cities c JOIN city2region c2r ON c.id = c2r.id")
    assert sorted(enc_cursor.fetchall()) == [('33008eec-464e-4022-a6c4-90c7cc70612e', '33008eec-464e-4022-a6c4-90c7cc70612e', 'Test city 1', 'Region 1')]


def test_blind_index_correctness(prepare_schema, enc_cursor):
    enc_cursor.execute("INSERT INTO cities (id, name, kladr_id, priority, created_at, updated_at, timezone) VALUES ('08a3f421-cf10-4dc9-855a-7b7e8565f2b1', 'City 1', '1', null, '2023-11-02 10:30:02.490527', '2023-12-20 00:00:52.932486', null);")
    enc_cursor.execute("INSERT INTO cities (id, name, kladr_id, priority, created_at, updated_at, timezone) VALUES ('33008eec-464e-4022-a6c4-90c7cc70612e', 'City 2', '1', null, '2023-11-02 10:30:02.490527', '2023-12-20 00:00:52.932486', null);")

    enc_cursor.execute("SELECT c.id, c.name FROM cities c WHERE c.name = 'City 1';")
    assert sorted(enc_cursor.fetchall()) == [('08a3f421-cf10-4dc9-855a-7b7e8565f2b1', 'City 1')]
    
    enc_cursor.execute("SELECT c.id, c.name FROM cities c WHERE c.name = 'City 2';")
    assert sorted(enc_cursor.fetchall()) == [('33008eec-464e-4022-a6c4-90c7cc70612e', 'City 2')]


def test_join_correctness(prepare_schema, enc_cursor):
    # Rows ID correspond to the similar join key (first 2 bytes of SHA256), so encrypted join requires skipping some rows at the proxy level
    enc_cursor.execute("INSERT INTO cities (id, name, kladr_id, priority, created_at, updated_at, timezone) VALUES ('1e63b6ff-4fe5-4498-90d1-d84693a84db8', 'City 1', '1', null, '2023-11-02 10:30:02.490527', '2023-12-20 00:00:52.932486', null);")
    enc_cursor.execute("INSERT INTO cities (id, name, kladr_id, priority, created_at, updated_at, timezone) VALUES ('4df0dc1a-2d9d-4682-848b-c323e922c60f', 'City 2', '1', null, '2023-11-02 10:30:02.490527', '2023-12-20 00:00:52.932486', null);")

    enc_cursor.execute("INSERT INTO city2region (id, region) VALUES ('1e63b6ff-4fe5-4498-90d1-d84693a84db8', 'Region 1');")
    enc_cursor.execute("INSERT INTO city2region (id, region) VALUES ('4df0dc1a-2d9d-4682-848b-c323e922c60f', 'Region 2');")

    enc_cursor.execute("SELECT c.id AS c_id, c2r.id AS c2r_id, c.name, c2r.region FROM cities c JOIN city2region c2r ON c.id = c2r.id WHERE c.id = '1e63b6ff-4fe5-4498-90d1-d84693a84db8';")
    assert sorted(enc_cursor.fetchall()) == [('1e63b6ff-4fe5-4498-90d1-d84693a84db8', '1e63b6ff-4fe5-4498-90d1-d84693a84db8', 'City 1', 'Region 1')]

    enc_cursor.execute("SELECT c.id AS c_id, c2r.id AS c2r_id, c.name, c2r.region FROM cities c JOIN city2region c2r ON c.id = c2r.id WHERE c.id = '4df0dc1a-2d9d-4682-848b-c323e922c60f';")
    assert sorted(enc_cursor.fetchall()) == [('4df0dc1a-2d9d-4682-848b-c323e922c60f', '4df0dc1a-2d9d-4682-848b-c323e922c60f', 'City 2', 'Region 2')]


# Ensure that encrypted indexing is allowed only for indexed columns
def test_blind_index_requirements(prepare_schema, enc_cursor):
    with pytest.raises(psycopg2.DatabaseError) as excinfo:
        enc_cursor.execute("SELECT c.id, c.name FROM cities c WHERE c.kladr_id = '1900000400000';")

    assert str(excinfo.value) == "postgres_tde: invalid use of encrypted column cities.kladr_id\n"


# Ensure that encrypted columns is not used in expressions
def test_blind_index_expressions(prepare_schema, enc_cursor):
    with pytest.raises(psycopg2.DatabaseError) as excinfo:
        enc_cursor.execute("SELECT c.id FROM cities c WHERE c.id LIKE 'aboba'")

    assert str(excinfo.value) == "postgres_tde: invalid use of encrypted column cities.id\n"


# Ensure that encrypted join is allowed only for columns that are marked as joinable
def test_join_requirements(prepare_schema, enc_cursor):
    with pytest.raises(psycopg2.DatabaseError) as excinfo:
        enc_cursor.execute("SELECT c.id, c.name FROM cities c JOIN city2region c2r ON c.id = c2r.region;")

    assert str(excinfo.value) == "postgres_tde: invalid use of encrypted column cities.id\n"


def test_star_prohibition(prepare_schema, enc_cursor):
    with pytest.raises(psycopg2.DatabaseError) as excinfo:
        enc_cursor.execute("SELECT * FROM cities;")

    assert str(excinfo.value) == "postgres_tde: star expression is not supported\n"


def test_unannotated_insert_prohibition(prepare_schema, enc_cursor):
    with pytest.raises(psycopg2.DatabaseError) as excinfo:
        enc_cursor.execute("INSERT INTO cities VALUES ('1e63b6ff-4fe5-4498-90d1-d84693a84db8', 'City 1', '1', null, '2023-11-02 10:30:02.490527', '2023-12-20 00:00:52.932486', null);")

    assert str(excinfo.value) == "postgres_tde: columns definition for INSERT is required for TDE-enabled tables\n"


# Ensure annotations requirement for SELECT
def test_select_annotation_requirement(prepare_schema, enc_cursor):
    with pytest.raises(psycopg2.DatabaseError) as excinfo:
        enc_cursor.execute("SELECT id, name FROM cities WHERE kladr_id = '1900000400000';")

    assert str(excinfo.value) == "postgres_tde: unable to determine the source of column id. Please specify an explicit table/alias reference\n"


def test_join_columns_requirement(prepare_schema, enc_cursor):
    with pytest.raises(psycopg2.DatabaseError) as excinfo:
        enc_cursor.execute("SELECT c.name, c2r.region FROM cities c JOIN city2region c2r ON c.id = c2r.id")

    assert str(excinfo.value) == "postgres_tde: columns present in join condition must be also present in SELECT body\n"


def test_join_ambiguous_columns(prepare_schema, enc_cursor):
    with pytest.raises(psycopg2.DatabaseError) as excinfo:
        enc_cursor.execute("SELECT c.id, c2r.id FROM cities c JOIN city2region c2r ON c.id = c2r.id")

    assert str(excinfo.value) == "postgres_tde: detected ambiguous column name id. Please specify a different alias for each column\n"


def test_update_literal_requirement(prepare_schema, enc_cursor):
    with pytest.raises(psycopg2.DatabaseError) as excinfo:
        enc_cursor.execute("UPDATE cities SET priority = 1 + 1 WHERE cities.id = '08a3f421-cf10-4dc9-855a-7b7e8565f2b1';")

    assert str(excinfo.value) == "postgres_tde: only literals can be used as UPDATE values for blind-indexed columns\n"
