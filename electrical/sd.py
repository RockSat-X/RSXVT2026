#include "sd.meta"
#meta export SD_CMDS, SD_REGISTERS

import types
import deps.stpy.pxd.pxd as pxd
from deps.stpy.mcus import MCUS



################################################################################
#
# Some SD commands that'll be used.
# @/pg 128/sec 4.7.4/`SD`.
#



SD_CMDS = pxd.SimpleNamespaceTable(
    ('name'                   , 'code', 'acmd', 'waitresp', 'receiving'),
    ('GO_IDLE_STATE'          , 0     , False , 'none'    , False      ),
    ('ALL_SEND_CID'           , 2     , False , 'r2'      , False      ),
    ('SEND_RELATIVE_ADDR'     , 3     , False , 'r6'      , False      ),
    ('SWITCH_FUNC'            , 6     , False , 'r1'      , True       ),
    ('SELECT_DESELECT_CARD'   , 7     , False , 'r1b'     , False      ),
    ('SEND_IF_COND'           , 8     , False , 'r7'      , False      ),
    ('SEND_CSD'               , 9     , False , 'r2'      , False      ),
    ('STOP_TRANSMISSION'      , 12    , False , 'r1b'     , False      ),
    ('SEND_STATUS_TASK_STATUS', 13    , False , 'r1'      , False      ),
    ('SET_BLOCK_LEN'          , 16    , False , 'r1'      , False      ),
    ('READ_SINGLE_BLOCK'      , 17    , False , 'r1'      , True       ),
    ('READ_MULTIPLE_BLOCK'    , 18    , False , 'r1'      , True       ),
    ('WRITE_BLOCK'            , 24    , False , 'r1'      , False      ),
    ('WRITE_MULTIPLE_BLOCK'   , 25    , False , 'r1'      , False      ),
    ('APP_CMD'                , 55    , False , 'r1'      , False      ),
    ('SET_BUS_WIDTH'          , 6     , True  , 'r1'      , False      ),
    ('SD_SEND_OP_COND'        , 41    , True  , 'r3'      , False      ),
    ('SEND_SCR'               , 51    , True  , 'r1'      , True       ),
)



Meta.enums('SDCmd', 'u32', (cmd.name for cmd in SD_CMDS))



for mcu in PER_MCU():

    sdmmc_waitresp = MCUS[mcu].database.get('SDMMC_WAITRESP', None)

    if sdmmc_waitresp is None:
        continue

    Meta.enums('SDWaitRespType', 'u32', sdmmc_waitresp.value.keys())
    Meta.enums('SDWaitRespCode', 'u32', sdmmc_waitresp.value.items())



Meta.lut('SD_CMDS', (
    (
        ('u8'                 , 'code'         , cmd.code                        ),
        ('b32'                , 'acmd'         , cmd.acmd                        ),
        ('enum SDWaitRespType', 'waitresp_type', f'SDWaitRespType_{cmd.waitresp}'),
        ('enum SDWaitRespCode', 'waitresp_code', f'SDWaitRespCode_{cmd.waitresp}'),
        ('b32'                , 'receiving'    , cmd.receiving                   ),
    )
    for cmd in SD_CMDS
))



################################################################################
#
# Layout of SD card register values.
#



