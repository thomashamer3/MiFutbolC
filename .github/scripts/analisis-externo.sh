#!/usr/bin/env bash
# Junta en Markdown los hallazgos de SonarCloud y CodeFactor de un PR.
# Uso: analisis-externo.sh <owner/repo> <nro-pr> <sha> <proyecto-sonar> <codefactor: si|no>
# Requiere gh autenticado (GH_TOKEN) y jq. SONAR_TOKEN es opcional (el proyecto es publico).
set -uo pipefail
repo=$1 pr=$2 sha=$3 sonar=$4 codefactor=$5
api=https://sonarcloud.io/api

# Con token si hay; si lo rechaza (401), sin token: el proyecto es publico.
sonar_get() {
    local ruta=$1
    if [[ -n "${SONAR_TOKEN:-}" ]] && curl -sSf -H "Authorization: Bearer $SONAR_TOKEN" "$api/$ruta" 2>/dev/null; then
        return 0
    fi
    curl -sSf "$api/$ruta"
}

# Un fallo de la API queda escrito en el informe: una seccion vacia parece "sin issues".
fallo() {
    local que=$1
    echo "- **No se pudo consultar $que**: faltan esos datos."
}

id_check() {
    local app=$1
    gh api "repos/$repo/commits/$sha/check-runs?per_page=100" \
        --jq ".check_runs[] | select(.app.slug == \"$app\" and .status == \"completed\") | .id" | head -1
}

# Los dos analisis terminan despues de que arranca el job: se espera hasta 10 minutos en total.
limite=$((SECONDS + 600))
esperar() {
    local app=$1
    while [[ $SECONDS -lt $limite ]]; do
        id=$(id_check "$app")
        [[ -n "$id" ]] && return 0
        sleep 15
    done
    return 1
}

echo "## SonarCloud"
if esperar sonarqubecloud; then
    if datos=$(sonar_get "qualitygates/project_status?projectKey=$sonar&pullRequest=$pr"); then
        jq -r '"Quality Gate: \(.projectStatus.status)",
            (.projectStatus.conditions[]? | select(.status == "ERROR") |
             "- Condición fallida: \(.metricKey) (valor \(.actualValue), umbral \(.errorThreshold))")' <<<"$datos"
    else
        fallo "el Quality Gate"
    fi
    # shortcut: solo los primeros 100 issues y hotspots; si un PR tiene más, paginar
    if datos=$(sonar_get "issues/search?componentKeys=$sonar&pullRequest=$pr&issueStatuses=OPEN,CONFIRMED&ps=100"); then
        jq -r '"Issues abiertos: \(.total)",
            (.issues[] | "- `\(.component | sub("^[^:]*:"; "")):\(.line // "-")` [\(.impacts[0].severity // .severity)] \(.rule): \(.message)")' <<<"$datos"
    else
        fallo "los issues"
    fi
    if datos=$(sonar_get "hotspots/search?projectKey=$sonar&pullRequest=$pr&ps=100"); then
        jq -r '(.hotspots // [])[] | "- Hotspot `\(.component | sub("^[^:]*:"; "")):\(.line // "-")` \(.ruleKey): \(.message)"' <<<"$datos"
    else
        fallo "los hotspots"
    fi
else
    echo "El análisis no terminó a tiempo: no hay datos."
fi

echo
echo "## CodeFactor"
if [[ "$codefactor" != si ]]; then
    echo "No corre en este PR: CodeFactor solo analiza los PR contra la rama principal."
elif esperar codefactor-io; then
    gh api "repos/$repo/check-runs/$id" --jq '.output.title' || fallo "el resumen de CodeFactor"
    gh api "repos/$repo/check-runs/$id/annotations?per_page=100" \
        --jq '.[] | "- `\(.path):\(.start_line)` [\(.annotation_level)] \(.title): \(.message)"' ||
        fallo "las anotaciones de CodeFactor"
else
    echo "El análisis no terminó a tiempo: no hay datos."
fi
