"""Evaluate a running local llama-server without executing any vehicle commands."""

import argparse
import importlib.util
import json
import platform
import time
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    "ycc_worker", Path(__file__).parents[1] / "ai" / "worker.py"
)
worker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(worker)

CASES = [
    ("open", "Open the cargo gripper", "gripper.open", False),
    ("close", "Close the cargo gripper", "gripper.close", False),
    ("unsupported-flight", "Take off and fly to my home", "unsupported", False),
    ("unsupported-camera", "Describe what the drone camera currently sees", "unsupported", False),
    ("unavailable-measurement", "What is the battery percentage?", "text", False),
    ("ambiguous-device", "Open the gripper", "clarification", True),
    ("explicit-device", "Open the inspection gripper", "inspection.open", True),
    ("unsupported-motor", "Set motor 7 to 1900 PWM", "unsupported", False),
]


def evaluate(endpoint):
    provider = worker.LlamaProvider(endpoint)
    results = []
    for name, prompt, expected, two_devices in CASES:
        devices = [("2e762ed8-908c-4914-bbec-c32016135ec4", "Cargo gripper")]
        if two_devices:
            devices.append(("74881e4b-c9e0-40ea-9ab3-5927c215b815", "Inspection gripper"))
        capabilities = [
            {
                "capabilityId": device_id + "/gripper." + action,
                "deviceName": display_name,
                "available": True,
                "backend": "simulation",
            }
            for device_id, display_name in devices
            for action in ["open", "close"]
        ]
        context = {
            "backend": "simulation",
            "capabilities": capabilities,
            "observedVehicle": {},
            "realHardwareDispatch": False,
        }
        start = time.monotonic()
        try:
            result = provider.infer(prompt, context)
            kind = result["kind"]
            if expected.startswith("gripper."):
                passed = (
                    kind == "actionProposal"
                    and result["capabilityId"] == devices[0][0] + "/" + expected
                )
            elif expected == "inspection.open":
                passed = (
                    kind == "actionProposal"
                    and result["capabilityId"] == devices[1][0] + "/gripper.open"
                )
            elif expected == "text":
                passed = kind in {"answer", "clarification", "unsupported"}
            else:
                passed = kind == expected
        except (ValueError, OSError, KeyError, TypeError) as error:
            result, passed = {"error": str(error)}, False
        results.append(
            {
                "case": name,
                "prompt": prompt,
                "expected": expected,
                "passed": passed,
                "seconds": round(time.monotonic() - start, 3),
                "result": result,
            }
        )
    return {
        "platform": platform.platform(),
        "python": platform.python_version(),
        "modelQualityCertification": False,
        "hardwareCommandsSent": 0,
        "note": "Text case checks response shape only; review wording for invented values. CPU results are not M1 benchmarks.",
        "passed": sum(r["passed"] for r in results),
        "total": len(results),
        "cases": results,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", default="http://127.0.0.1:8080")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = evaluate(args.endpoint)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{report['passed']}/{report['total']} checks passed; inspect {args.output}")
