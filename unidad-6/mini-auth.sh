
#!/usr/bin/env bash

DIR="$(cd "$(dirname "$0")" && pwd)/.mini-auth"
DB="$DIR/users.db"
KEY="$DIR/jwt-secret.hex"
TTL=30
HEADER='{"alg":"HS256","typ":"JWT"}'

fail() { echo "Error: $*" >&2; exit 1; }
sha256() { printf '%s' "$1" | openssl dgst -sha256 | cut -d ' ' -f 2; }
b64() { openssl base64 -A | tr '+/' '-_' | tr -d '='; }
decode() {
  local s="${1//-/+}"
  s="${s//_/\/}"
  case $((${#s} % 4)) in 2) s+='==';; 3) s+='=';; 1) return 1;; esac
  printf '%s' "$s" | openssl base64 -d -A
}
sign() { printf '%s' "$1" | openssl dgst -sha256 -hmac "$(cat "$KEY")" -binary | b64; }
get_hash() { awk -F: -v u="$1" '$1==u {print $2; exit}' "$DB"; }

mkdir -p "$DIR"
chmod 700 "$DIR"
touch "$DB"
[[ -f "$KEY" ]] || openssl rand -hex 32 > "$KEY" # creamos el secret si no existe en un archivo
chmod 600 "$DB" "$KEY"

case "${1:-}" in
  register)
    read -r -p 'User: ' user
    [[ "$user" =~ ^[a-zA-Z][a-zA-Z0-9_.-]*$ ]] || fail 'Invalid User'
    [[ -z "$(get_hash "$user")" ]] || fail 'User already exists'
    read -r -s -p 'Password: ' pass; echo
    printf '%s:%s\n' "$user" "$(sha256 "$pass")" >> "$DB"
    echo 'User created.'
    ;;
  login)
    read -r -p 'User: ' user
    read -r -s -p 'Password: ' pass; echo
    hash="$(get_hash "$user")"
    [[ -n "$hash" && "$hash" == "$(sha256 "$pass")" ]] || fail 'Invalid Credentials'
    now="$(date +%s)"
    header="$(printf '%s' "$HEADER" | b64)"
    payload="$(printf '{"sub":"%s","iat":%s,"exp":%s}' "$user" "$now" "$((now + TTL))" | b64)"
    data="$header.$payload"
    printf '%s.%s' "$data" "$(sign "$data")"
    ;;
  profile)
    token="${2:-}"
    [[ "$token" =~ ^[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+$ ]] || fail 'JWT mal formed'
    IFS=. read -r header payload signature <<< "$token"
    [[ "$header" == "$(printf '%s' "$HEADER" | b64)" ]] || fail 'Invalid Header'
    [[ "$signature" == "$(sign "$header.$payload")" ]] || fail 'Invalid Signature'
    json="$(decode "$payload")" || fail 'Invalid Payload'
    [[ "$json" =~ ^\{\"sub\":\"([a-zA-Z][a-zA-Z0-9_.-]*)\",\"iat\":([0-9]+),\"exp\":([0-9]+)\}$ ]] || fail 'Invalid Payload'
    user="${BASH_REMATCH[1]}"
    exp="${BASH_REMATCH[3]}"
    (( $(date +%s) < exp )) || fail 'Expired JWT'
    [[ -n "$(get_hash "$user")" ]] || fail 'User Not Found'
    printf '{"username":"%s","status":"active"}\n' "$user"
    ;;
  *) echo "Commands: $0 {register|login|profile <JWT>}" ;;
esac
