#meta types, log, ANSI,       root, justify, coalesce, find_dupe, mk_dict, OrderedSet, AllocatingNamespace, ContainedNamespace, CMSIS_SET, CMSIS_WRITE, CMSIS_SPINLOCK, CMSIS_TUPLE :
from deps.pxd.utils        import root, justify, coalesce, find_dupe, mk_dict, OrderedSet, AllocatingNamespace, ContainedNamespace, c_repr
from deps.pxd.log          import log, ANSI
from deps.stpy.cmsis_tools import get_cmsis_tools
import types



cmsis_tools    = get_cmsis_tools(Meta)
CMSIS_SET      = cmsis_tools.CMSIS_SET
CMSIS_WRITE    = cmsis_tools.CMSIS_WRITE
CMSIS_SPINLOCK = cmsis_tools.CMSIS_SPINLOCK
CMSIS_TUPLE    = cmsis_tools.CMSIS_TUPLE




################################################################################################################################



# This file is the meta-directive where we
# define any global functions/constants
# for all other meta-directives to have
# without having them to explicitly import it.