SD_REGISTERS = (

    types.SimpleNamespace(
        name          = 'SDCardSpecificData',
        bit_count     = 128,
        version_field = 'CSD_STRUCTURE',
        layouts       = {

            # @/pg 259/tbl 5-16/`SD`.
            2 : [
                ('CSD_STRUCTURE'     , 127, 126, None          ),
                ('TAAC'              , 119, 112, '0x0E'        ),
                ('NSAC'              , 111, 104, '0x00'        ),
                ('TRAN_SPEED'        , 103,  96, "0b0'0110'010"), # Must be 25MHz. @/pg 253/tbl 5-6/`SD`.
                ('CCC'               ,  95,  84, None          ),
                ('READ_BL_LEN'       ,  83,  80, '0x09'        ),
                ('READ_BL_PARTIAL'   ,  79,  79, 0             ),
                ('WRITE_BLK_MISALIGN',  78,  78, 0             ),
                ('READ_BLK_MISALIGN' ,  77,  77, 0             ),
                ('DSR_IMP'           ,  76,  76, None          ),
                ('C_SIZE'            ,  69,  48, None          ),
                ('ERASE_BLK_EN'      ,  46,  46, 1             ),
                ('SECTOR_SIZE'       ,  45,  39, '0x7F'        ),
                ('WP_GRP_SIZE'       ,  38,  32, '0x00'        ),
                ('WP_GRP_ENABLE'     ,  31,  31, 0             ),
                ('R2W_FACTOR'        ,  28,  26, '0x02'        ),
                ('WRITE_BL_LEN'      ,  25,  22, '0x09'        ),
                ('WRITE_BL_PARTIAL'  ,  21,  21, 0             ),
                ('FILE_FORMAT_GRP'   ,  15,  15, 0             ),
                ('COPY'              ,  14,  14, None          ),
                ('PERM_WRITE_PROTECT',  13,  13, None          ),
                ('TEMP_WRITE_PROTECT',  12,  12, None          ),
                ('FILE_FORMAT'       ,  11,  10, 0             ),
                ('WP_UPC'            ,   9,   9, None          ),
                ('CRC'               ,   7,   1, None          ),
            ],

        },
    ),

    types.SimpleNamespace(
        name          = 'SDConfigurationRegister',
        bit_count     = 64,
        version_field = 'SCR_STRUCTURE',
        layouts       = {

            # @/pg 266/tbl 5-17/`SD`.
            1 : [
                ('SCR_STRUCTURE'        , 63, 60, None),
                ('SD_SPEC'              , 59, 56, None),
                ('DATA_STAT_AFTER_ERASE', 55, 55, None),
                ('SD_SECURITY'          , 54, 52, None),
                ('SD_BUS_WIDTHS'        , 51, 48, None),
                ('SD_SPEC3'             , 47, 47, None),
                ('EX_SECURITY'          , 46, 43, None),
                ('SD_SPEC4'             , 42, 42, None),
                ('SD_SPECX'             , 41, 38, None),
                ('CMD_SUPPORT'          , 36, 32, None),
            ],

        },
    ),

    # @/pg 110/tbl 4-13/`SD`.
    types.SimpleNamespace(
        name          = 'SDSwitchFunctionStatus',
        bit_count     = 512,
        version_field = 'version',
        layouts       = {

            1 : [
                ('max_consumption'  , 511, 496, None),
                ('group_6_support'  , 495, 480, None),
                ('group_5_support'  , 479, 464, None),
                ('group_4_support'  , 463, 448, None),
                ('group_3_support'  , 447, 432, None),
                ('group_2_support'  , 431, 416, None),
                ('group_1_support'  , 415, 400, None),
                ('group_6_selection', 399, 396, None),
                ('group_5_selection', 395, 392, None),
                ('group_4_selection', 391, 388, None),
                ('group_3_selection', 387, 384, None),
                ('group_2_selection', 383, 380, None),
                ('group_1_selection', 379, 376, None),
                ('version'          , 375, 368, None),
            ],

            2 : [
                ('max_consumption'  , 511, 496, None),
                ('group_6_support'  , 495, 480, None),
                ('group_5_support'  , 479, 464, None),
                ('group_4_support'  , 463, 448, None),
                ('group_3_support'  , 447, 432, None),
                ('group_2_support'  , 431, 416, None),
                ('group_1_support'  , 415, 400, None),
                ('group_6_selection', 399, 396, None),
                ('group_5_selection', 395, 392, None),
                ('group_4_selection', 391, 388, None),
                ('group_3_selection', 387, 384, None),
                ('group_2_selection', 383, 380, None),
                ('group_1_selection', 379, 376, None),
                ('version'          , 375, 368, None),
                ('group_6_busy'     , 367, 352, None),
                ('group_5_busy'     , 351, 336, None),
                ('group_4_busy'     , 335, 320, None),
                ('group_3_busy'     , 319, 304, None),
                ('group_2_busy'     , 303, 288, None),
                ('group_1_busy'     , 287, 272, None),
            ],

        },
    ),

)



################################################################################
#
# Post-process `SD_REGISTERS`.
#



