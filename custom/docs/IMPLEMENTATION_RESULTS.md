# Implementation results — 18 October 2026

Base repository: `yousifhack/drone-control-center`, branch `yousif/control-center`, commit `4a4f0fa6bd05eb2d3afd8363978390de7c4c94d5`.

Delivery branch: `codex/ai-foundation` (local). GitHub's connected integration returned HTTP 403, `Resource not accessible by integration`, when asked to create this branch. No remote branch, commit or pull request was published. A patch and Git branch bundle are supplied instead. Public visibility did not grant the integration write permission.

## Verified results

| Check | Result |
|---|---|
| Qt/C++ standalone foundation build | Passed on Linux x86-64, GCC 13, Qt 6.11.1 |
| QML singleton registration and state delivery | Passed in a real QQmlEngine |
| Core/protocol C++ tests | 17 test cases passed; Qt reports 19 including initialization/cleanup |
| Python provider tests | 7 passed |
| Simulated device end-to-end | Proposal → approval → Dispatched → Accepted → Completed, position 0.9, explicitly simulated |
| Real model through C++ → Python → llama.cpp → C++ | “Open the cargo gripper” returned a valid session-bound proposal; no automatic dispatch |
| C++ formatting | clang-format 22.1.5, repository style |
| Python lint/format | ruff 0.15.20 |
| QGC vehicle null analyzer | Passed for the new read bridge |

Core cases exercise malformed/duplicate JSON, revision conflicts, stale AI context, stale approvals, ownership contention, request deduplication, missing ACK, missing feedback, device rejection, crash recovery, exclusive journal access, worker crash, oversized worker output and forged approval fields.

These checks use the new production core classes. The hostile-worker fixture is explicitly test-only and is not used as evidence of real model inference.

## Actual local-model measurements

Both models were run through a locally built llama.cpp server at commit `5aeac37a35b44fde4bdffaec9159f2fcb253b622`, CPU-only, two threads, context 4096, one slot. The runtime and model weights are external dependencies, not committed files.

| Candidate | Download size | Development evaluation | Median request time |
|---|---|---|---|
| Qwen2.5 0.5B Instruct Q4_K_M | 491,400,032 bytes | 4/8 checks | 2.19 seconds |
| Qwen2.5 1.5B Instruct Q4_K_M | 1,117,320,736 bytes | 6/8 checks | 2.34 seconds |

These are eight development checks, **not** a held-out accuracy estimate, robustness certification, M1 benchmark or flight-readiness result. One text-response check only checks shape; its wording was also inspected. Cold end-to-end 0.5B inference with the fuller core snapshot took about 6.98 seconds.

The 0.5B model proposed a gripper action for unsupported flight/motor requests and hallucinated a camera description. The 1.5B model correctly handled those cases but still selected a gripper when the request was ambiguous, and incorrectly rejected an explicitly named second gripper. Neither candidate passes the blueprint's operational AI acceptance gate. The application therefore exposes proposals for review and labels generated prose as unverified. No model is authorized for autonomous or physical dispatch.

Full prompts, outputs, durations, model repository revisions and SHA-256 hashes are in:

- `evidence/qwen-0.5b-evaluation.json`
- `evidence/qwen-1.5b-evaluation.json`

The candidate model downloader pins the tested 1.5B artifact and verifies its checksum. It does not convert a failed evaluation into a passed one.

## Open acceptance gates

| Gate | Actual status |
|---|---|
| Full QGroundControl application build | Not run successfully here; complete QGC SDK/submodule dependencies are absent |
| Repository-wide `just build` / `just lint` | Blocked: `just` is not installed in this execution environment; focused checks above were run |
| macOS M1 build, launch and memory/latency measurements | Not run; requires Mac acceptance |
| Windows build and launch | Not run; requires Windows acceptance |
| Focused GitHub CI | Workflow supplied, not executed because publishing was denied |
| Read bridge with actual QGC vehicle/SITL | Implemented but not integration-validated |
| Physical gripper, calibration and servo mappings | Not implemented or tested |
| Existing QGC commands governed by new coordinator | Not implemented; original QGC control paths remain independent |
| Reliable conversational operational assistant | Not passed; concrete semantic failures are recorded above |
| Vision, speech, mission execution, underwater autonomy | Not implemented in this package |

## Blueprint coverage

P0 is a source/integration inventory, not a clean full-app baseline build. P1/P2 provide the initial strict simulation contracts and executable coordinator, not every proposed hardware/configuration feature. P3 provides the supervised local worker connection. P4 now has actual model inference and measured failures; its quality/M1 acceptance gate is **open**. P6 contributes atomic configuration persistence; physical calibration is absent. P7 has a read bridge only. P5 and P8–P10 are not claimed complete.

This implementation intentionally makes unsupported work explicit. Replacing the simulator with physical hardware requires a verified adapter, device mapping and feedback path; it is not just a change of numbers. The next engineering work should close the full-app/platform integration gates and strengthen grounded AI interpretation before enabling physical commands.

## Sources and dependency identity

- Original application/API source: the base commit identified above, including `AGENTS.md`, `Vehicle.h`, `MultiVehicleManager.h`, `VehicleLinkManager.h`, `Fact.h`, firmware and build integration files.
- Qt QML module integration: https://doc.qt.io/qt-6/qt-add-qml-module.html
- llama.cpp runtime/provider API: https://github.com/ggml-org/llama.cpp/tree/5aeac37a35b44fde4bdffaec9159f2fcb253b622/tools/server
- Model artifacts: https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct-GGUF and https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct-GGUF (Apache-2.0 metadata; exact revisions/hashes in evidence).

The supplied blueprint remains the long-term design reference. `FOUNDATION.md` documents the actual implemented API, file ownership and build/run steps.
