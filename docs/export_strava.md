# Export Strava

Le fichier FIT produit par `core/fit` peut aller sur Strava de deux façons, avec le même fichier :
- **À la main** : dépôt du fichier sur le site ou dans l'appli Strava. Aucun développement, pas d'API.
- **Par l'API** : `tools/strava_upload.py`. **Un abonnement Strava est requis** pour un développeur de niveau Standard (voir ci-dessous).

## Abonnement requis pour l'API

D'après la presse (TechRepublic, Gigazine, The Verge et le forum intervals.icu qui cite le courriel de Strava, juin 2026) : depuis le 1er juin 2026 pour les nouveaux développeurs et le 30 juin 2026 pour les existants, l'accès à l'API au niveau Standard demande un abonnement Strava (annoncé à 11,99 $ par mois). Les intégrations d'appareils et le téléchargement de ses propres données restent gratuits. Source officielle non lue : message du 1er juin 2026 sur communityhub.strava.com, "An update to our developer program". Les pages de documentation developers.strava.com consultées (Uploading, Rate Limits) ne mentionnent pas cette règle.

Conséquence : sans abonnement, rester sur l'envoi manuel. Le script ne sert que si un abonnement est pris, ou si l'abonnement existe déjà (un billet de Terra indique qu'il n'y a alors pas de frais d'API en plus pour le niveau Standard).

## Utilisation du script

1. Créer une application sur https://www.strava.com/settings/api, avec `localhost` comme domaine de rappel (Authorization Callback Domain).
2. Une seule fois : `python3 tools/strava_upload.py auth` (navigateur, droit `activity:write`).
3. Pour chaque sortie : `python3 tools/strava_upload.py upload sortie.fit --sport GravelRide --name "..."`.

Identifiants et jetons : `~/.config/gps-velo/strava.json`, hors du dépôt, droits 600. Le jeton d'accès dure 6 h et se renouvelle seul ; le nouveau refresh token est enregistré à chaque renouvellement.

## Ce que la doc Strava confirme (developers.strava.com, pages Uploading, Authentication, Rate Limits)

- `POST /api/v3/uploads` en multipart, champs `file` et `data_type` obligatoires (`fit`, `fit.gz`...), `sport_type`, `name`, `description`, `commute`, `trainer`, `external_id` facultatifs. `activity_type` est déprécié.
- Traitement asynchrone : réponse 201 avec un identifiant, puis `GET /api/v3/uploads/:id` au plus une fois par seconde jusqu'à "Your activity is ready." avec l'`activity_id`. Un doublon est signalé dans `error`.
- FIT : fichier Activity ; chaque `record` doit avoir un `timestamp` ; le sport vient de `session.sport` et `sub_sport`.
- Une app neuve est en mode un seul athlète (le propriétaire). Limites par défaut : 200 requêtes/15 min et 2000/jour au global.

## À vérifier au premier envoi réel

- Le domaine de rappel `localhost` accepté par Strava pour l'autorisation (API seulement).
- Le fichier produit par `core/fit` est accepté tel quel (le script a été testé contre un faux serveur, pas contre Strava).
- Altitude et dénivelé : le fabricant écrit est 255 (développement). La doc dit que l'altitude est reprise du fichier pour un appareil à baromètre et recalculée sinon, et que le fabricant doit être enregistré chez ANT+ pour que l'appareil soit reconnu. Comparer le dénivelé affiché par Strava à celui du fichier.
- `--sport` doit l'emporter sur le sport détecté dans le fichier (la doc l'indique).

## Plus tard : envoi direct depuis la carte

Même protocole que le script (TLS, multipart lu depuis la SD, interrogation du statut), mais il faudrait ranger le client secret et le refresh token dans l'appareil, et l'abonnement est requis. À décider une fois le matériel en main.
