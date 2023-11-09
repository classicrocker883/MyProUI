#
# random-bin.py
# Set a unique firmware name based on current date and time
#
import pioutil
if pioutil.is_pio_build():
    from datetime import datetime
    from SCons.Script import Import
    from SCons.Script import DefaultEnvironment
    env = DefaultEnvironment()
    Import("env")
    env['PROGNAME'] = datetime.now().strftime("firmware-%Y%m%d-%H%M%S")
    def name_target(target, source, env):
        print("FIRMWARE ELF: %s.elf" % env['PROGNAME'])
        print("FIRMWARE BIN: %s.bin" % env['PROGNAME'])
    import marlin
    marlin.add_post_action(name_target)