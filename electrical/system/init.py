#include "SYSTEM_init.meta"
#meta

@Meta.ifs(TARGETS, '#if')
def _(target):

    yield f'TARGET_NAME_IS_{target.name}'



    # Output the system initialization routine where we configure the low-level MCU stuff.

    with Meta.enter('''
        extern void
        SYSTEM_init(void)
    '''):



        # Figure out the register values.

        configuration, defines = SYSTEM_PARAMETERIZE(target, SYSTEM_OPTIONS[target.name])



        # Figure out the procedure to set the register values.

        SYSTEM_CONFIGURIZE(target, configuration)



    # We also make defines so that the C code can use the results
    # of the parameterization and configuration procedure (e.g. the resulting CPU frequency).

    for name, expansion in defines:
        Meta.define(name, expansion)

