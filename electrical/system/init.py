#include "SYSTEM_init.meta"
#meta

@Meta.ifs(SYSTEM_OPTIONS, style = '#if')
def _(system_options):

    target_name, options = system_options
    target               = TARGETS.get(target_name)

    yield f'TARGET_NAME_IS_{target_name}'



    # Output the system initialization routine where we configure the low-level MCU stuff.

    with Meta.enter('''
        extern void
        SYSTEM_init(void)
    '''):

        configuration, defines = SYSTEM_PARAMETERIZE(target, options)

        for name, expansion in defines:
            Meta.define(name, expansion)

        SYSTEM_CONFIGURIZE(target, configuration)
