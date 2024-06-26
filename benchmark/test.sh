#!/bin/zsh

# Total time in seconds
TIMEFMT=%*E

ENCRYPTED_HOST=localhost
HOST=localhost
OPTS=sslmode=disable
export PGPASSWORD=postgres

prepare() {
  psql -h $HOST -p 5432 -U postgres -f cleanup.sql $OPTS &> /dev/null
  psql -h $HOST -p 5432 -U postgres -f ../demo_tables_open.sql $OPTS > /dev/null
  psql -h $HOST -p 5432 -U postgres -f ../demo_tables.sql $OPTS > /dev/null
}

reset() {
  prepare
  psql -h $HOST -p 5432 -U postgres -f demo_inserts_open.sql $OPTS > /dev/null
  psql -h $ENCRYPTED_HOST -p 5433 -U postgres -f demo_inserts.sql $OPTS > /dev/null
}


prepare

echo 'Metric 1: unencrypted insert'
time psql -h $HOST -p 5432 -U postgres -f demo_inserts_open.sql $OPTS > /dev/null

prepare

echo 'Metric 2: unencrypted insert through the proxy'
time psql -h $ENCRYPTED_HOST -p 5433 -U postgres -f demo_inserts_open.sql $OPTS > /dev/null

prepare

echo 'Metric 3: encrypted insert'
time psql -h $ENCRYPTED_HOST -p 5433 -U postgres -f demo_inserts.sql $OPTS > /dev/null

reset

echo 'Metric 4: unencrypted select'
time psql -h $HOST -p 5432 -U postgres -f select_test_open.sql $OPTS > /dev/null

reset

echo 'Metric 5: unencrypted select through the proxy'
time psql -h $ENCRYPTED_HOST -p 5433 -U postgres -f select_test_open.sql $OPTS > /dev/null

reset

echo 'Metric 6: encrypted select'
time psql -h $ENCRYPTED_HOST -p 5433 -U postgres -f select_test.sql $OPTS > /dev/null

reset

echo 'Metric 7: unencrypted join'
time psql -h $HOST -p 5432 -U postgres -f join_test_open.sql $OPTS > /dev/null

reset

echo 'Metric 8: unencrypted join through the proxy'
time psql -h $ENCRYPTED_HOST -p 5433 -U postgres -f join_test_open.sql $OPTS > /dev/null

reset

echo 'Metric 9: encrypted join'
time psql -h $ENCRYPTED_HOST -p 5433 -U postgres -f join_test.sql $OPTS > /dev/null
