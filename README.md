# Yousif Control Center

An experimental drone control interface built on [QGroundControl](https://github.com/mavlink/qgroundcontrol), exploring a shared command system for a desktop interface, an AI assistant, and future smart-glasses input.

**Stage: prototype development.** The long-term goal is natural-language and visual assistance for drone operation. Cloud deployment, glasses control, and end-to-end AI flight execution are development goals, not demonstrated capabilities.

## Project work in this repository

The default branch, `yousif/control-center`, contains the custom interface and development-profile foundation:

| Area | Repository evidence | Scope |
| --- | --- | --- |
| Custom interface | [QML components](custom/qml) | Operator interface customization |
| Development profiles | [ProfileStore header](custom/src/ProfileStore.h), [implementation](custom/src/ProfileStore.cc), [example profiles](custom/profiles) | Profile loading and an exposed action catalog; a profile is not proof that hardware is connected |
| Application integration | [Custom build configuration](custom/CMakeLists.txt), [resources](custom/custom.qrc) | Custom components within the QGroundControl codebase |
| Architecture | [Component responsibilities and command rules](custom/docs/ARCHITECTURE.md) | Design reference; implementation status must be checked against the relevant branch |
| Development tooling | [Custom tools](custom/tools) | Supporting development scripts |

These links identify existing source work, not a claim that every component has been tested on a physical aircraft. QGroundControl supplies the underlying ground-station functionality; this project's contribution is the custom layer.

## Next milestone: one traceable command in simulation

The next integration target is a request that can be followed from operator input to a recorded simulator outcome.

- [ ] Define a versioned request format with request ID, vehicle ID, action, arguments, timestamp, and expiry.
- [ ] Route desktop and AI requests through the same command coordinator.
- [ ] Reject unsupported actions, invalid arguments, expired requests, and requests for the wrong vehicle.
- [ ] Connect the coordinator to one selected autopilot simulator and document the setup.
- [ ] Record request receipt, validation, dispatch, acknowledgment, and observed completion separately.
- [ ] Demonstrate a supported request and rejection cases, with reproducible instructions and logs.
- [ ] Measure end-to-end latency before making responsiveness claims.

## Cloud and glasses integration plan

Keep AI processing behind a defined service interface so local and cloud execution can be evaluated without redesigning the operator interface. Separate the development environment from the service's runtime deployment.

A future glasses adapter should submit the same structured requests as the desktop interface. Direct device access, phone mediation, display access, and gesture input remain to be verified with the actual hardware and available interfaces. A cloud service also needs a defined connection to the vehicle-side communication gateway.

Flight stabilization and link-loss behavior remain the autopilot's responsibility. AI output is a proposed action that must pass validation; it is not executable code.

## Progress note — 2026-10-29

This update makes the project scope, existing source components, and next integration milestone visible from the repository front page. It is a documentation milestone; no new flight, cloud, or glasses capability is claimed.

---

## Upstream QGroundControl

The original upstream overview follows. Its downloads, badges, and feature descriptions refer to QGroundControl, not a packaged release of Yousif Control Center.

<p align="center">
  <img src="https://raw.githubusercontent.com/Dronecode/UX-Design/35d8148a8a0559cd4bcf50bfa2c94614983cce91/QGC/Branding/Deliverables/QGC_RGB_Logo_Horizontal_Positive_PREFERRED/QGC_RGB_Logo_Horizontal_Positive_PREFERRED.svg" alt="QGroundControl Logo" width="500">
</p>

<p align="center">
  <a href="https://github.com/mavlink/QGroundControl/releases"><img src="https://img.shields.io/github/v/release/mavlink/QGroundControl" alt="Latest Release"></a>
  <a href="https://github.com/mavlink/qgroundcontrol/blob/master/.github/COPYING.md"><img src="https://img.shields.io/github/license/mavlink/QGroundControl" alt="License"></a>
  <a href="https://github.com/mavlink/QGroundControl/actions/workflows/linux.yml"><img src="https://github.com/mavlink/QGroundControl/actions/workflows/linux.yml/badge.svg" alt="Linux Build"></a>
  <a href="https://securityscorecards.dev/viewer/?uri=github.com/mavlink/qgroundcontrol"><img src="https://img.shields.io/ossf-scorecard/github.com/mavlink/qgroundcontrol?label=openssf%20scorecard" alt="OpenSSF Scorecard"></a>
  <a href="https://crowdin.com/project/qgroundcontrol"><img src="https://badges.crowdin.net/qgroundcontrol/localized.svg" alt="Crowdin"></a>
  <a href="https://discord.com/channels/1022170275984457759/1022185820683255908"><img src="https://img.shields.io/discord/1022170275984457759?logo=discord&logoColor=white&label=Discord" alt="Dronecode Discord"></a>
  <a href="https://doi.org/10.5281/zenodo.595404"><img src="https://zenodo.org/badge/DOI/10.5281/zenodo.595404.svg" alt="DOI"></a>
</p>

**QGroundControl** (QGC) is a Ground Control Station (GCS) for UAVs, providing full flight control
and mission planning for any *MAVLink-enabled* drone, including *PX4* and *ArduPilot* platforms.

## Features

- **Mission planning** — plan, edit, and fly autonomous waypoint, survey, and structure-scan missions.
- **Live Fly View** — real-time flight display with map, instruments, and full vehicle telemetry.
- **Vehicle setup** — guided wizards for sensor calibration, radio, flight modes, and power.
- **Parameter tuning** — inspect and edit every vehicle parameter through the Fact System.
- **Video streaming** — GStreamer-based UDP RTP / RTSP video with recording in the Flight Display.
- **Multi-vehicle** — connect to and monitor multiple vehicles simultaneously.
- **MAVLink tooling** — built-in MAVLink Inspector, console, and log download/analysis.
- **Cross-platform** — Windows, macOS, Linux, Android, and iOS from a single codebase.

## Download

Grab the latest stable build for your platform, or see all assets on the
[releases page](https://github.com/mavlink/QGroundControl/releases/latest):

<p align="center">
  <a href="https://github.com/mavlink/QGroundControl/releases/latest/download/QGroundControl-installer.exe"><img src="https://img.shields.io/badge/Windows-0078D6?logo=windows&logoColor=white" alt="Windows"></a>
  <a href="https://github.com/mavlink/QGroundControl/releases/latest/download/QGroundControl.dmg"><img src="https://img.shields.io/badge/macOS-000000?logo=apple&logoColor=white" alt="macOS"></a>
  <a href="https://github.com/mavlink/QGroundControl/releases/latest/download/QGroundControl-x86_64.AppImage"><img src="https://img.shields.io/badge/Linux-FCC624?logo=linux&logoColor=black" alt="Linux (AppImage)"></a>
  <a href="https://github.com/mavlink/QGroundControl/releases/latest/download/QGroundControl.apk"><img src="https://img.shields.io/badge/Android-3DDC84?logo=android&logoColor=white" alt="Android"></a>
</p>

## Links

- [Official Website](http://qgroundcontrol.com)
- [User Manual](https://docs.qgroundcontrol.com/en/)
- [Developer Guide](https://dev.qgroundcontrol.com/en/) / [Build Instructions](https://dev.qgroundcontrol.com/en/getting_started/)
- [Discussion & Support](https://docs.qgroundcontrol.com/en/Support/Support.html)
- [Dronecode Discord](https://discord.com/channels/1022170275984457759/1022185820683255908)
- [Security Policy](.github/SECURITY.md)
- [Code of Conduct](.github/CODE_OF_CONDUCT.md)
- [License](https://github.com/mavlink/qgroundcontrol/blob/master/.github/COPYING.md)

## Contributing

QGC is open source and welcomes contributions. See [AGENTS.md](AGENTS.md) for build/test/lint
commands and coding conventions, and [.github/CONTRIBUTING.md](.github/CONTRIBUTING.md) for
architecture patterns and the contribution workflow.

QGC's interface is translated by the community — help translate it into your language on
[Crowdin](https://crowdin.com/project/qgroundcontrol).

## Star history

[![Star History Chart](https://api.star-history.com/svg?repos=mavlink/qgroundcontrol&type=Date)](https://star-history.com/#mavlink/qgroundcontrol&Date)

