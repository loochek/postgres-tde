# Postgres TDE deployment

---

## Standalone Envoy

`conf.yaml` defines simple Envoy L4 filter chain involving Postgres TDE - it assumes that Postgres is available at `localhost:5432` and listens DB connections on port 5433.

---

## Istio

Cluster-wide configuration is available as well. `postgres-tde.yaml` defines an EnvoyFilter object that puts Postgres TDE filter into each pod specified by the selector. It accessible on port 5433 and proxies queries to the `postgres` service on port 5432

### Preparation

Because binary filters must be compiled inside Envoy, there is the custom `istio-proxy` image with Postgres TDE integrated: `loochek/proxyv2:1.21.1`.

To get it involved, some extra configuration must be provided during Istio installation:

`istioctl install --set profile=demo --set values.global.proxy.image=docker.io/loochek/proxyv2:1.21.1 --set values.global.proxy_init.image=docker.io/loochek/proxyv2:1.21.1 -y`

### Test

You can interact with Postgres TDE on the `test` pod: `kubectl exec -it test -- /bin/bash`

---

### How to test

`psql -h localhost -p 5433 -U postgres`

This directory contains some SQL files with test queries, as well as DDL for the test tables
