# État du projet

Statut : brouillon.

Dernière mise à jour : 24 novembre 2026, branche `feat/ble-make-test`.

## Niveaux de preuve

- Codé : le code existe.
- Test présent (non rejoué) : un test existe et est branché dans une cible Make.
- Rejoué : test exécuté avec succès avec date, commit et environnement consignés.
- Validé sur matériel : vérifié sur la carte Waveshare.

## Fonctionnalités

| Fonctionnalité | Module | Niveau actuel | Remarque |
|---|---|---|---|
| Décodage BLE (FC, cadence, puissance) | `core/ble_decode` | Test présent (non rejoué) | `make ble-test` compile et lance `tests/test_ble.c`. Inclus dans `make test`. Aucun test sur matériel. |

## Mise à jour

Un niveau n'est relevé que sur preuve : test rejoué ou essai sur matériel.
