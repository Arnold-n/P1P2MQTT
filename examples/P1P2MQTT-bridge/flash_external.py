Import("env")

def flash_external(target, source, env):
    binary_path = env.GetProjectOption("external_binary_path")
    upload_protocol = env.get("UPLOAD_PROTOCOL", "esptool")

    if not binary_path:
        print("Error: No binary path specified in the environment.")
        return

    if upload_protocol == "espota":
        # Command for OTA
        print(f"Flashing external binary via OTA: {binary_path} to IP: {env.get('UPLOAD_PORT')}")
        env.Execute(
            f"$PYTHONEXE $UPLOADER --auth=P1P2MQTT --debug --progress -i $UPLOAD_PORT -f {binary_path}"
        )
    else:
        # Command for USB (esptool)
        print(f"Flashing external binary via USB: {binary_path} on port: {env.get('UPLOAD_PORT')}")
        env.Execute(
            f"$PYTHONEXE $UPLOADER --before default_reset --after hard_reset --chip esp8266 --port $UPLOAD_PORT --baud $UPLOAD_SPEED write_flash 0x0 {binary_path}"
        )

env.AddCustomTarget("flash_external", None, flash_external, "Flash official firmware")