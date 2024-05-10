#!/bin/bash

ENCRYPTED_HOST=localhost
HOST=localhost
OPTS=sslmode=disable
export PGPASSWORD=postgres

prepare() {
  psql -h $HOST -p 5432 -U postgres -f cleanup.sql $OPTS &> /dev/null
  psql -h $HOST -p 5432 -U postgres -f demo_tables_open.sql $OPTS > /dev/null
  psql -h $HOST -p 5432 -U postgres -f demo_tables.sql $OPTS > /dev/null
}

prepare

echo 'Metric 0: unencrypted insert through the proxy'
time psql -h $ENCRYPTED_HOST -p 5433 -U postgres -f demo_inserts_open.sql $OPTS > /dev/null

prepare

echo 'Metric 1: unencrypted insert'
time psql -h $HOST -p 5432 -U postgres -f demo_inserts_open.sql $OPTS > /dev/null

echo 'Metric 2: encrypted insert'
time psql -h $ENCRYPTED_HOST -p 5433 -U postgres -f demo_inserts.sql $OPTS > /dev/null

echo 'Metric 3: unencrypted select'
time psql -h $HOST -p 5432 -U postgres -f select_test_open.sql $OPTS > /dev/null

echo 'Metric 4: encrypted select'
time psql -h $ENCRYPTED_HOST -p 5433 -U postgres -f select_test.sql $OPTS > /dev/null

echo 'Metric 5: unencrypted join'
time psql -h $HOST -p 5432 -U postgres -f join_test_open.sql $OPTS > /dev/null

echo 'Metric 6: encrypted join'
time psql -h $ENCRYPTED_HOST -p 5433 -U postgres -f join_test.sql $OPTS > /dev/null
