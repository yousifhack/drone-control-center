# Control Center Architecture

## Purpose
Adapt the operator interface and available actions to the connected
vehicle, using shared information for buttons, controllers, and AI.

## Component responsibilities
- Vehicle integration: obtain identity, telemetry, and reported
  capabilities through QGroundControl.
- Profile storage: load and save operator-configured capabilities.
- Capability resolver: combine reported capabilities with a verified
  profile and current vehicle state.
- Action catalog: define action identifiers, inputs, and requirements.
- Command coordinator: validate requests and track their execution.
- Interface: display state and submit action requests.
- Controller adapters: translate device input into action requests.
- AI service: interpret observations and propose supported actions.
- Event recorder: record requests, decisions, and outcomes.

## Rules
- A profile describes hardware; it does not prove hardware is present.
- A declared capability is not automatically executable.
- Unknown capabilities remain unavailable.
- Discovery does not silently change flight tuning.
- Buttons, controllers, and AI use the same command coordinator.
- Generated text and vehicle metadata are never executed as code.
- Vehicle selection and permissions are checked before execution.
- Command acknowledgment is distinct from physical completion.
- Profiles are versioned and associated with a verified vehicle identity.
- Development profiles never authorize real vehicle commands.

## Current integration
- custom/cmake/CustomOverrides.cmake sets application branding.
- custom/CMakeLists.txt includes custom/custom.qrc.
- custom/custom.qrc bundles the custom interface files.
- src/FlyView/FlyViewCustomLayer.qml loads VehicleStatusPanel.qml.
- custom/qml/VehicleStatusPanel.qml displays vehicle status.

## Current limitations
- The status panel has temporary positioning.
- Capability profiles are not yet loaded by the application.
- No custom command execution or AI integration exists yet.
