#!/usr/bin/env bash
# Publica la revision de un PR o actualiza la anterior, buscandola por su titulo.
# Uso: publicar-revision.sh <nro-pr> <titulo> <revision.md> <analisis.md>
# Requiere gh autenticado (GH_TOKEN) y GITHUB_REPOSITORY.
set -euo pipefail
pr=$1 titulo=$2 revision=$3 analisis=$4

{
    echo "$titulo"
    echo
    cat "$revision"
    echo
    echo "<details><summary>Datos de SonarCloud y CodeFactor</summary>"
    echo
    cat "$analisis" 2>/dev/null || echo "No hay datos."
    echo
    echo "</details>"
} > comentario.md

# Por titulo y no con --edit-last: todos los jobs comentan como github-actions[bot].
id=$(gh api "repos/$GITHUB_REPOSITORY/issues/$pr/comments" --paginate \
    --jq ".[] | select(.user.login == \"github-actions[bot]\" and (.body | startswith(\"$titulo\"))) | .id" | head -1)
if [[ -n "$id" ]]; then
    gh api -X PATCH "repos/$GITHUB_REPOSITORY/issues/comments/$id" -F body=@comentario.md > /dev/null
else
    gh pr comment "$pr" --body-file comentario.md
fi
