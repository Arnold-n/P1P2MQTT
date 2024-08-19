Import("env")
env.Replace(PROGNAME="P1P2MQTT_v%s-%s" % (env.GetProjectOption("custom_version"), env["PIOENV"]))