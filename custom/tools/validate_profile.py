"""Validate version 1 development vehicle profiles. Sends no commands."""

import json
import sys
from pathlib import Path


def require(condition, message):
    if not condition:
        raise ValueError(message)


def require_text(value, location):
    require(
        isinstance(value, str) and bool(value.strip()),
        f"{location}: expected non-empty text",
    )


def validate_profile(profile):
    require(isinstance(profile, dict), "Profile must be a JSON object")

    version = profile.get("schemaVersion")
    require(
        type(version) is int and version == 1,
        "schemaVersion: only version 1 is supported",
    )

    for field in ("profileId", "displayName"):
        require_text(profile.get(field), field)

    # This first validator intentionally supports development profiles only.
    require(
        profile.get("developmentOnly") is True,
        "Only development profiles are supported at this stage",
    )
    require(
        "vehicleIdentity" in profile and profile["vehicleIdentity"] is None,
        "Development profiles must have vehicleIdentity set to null",
    )
    require(
        profile.get("expectedVehicleClass") == "multirotor",
        "This initial profile format currently supports multirotor examples",
    )

    accessories = profile.get("accessories")
    require(isinstance(accessories, list), "accessories must be a list")

    accessory_ids = set()
    action_ids = set()

    for index, accessory in enumerate(accessories):
        location = f"accessories[{index}]"
        require(isinstance(accessory, dict), f"{location}: expected an object")

        accessory_id = accessory.get("accessoryId")
        require_text(accessory_id, f"{location}.accessoryId")
        require(
            accessory_id not in accessory_ids,
            f"Duplicate accessoryId: {accessory_id}",
        )
        accessory_ids.add(accessory_id)

        require_text(accessory.get("displayName"), f"{location}.displayName")
        require(accessory.get("type") == "gripper", f"{location}: unsupported accessory type")
        require(
            accessory.get("verificationState") == "unverified",
            f"{location}: development accessory must remain unverified",
        )

        for field in ("hardwareBinding", "feedbackBinding"):
            require(
                field in accessory and accessory[field] is None,
                f"{location}.{field}: must be null at this stage",
            )

        actions = accessory.get("actions")
        require(
            isinstance(actions, list) and len(actions) > 0,
            f"{location}.actions: expected a non-empty list",
        )

        for action in actions:
            require(isinstance(action, dict), f"{location}: action must be an object")
            action_id = action.get("actionId")
            require_text(action_id, f"{location}.actionId")
            require(
                action_id in ("gripper.open", "gripper.close"),
                f"Unsupported action: {action_id}",
            )
            require(action_id not in action_ids, f"Duplicate actionId: {action_id}")
            action_ids.add(action_id)
            require_text(action.get("displayName"), f"{action_id}.displayName")

    return len(accessories), len(action_ids)


def main():
    if len(sys.argv) != 2:
        print("Usage: python custom/tools/validate_profile.py PROFILE.json")
        return 2

    path = Path(sys.argv[1])

    try:
        profile = json.loads(path.read_text(encoding="utf-8"))
        accessory_count, action_count = validate_profile(profile)
    except (OSError, ValueError) as error:
        print(f"INVALID: {error}")
        return 1

    print(f"VALID: {profile['displayName']}")
    print(f"Accessories: {accessory_count}")
    print(f"Declared actions: {action_count}")
    print("Development profile only. Hardware execution is not authorized.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
