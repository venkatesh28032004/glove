import asyncio
import bleak
import pyautogui as pag

def do_action(signal):
    if signal == "Forward fall gesture":
        pag.hotkey("ctrl", "z")
    elif signal == "Up clock gesture":
        pag.hotkey("ctrl", "y")
    elif signal == "Left fall gesture":
        pag.hotkey("alt", "tab")
    else:
        pag.press("start")
async def monitor_disconnection(client):
    while client.is_connected:
        await asyncio.sleep(1)  # Check connection status every second
    print("Disconnected from BLE device.")
async def main():
    # Replace with your ESP32 BLE server's MAC address
    address = "08:D1:F9:35:C5:FA"

    # Service and characteristic UUIDs (modify if needed)
    service_uuid = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"  # Example: "0000ffe0-0000-1000-8000-00805f9b34fb"
    characteristic_uuid = "beb5483e-36e1-4688-b7f5-ea07361b26a8"  # Example: "0000fff1-0000-1000-8000-00805f9b34fb"

    async with bleak.BleakClient(address) as client:
        print(f"Connected to: {client.is_connected}")

        # Discover services and characteristics
        print("Discovering services and characteristics...")
        for service in await client.get_services():
            print(f"- Service: {service}")
            for characteristic in service.characteristics:
                print(f"  - Characteristic: {characteristic}")

                # Check if the characteristic matches the expected UUID
                if characteristic.uuid == characteristic_uuid:
                    print(f"Found characteristic with UUID: {characteristic.uuid}")

                    # Check if notification is supported
                    # if not characteristic.properties.notify:
                    #     print("Characteristic does not support notifications.")
                    #     continue

                    # Subscribe to notifications
                    def notification_handler(sender, data):
                        print(f"Notification received: {data.hex()}")
                        try:
                            # Decode data based on format (e.g., ASCII, integer)
                            decoded_data = data.decode('ascii')  # Change if needed
                            print(f"ESP32 says: {decoded_data}")
                            do_action(decoded_data)
                        except UnicodeDecodeError:
                            print(f"Unable to decode data (non-ASCII).")

                    await client.start_notify(characteristic, notification_handler)

                    # Wait for notifications (optional timeout)
                    print("Waiting for notifications (press Enter to exit)...")
                    disconnect_task = asyncio.create_task(monitor_disconnection(client))
                    try:
                        await asyncio.gather(asyncio.sleep(10), disconnect_task)
                    except asyncio.TimoutError:
                        print("Timeout reached.")

                    # Unsubscribe from notifications (optional)
                    await client.stop_notify(characteristic)
                    break  # Only one characteristic needed

        # If the expected characteristic wasn't found, indicate it
        else:
            print(f"Characteristic with UUID {characteristic_uuid} not found.")

if name == "main":
    asyncio.run(main())
