"""Shared policy for skipping device execution in explicitly no-GPU jobs."""


NO_DEVICE_ERROR_CODES = frozenset({
    "no_device",
    "driver_unavailable",
    "initialization_failed",
})


def should_skip_device_execution(error_code, *, allow_no_device_skip,
                                 require_device):
    """Return true only for recognized unavailable-device errors in opt-in CI."""
    return (
        allow_no_device_skip
        and not require_device
        and error_code in NO_DEVICE_ERROR_CODES
    )
