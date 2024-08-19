Import("env")
env.Replace(PROGNAME="P1P2Monitor_v%s-%s" % (env.GetProjectOption("custom_version"), env["PIOENV"]))