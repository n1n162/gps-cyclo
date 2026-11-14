#!/usr/bin/env python3
"""Envoie un fichier FIT a Strava (API v3, POST /uploads). Sans dependance, python3 seul.

Premiere fois (autorisation dans le navigateur, une seule fois) :
    python3 strava_upload.py auth
Ensuite, pour chaque sortie :
    python3 strava_upload.py upload sortie.fit [--name "Sortie du dimanche"] [--sport Ride] [--description "..."] [--commute]

Prerequis : creer une application sur https://www.strava.com/settings/api, avec "localhost" comme domaine de
rappel (Authorization Callback Domain). Les identifiants et jetons sont ranges hors du depot, dans
~/.config/gps-velo/strava.json (droits 600). Details : docs/export_strava.md"""
import argparse, getpass, http.server, json, os, sys, time, urllib.error, urllib.parse, urllib.request, uuid, webbrowser

BASE = os.environ.get("STRAVA_BASE", "https://www.strava.com")
CONF = os.path.join(os.environ.get("XDG_CONFIG_HOME", os.path.expanduser("~/.config")), "gps-velo", "strava.json")
PORT = 8765
REDIRECT = "http://localhost:%d/callback" % PORT
SPORTS = ["Ride", "GravelRide", "MountainBikeRide", "EBikeRide", "EMountainBikeRide"]
POLL_S = 2          # la doc demande au plus une interrogation par seconde
TIMEOUT_S = 120


