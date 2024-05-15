import random
import string
from collections import namedtuple
from uuid import uuid4
from randomtimestamp import randomtimestamp

N = 10000

City = namedtuple('City' , 'id name region kladr_id created_at updated_at')

cities = [
    City(
        uuid4(),
        ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(20)),
        ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(20)),
        ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(20)),
        randomtimestamp(),
        randomtimestamp(),
    ) for _ in range(N)
]

f = open('demo_inserts_open.sql', 'w')
for city in cities:
    f.write(f"INSERT INTO cities_open (id, name, kladr_id, priority, created_at, updated_at, timezone) VALUES ('{city.id}', '{city.name}', '{city.kladr_id}', null, '{city.created_at.isoformat()}', '{city.updated_at.isoformat()}', null);\n")
    f.write(f"INSERT INTO city2region_open (id, region) VALUES ('{city.id}', '{city.region}');\n")
f.close()

f = open('demo_inserts.sql', 'w')
for city in cities:
    f.write(f"INSERT INTO cities (id, name, kladr_id, priority, created_at, updated_at, timezone) VALUES ('{city.id}', '{city.name}', '{city.kladr_id}', null, '{city.created_at.isoformat()}', '{city.updated_at.isoformat()}', null);\n")
    f.write(f"INSERT INTO city2region (id, region) VALUES ('{city.id}', '{city.region}');\n")
f.close()

f = open('select_test_open.sql', 'w')
for city in cities:
    f.write(f"SELECT c.id, c.name, c.kladr_id, c.priority, c.timezone, c.created_at, c.updated_at FROM cities_open c WHERE c.name = '{city.name}';\n")
f.close()

f = open('select_test.sql', 'w')
for city in cities:
    f.write(f"SELECT c.id, c.name, c.kladr_id, c.priority, c.timezone, c.created_at, c.updated_at FROM cities c WHERE c.name = '{city.name}';\n")
f.close()

f = open('join_test_open.sql', 'w')
for city in cities:
    f.write(f"SELECT c.id AS c_id, c2r.id AS c2r_id, c.name, c2r.region, c.kladr_id, c.priority, c.timezone, c.created_at, c.updated_at FROM cities_open c JOIN city2region_open c2r ON c.id = c2r.id WHERE c.name = '{city.name}';\n")
f.close()

f = open('join_test.sql', 'w')
for city in cities:
    f.write(f"SELECT c.id AS c_id, c2r.id AS c2r_id, c.name, c2r.region, c.kladr_id, c.priority, c.timezone, c.created_at, c.updated_at FROM cities c JOIN city2region c2r ON c.id = c2r.id WHERE c.name = '{city.name}';\n")
f.close()
