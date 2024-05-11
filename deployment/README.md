# Postgres TDE deployment examples

---

## Standalone Envoy configuration

`conf.yaml` defines simple Envoy L4 filter chain involving Postgres TDE - it assumes that Postgres is available at `localhost:5432` and listens DB connections on port 5433

---

## Istio

### Preparation

Because binary filters must be compiled inside Envoy, there is the custom `istio-proxy` image with Postgres TDE integrated: `loochek/proxyv2:1.21.1`.

To get it involved, some extra configuration must be provided during Istio installation:

`istioctl install --set profile=demo --set values.global.proxy.image=docker.io/loochek/proxyv2:1.21.1 --set values.global.proxy_init.image=docker.io/loochek/proxyv2:1.21.1 -y`

### Demo cluster

`istio` folder contains complete cluster configuration for demonstration purposes - it can be applied with `kubectl apply -f ./istio`.
Don't forget to enable sidecar autoinjecton: `kubectl label namespace default istio-injection=enabled` 

`postgres-tde.yaml` defines an EnvoyFilter object that puts Postgres TDE filter into each pod specified by the selector. It accessible on port 5433 and proxies queries to the `postgres` service on port 5432

You can interact with Postgres TDE on the `test` pod: `kubectl exec -it test -- /bin/bash`

---

### What's next?

You can run [test suite](../test) or [performance benchmark](../benchmark), or make some queries by hand using `psql`

> This project is WIP prototype, so
> - TDE configuration is [hard-coded](../postgres_tde/source/filters/network/postgres_tde/config/dummy_config.h) for [these](../demo_tables_open.sql) tables and cannot be altered without Envoy recompilation
> - DDL commands are not implemented - [definitions](../demo_tables.sql) for the encrypted tables must be applied manually
