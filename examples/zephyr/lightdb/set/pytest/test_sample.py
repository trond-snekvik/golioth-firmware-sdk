import contextlib
import logging
import pytest
import time
import yaml
import datetime

LOGGER = logging.getLogger(__name__)

pytestmark = pytest.mark.anyio

async def test_lightdb_set(shell, device, wifi_ssid, wifi_psk):
    # Delete counter in lightdb state

    await device.lightdb.delete("counter")

    time.sleep(2)

    # Set Golioth credential

    golioth_cred = (await device.credentials.list())[0]
    shell.exec_command(f"settings set golioth/psk-id {golioth_cred.identity}")
    shell.exec_command(f"settings set golioth/psk {golioth_cred.key}")

    # Set WiFi credential

    shell.exec_command(f"settings set wifi/ssid \"{wifi_ssid}\"")
    shell.exec_command(f"settings set wifi/psk \"{wifi_psk}\"")

    shell._device.clear_buffer()
    shell._device.write('kernel reboot cold\n\n'.encode())
    shell._device.readlines_until(regex=".*Booting", timeout=10.0)

    # Wait for Golioth connection

    shell._device.readlines_until(regex=".*Golioth CoAP client connected", timeout=30.0)

    # Verify lightdb writes

    for i in range(0,4):
        shell._device.readlines_until(regex=f".*Setting counter to {i}", timeout=10.0)
        counter = await device.lightdb.get("counter")
        assert counter == i
