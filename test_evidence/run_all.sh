#!/bin/bash
# Runs every scenario headless in Wokwi and stores serial logs + pass/fail.
# Requires: WOKWI_CLI_TOKEN in the environment, firmware built in ../src/build/Debug.
set -u
cd "$(dirname "$0")"
CLI="${WOKWI_CLI:-$HOME/bin/wokwi-cli}"
[ -n "${WOKWI_CLI_TOKEN:-}" ] || { echo "Set WOKWI_CLI_TOKEN first"; exit 2; }
mkdir -p logs timing screenshots
cp ../project_files/diagram.json sim/diagram.json   # keep sim copy in sync (+ logic analyzer, see sim/)
python3 sim/add_analyzer.py sim/diagram.json
: > logs/results.txt
for s in scenarios/*.scenario.yaml; do
  id=$(basename "$s" .scenario.yaml)
  "$CLI" --quiet --timeout 40000 --scenario "$PWD/$s" \
    --serial-log-file "logs/$id.log" --vcd-file "timing/$id.vcd" sim > "logs/$id.cli.txt" 2>&1
  rc=$?
  [ $rc -eq 0 ] && r=PASS || r="FAIL(rc=$rc)"
  echo "$id $r" | tee -a logs/results.txt
done