for register in SD_REGISTERS:

    for version, fields in register.layouts.items():

        for field_i, field in enumerate(fields):

            name, high_bit, low_bit, expected = field



            # The given byte-array starts
            # from most-significant byte to
            # least-significant, so indices
            # and terms will be "backwards".

            start_byte_index = (register.bit_count // 8 - 1) - (low_bit  // 8)
            last_byte_index  = (register.bit_count // 8 - 1) - (high_bit // 8)
            shift_excess     = []



            # The entire field is contained in a single byte.

            if start_byte_index == last_byte_index:

                shift_excess += [(
                    low_bit % 8,
                    ((high_bit + 7) // 8) * 8 - high_bit - 1
                )]



            # The field straddles across byte boundaries.

            else:

                shift_excess += [(
                    low_bit % 8,
                    0
                )]



            # For fields where there's whole bytes in the
            # middle (and thus no shifts and masks needed).

            shift_excess += [(0, 0)] * (start_byte_index - last_byte_index - 1)



            # The field has some leftover
            # most-significant bits in the
            # last byte.

            if start_byte_index != last_byte_index:

                shift_excess += [(
                    0,
                    7 - (high_bit % 8)
                )]



            # Put all the bytes together through masks
            # and shifts to compute the field's value.

            terms    = []
            position = 0
            for shift, excess in shift_excess:

                length = 8 - shift - excess
                mask   = (1 << length) - 1
                term   = f'source[{start_byte_index - len(terms)}]'

                if shift    : term = f'({term} >> {shift})'
                if excess   : term = f'({term} & {mask})'
                if position : term = f'({term} << {position})'

                position += length
                terms    += [term]



            # Determine smallest type for the field.

            bit_count = high_bit - low_bit + 1

            if   bit_count <= 8  : type = 'u8'
            elif bit_count <= 16 : type = 'u16'
            else                 : type = 'u32'



            # Update the register field with the
            # post-processing stuff we just did.

            fields[field_i] = types.SimpleNamespace(
                name        = name,
                expected    = expected,
                bits        = ' | '.join(terms),
                struct_type = type,
                struct_name = name if version is None else f'v{version}_{name}',
            )



    # Find the version field.

    if register.version_field is not None:

        register.version_field, *_ = (
            field
            for version, fields in register.layouts.items()
            for field in fields
            if field.name == register.version_field
        )



################################################################################
#
# Make comprehensive structures for storing
# the fields of the SD card's registers.
#



for register in SD_REGISTERS:



    if register.version_field is not None:

        # The SD document typically refer to
        # the version number off-by-one.

        Meta.enums(
            f'{register.name}Version',
            'u32',
            ((version, version - 1) for version in register.layouts)
        )



    with Meta.enter(f'struct {register.name}'):



        # Simply list the fields if there's only one version.

        if register.version_field is None:

            for field in register.layouts[None]:
                Meta.line(f'''
                    {field.struct_type} {field.struct_name};
                ''')

            continue



        # Version field determines which version
        # of the register should be used.

        Meta.line(f'''
            enum {register.name}Version {register.version_field.name};
        ''')



        # Make the fields from different versions
        # of the register overlap in memory.

        with Meta.enter('union'):

            for version, fields in register.layouts.items():

                with Meta.enter('struct'):

                    for field in fields:

                        Meta.line(f'''
                            {field.struct_type} {field.struct_name};
                        ''')



################################################################################
#
# Generic procedure to parse SD registers.
#



with Meta.enter('#define SD_parse_register(DST, SRC)'):

    with Meta.enter('_Generic', '(', ')((DST), (SRC))'):

        Meta.line('''
            (DST),
        ''')

        Meta.line(',\n'.join(
            f'struct {register.name}* : _SD_parse_{register.name}'
            for register in SD_REGISTERS
        ))



################################################################################
#
# Procedures to parse an SD register.
#



for register in SD_REGISTERS:

    with Meta.enter(f'''
        static useret b32
        _SD_parse_{register.name}(struct {register.name}* destination, u8* source)
    '''):



        def parse(version):



            # Get the fields that'll be extracted and asserted.

            assignments = []

            if version is not None:
                assignments += [(
                    register.version_field.name,
                    register.version_field.bits,
                    None
                )]

            assignments += [
                (field.struct_name, field.bits, field.expected)
                for field in register.layouts[version]
            ]



            # Extract the fields.

            with Meta.enter(
                '*destination =',
                f'''
                (struct {register.name})
                {{
                ''',
                '};',
                indented = True
            ):

                for columns in pxd.justify(
                    (
                        ('<', name),
                        ('<', bits),
                    )
                    for name, bits, expected in assignments
                ):
                    Meta.line('.{} = {},'.format(*columns))



            # Assert the fields.

            for columns in pxd.justify(
                (
                    ('<', name    ),
                    ('<', expected),
                )
                for name, bits, expected in assignments
                if expected is not None
            ):
                Meta.line('''
                    if (destination->{} != {}) return false;
                '''.format(*columns))



        # Clear the register struct.

        Meta.line('''
            *destination = (typeof(*destination)) {0};
        ''')



        # Parsing a register with only one layout.

        if register.version_field is None:

            parse(None)



        # Parsing a register with multiple layouts.

        else:

            with Meta.enter(f'''
                switch ({register.version_field.bits})
            '''):

                for version in register.layouts:

                    with Meta.enter(f'''
                        case {register.name}Version_{version}:
                    '''):

                        parse(version)

                Meta.line('''
                    default: return false;
                ''')



        # If the fields that have expected
        # values all match up, then we have
        # successfully parsed the register.

        Meta.line('return true;')
