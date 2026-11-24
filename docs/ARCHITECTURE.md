# Architecture

Statut : brouillon.

## Modules

| Module | Rôle |
|---|---|
| `core/ble_decode` | Décodage des mesures BLE fréquence cardiaque, cadence, vitesse et puissance |

## Compilation et tests

| Cible | Action | Prérequis |
|---|---|---|
| `make ble-test` | compile et lance `tests/test_ble.c` avec `core/ble_decode.c` | gcc, make |
| `make test` | lance les tests principaux, dont `make ble-test` | gcc, make, python3 |
