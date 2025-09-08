#meta types, log, ANSI,       root, justify, coalesce, find_dupe, mk_dict, OrderedSet, AllocatingNamespace, ContainedNamespace, CMSIS_SET, CMSIS_WRITE, CMSIS_SPINLOCK, CMSIS_TUPLE :
from deps.pxd.utils    import root, justify, coalesce, find_dupe, mk_dict, OrderedSet, AllocatingNamespace, ContainedNamespace, c_repr
from deps.pxd.log      import log, ANSI
from deps.stpy.helpers import get_helpers
import types



helpers        = get_helpers(Meta)
CMSIS_SET      = helpers.CMSIS_SET
CMSIS_WRITE    = helpers.CMSIS_WRITE
CMSIS_SPINLOCK = helpers.CMSIS_SPINLOCK
CMSIS_TUPLE    = helpers.CMSIS_TUPLE




################################################################################################################################



# This file is the meta-directive where we
# define any global functions/constants
# for all other meta-directives to have
# without having them to explicitly import it.