def load_conf():
    try:
        with open(CONF, encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        return {}


def save_conf(c):
    os.makedirs(os.path.dirname(CONF), exist_ok=True)
    fd = os.open(CONF, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        json.dump(c, f, indent=2)
    os.chmod(CONF, 0o600)


def call(req):
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            return r.status, json.loads(r.read().decode("utf-8") or "{}")
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", "replace")
        try:
            return e.code, json.loads(body)
        except ValueError:
            return e.code, {"raw": body}


def token_request(fields):
    data = urllib.parse.urlencode(fields).encode()
    return call(urllib.request.Request(BASE + "/oauth/token", data=data))


def auth():
    c = load_conf()
    cid = input("Client ID [%s] : " % c.get("client_id", "")).strip() or c.get("client_id", "")
    sec = getpass.getpass("Client secret (masque) : ").strip() or c.get("client_secret", "")
    if not cid or not sec:
        raise SystemExit("Client ID et secret necessaires (page https://www.strava.com/settings/api)")
    url = BASE + "/oauth/authorize?" + urllib.parse.urlencode({
        "client_id": cid, "response_type": "code", "redirect_uri": REDIRECT,
        "approval_prompt": "force", "scope": "activity:write"})
    got = {}

    class H(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
            got.update({k: v[0] for k, v in q.items()})
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.end_headers()
            self.wfile.write("Autorisation recue, tu peux fermer cet onglet.".encode("utf-8"))

        def log_message(self, *a):
            pass

    srv = http.server.HTTPServer(("127.0.0.1", PORT), H)
    print("Ouverture du navigateur. Si rien ne s'ouvre, copie ce lien :\n" + url)
    webbrowser.open(url)
    while "code" not in got and "error" not in got:
        srv.handle_request()
    srv.server_close()
    if "error" in got:
        raise SystemExit("Autorisation refusee : " + got["error"])
    if "activity:write" not in got.get("scope", ""):
        raise SystemExit("Le droit 'activity:write' n'a pas ete accorde (scope recu : %s). "
                         "Relance et laisse la case cochee." % got.get("scope", ""))
    st, r = token_request({"client_id": cid, "client_secret": sec, "code": got["code"],
                           "grant_type": "authorization_code"})
    if st != 200 or "refresh_token" not in r:
        raise SystemExit("Echange du code refuse (%s) : %s" % (st, r))
    save_conf({"client_id": cid, "client_secret": sec, "access_token": r["access_token"],
               "refresh_token": r["refresh_token"], "expires_at": r["expires_at"]})
    print("OK, jetons enregistres dans " + CONF)


def access_token():
    c = load_conf()
    if not c.get("refresh_token"):
        raise SystemExit("Pas d'autorisation enregistree : lance d'abord `python3 strava_upload.py auth`")
    if c.get("expires_at", 0) - 60 > time.time():
        return c["access_token"]
    st, r = token_request({"client_id": c["client_id"], "client_secret": c["client_secret"],
                           "grant_type": "refresh_token", "refresh_token": c["refresh_token"]})
    if st != 200 or "access_token" not in r:
        raise SystemExit("Renouvellement du jeton refuse (%s) : %s. Relance `auth`." % (st, r))
    c.update(access_token=r["access_token"], refresh_token=r.get("refresh_token", c["refresh_token"]),
             expires_at=r["expires_at"])
    save_conf(c)           # le refresh token renvoye est conserve a chaque renouvellement
    return c["access_token"]


def multipart(fields, fname, content):
    b = uuid.uuid4().hex
    parts = []
    for k, v in fields.items():
        parts.append(('--%s\r\nContent-Disposition: form-data; name="%s"\r\n\r\n%s\r\n' % (b, k, v)).encode("utf-8"))
    parts.append(('--%s\r\nContent-Disposition: form-data; name="file"; filename="%s"\r\n'
                  'Content-Type: application/octet-stream\r\n\r\n' % (b, fname)).encode("utf-8"))
    parts.append(content)
    parts.append(("\r\n--%s--\r\n" % b).encode())
    return b"".join(parts), "multipart/form-data; boundary=" + b


def upload(a):
    with open(a.fichier, "rb") as f:
        content = f.read()
    gz = a.fichier.lower().endswith(".gz")
    if not gz and (len(content) < 14 or content[8:12] != b".FIT"):
        raise SystemExit("Ce fichier n'a pas l'en-tete FIT (.FIT attendu aux octets 8 a 11)")
    fields = {"data_type": "fit.gz" if gz else "fit", "sport_type": a.sport}
    if a.name: fields["name"] = a.name
    if a.description: fields["description"] = a.description
    if a.commute: fields["commute"] = "1"
    if a.external_id: fields["external_id"] = a.external_id
    fname = os.path.basename(a.fichier).replace('"', "_")
    body, ctype = multipart(fields, fname, content)
    tok = access_token()
    st, r = call(urllib.request.Request(BASE + "/api/v3/uploads", data=body, headers={
        "Authorization": "Bearer " + tok, "Content-Type": ctype}))
    if st != 201:
        hint = {401: " (jeton refuse : relance `auth`)", 429: " (limite de requetes atteinte, reessaie plus tard)"}.get(st, "")
        raise SystemExit("Envoi refuse, code %s%s : %s" % (st, hint, r.get("error") or r.get("message") or r))
    uid = r.get("id_str") or str(r["id"])
    print("Fichier accepte (envoi %s), traitement en cours..." % uid)
    t0 = time.time()
    while time.time() - t0 < TIMEOUT_S:
        time.sleep(POLL_S)
        st, r = call(urllib.request.Request(BASE + "/api/v3/uploads/" + uid,
                                            headers={"Authorization": "Bearer " + tok}))
        if st != 200:
            raise SystemExit("Suivi impossible, code %s : %s" % (st, r))
        if r.get("error"):
            raise SystemExit("Strava a refuse l'activite : %s" % r["error"])
        if r.get("activity_id"):
            print("Activite creee : https://www.strava.com/activities/%s" % r["activity_id"])
            return
        if "deleted" in (r.get("status") or ""):
            raise SystemExit("Activite supprimee cote Strava : %s" % r["status"])
    raise SystemExit("Pas de reponse finale apres %d s, verifie sur Strava (envoi %s)" % (TIMEOUT_S, uid))


def main():
    p = argparse.ArgumentParser(description="Envoi d'un fichier FIT a Strava")
    s = p.add_subparsers(dest="cmd", required=True)
    s.add_parser("auth", help="autorisation initiale dans le navigateur")
    u = s.add_parser("upload", help="envoyer un fichier FIT")
    u.add_argument("fichier")
    u.add_argument("--name"); u.add_argument("--description")
    u.add_argument("--sport", choices=SPORTS, default="Ride")
    u.add_argument("--commute", action="store_true")
    u.add_argument("--external-id", dest="external_id")
    a = p.parse_args()
    auth() if a.cmd == "auth" else upload(a)


main()
