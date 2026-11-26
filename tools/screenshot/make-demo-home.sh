#!/bin/sh
# Fills $1 with the made-up home folder shown in the README screenshot.
set -eu
cd "$1"
mkdir -p Desktop Documents/Invoices Downloads Music/Albums Pictures Videos Templates \
    "Photos/2026 Summer" Photos/Family Photos/Trips Recipes \
    Projects/garden-planner Projects/website Projects/recipe-box Projects/budget
file() { head -c "$2" /dev/zero > "$1"; }
printf '# Notes\n\nIdeas for the spring.\n' > Projects/notes.md
printf 'TODO\n' > Projects/todo.txt
file Projects/roadmap.ods 48200
file Projects/logo.svg 12400
file Projects/budget-2026.ods 31800
for f in beach:3400000 harbour:2900000 lighthouse:4100000 market:2600000 picnic:2500000 \
         sunset:3900000 tide-pools:3100000 village:2750000; do
    file "Photos/2026 Summer/${f%%:*}.jpg" "${f##*:}"
done
file "Photos/2026 Summer/boat trip.mp4" 48000000
for m in 01 02 03 04 05 06; do file "Documents/Invoices/invoice-2026-$m.pdf" 81234; done

# Fixed dates, so the picture is the same each time.
n=0
find . -mindepth 1 | sort | while read -r f; do
    n=$((n + 1))
    touch -d "2026-$((6 + n % 5))-$((1 + n * 7 % 27)) $((8 + n % 11)):$((10 + n * 13 % 50))" "$f"
done
