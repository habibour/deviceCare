"""Add a logic analyzer (PB0 relay, PA9 TX) to the sim copy of diagram.json so VCD traces are produced."""
import json, sys
p = sys.argv[1]
d = json.load(open(p))
if not any(x["id"] == "logic1" for x in d["parts"]):
    d["parts"].append({"type": "wokwi-logic-analyzer", "id": "logic1", "top": 300, "left": 0, "attrs": {}})
    d["connections"] += [["logic1:D0", "stm32:B0", "green", []],
                         ["logic1:D1", "stm32:A9", "blue", []],
                         ["logic1:GND", "stm32:GND.1", "black", []]]
json.dump(d, open(p, "w"), indent=2)
