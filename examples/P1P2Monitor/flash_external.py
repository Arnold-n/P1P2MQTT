Import("env")

def flash_external(source, target, env):
    binary_path = env.GetProjectOption("external_binary_path")
    bridge_ip = env.GetProjectOption("bridge_ip")
    if not binary_path or not bridge_ip:
        print("Error: external_binary_path or bridge_ip is not defined in platformio.ini")
        return
    print("Flashing external firmware: ", binary_path)
    env.Execute("avrdude -c avrisp -p atmega328p -P net:{}:328 -e -Uflash:w:{}:i".format(bridge_ip, binary_path))

env.AddCustomTarget("flash_external", None, flash_external, "Flash official firmware")