#!/usr/bin/env bash
# Junta en Markdown los hallazgos de SonarCloud y CodeFactor de un PR.
# Uso: analisis-externo.sh <owner/repo> <nro-pr> <sha> <proyecto-sonar> <codefactor: si|no>
# Requiere gh autenticado (GH_TOKEN) y jq. SONAR_TOKEN es opcional (el proyecto es publico).
set -uo pipefail
repo=$1 pr=$2 sha=$3 sonar=$4 codefactor=$5
api=https://sonarcloud.io/api
auth=()
[ -n "${SONAR_TOKEN:-}" ] && auth=(-u "$SONAR_TOKEN:")

id_check() {
    gh api "repos/$repo/commits/$sha/check-runs?per_page=100" \
        --jq ".check_runs[] | select(.app.slug == \"$1\" and .status == \"completed\") | .id" | head -1
}

# Los dos analisis terminan despues de que arranca el job: se espera hasta 10 minutos en total.
limite=$((SECONDS + 600))
esperar() {
    while [ $SECONDS -lt $limite ]; do
        id=$(id_check "$1")
        [ -n "$id" ] && return 0
        sleep 15
    done
    return 1
}

echo "## SonarCloud"
if esperar sonarqubecloud; then
    curl -sSf "${auth[@]}" "$api/qualitygates/project_status?projectKey=$sonar&pullRequest=$pr" |
        jq -r '"Quality Gate: \(.projectStatus.status)",
            (.projectStatus.conditions[]? | select(.status == "ERROR") |
             "- Condición fallida: \(.metricKey) (valor \(.actualValue), umbral \(.errorThreshold))")'
    # shortcut: solo los primeros 100 issues y hotspots; si un PR tiene más, paginar
    curl -sSf "${auth[@]}" "$api/issues/search?componentKeys=$sonar&pullRequest=$pr&issueStatuses=OPEN,CONFIRMED&ps=100" |
        jq -r '"Issues abiertos: \(.total)",
            (.issues[] | "- `\(.component | sub("^[^:]*:"; "")):\(.line // "-")` [\(.impacts[0].severity // .severity)] \(.rule): \(.message)")'
    curl -sSf "${auth[@]}" "$api/hotspots/search?projectKey=$sonar&pullRequest=$pr&ps=100" |
        jq -r '(.hotspots // [])[] | "- Hotspot `\(.component | sub("^[^:]*:"; "")):\(.line // "-")` \(.ruleKey): \(.message)"'
else
    echo "El análisis no terminó a tiempo: no hay datos."
fi

echo
echo "## CodeFactor"
if [ "$codefactor" != si ]; then
    echo "No corre en este PR: CodeFactor solo analiza los PR contra la rama principal."
elif esperar codefactor-io; then
    gh api "repos/$repo/check-runs/$id" --jq '.output.title'
    gh api "repos/$repo/check-runs/$id/annotations?per_page=100" \
        --jq '.[] | "- `\(.path):\(.start_line)` [\(.annotation_level)] \(.title): \(.message)"'
else
    echo "El análisis no terminó a tiempo: no hay datos."
fi
