#include "SD_defs.meta"
/* #meta SDMMC_UNITS, SD_CMDS, SD_REGISTERS

    import types
    from deps.stpy.pxd.utils import SimpleNamespaceTable, c_repr
    from deps.stpy.mcus      import MCUS

    #
    # Each SDMMC peripheral we're supporting.
    #

    SDMMC_UNITS = (1, 2)

    #
    # Some SD commands that'll be used. @/pg 128/sec 4.7.4/`SD`.
    #

    SD_CMDS = SimpleNamespaceTable(
        ('name'                   , 'code', 'acmd', 'resp', 'receiving'),
        ('GO_IDLE_STATE'          , 0     , False , None  , False      ),
        ('ALL_SEND_CID'           , 2     , False , 'r2'  , False      ),
        ('SEND_RELATIVE_ADDR'     , 3     , False , 'r6'  , False      ),
        ('SWITCH_FUNC'            , 6     , False , 'r1'  , True       ),
        ('SELECT_DESELECT_CARD'   , 7     , False , 'r1b' , False      ),
        ('SEND_IF_COND'           , 8     , False , 'r7'  , False      ),
        ('SEND_CSD'               , 9     , False , 'r2'  , False      ),
        ('STOP_TRANSMISSION'      , 12    , False , 'r1b' , False      ),
        ('SEND_STATUS_TASK_STATUS', 13    , False , 'r1'  , False      ),
        ('SET_BLOCK_LEN'          , 16    , False , 'r1'  , False      ),
        ('READ_SINGLE_BLOCK'      , 17    , False , 'r1'  , True       ),
        ('READ_MULTIPLE_BLOCK'    , 18    , False , 'r1'  , True       ),
        ('WRITE_BLOCK'            , 24    , False , 'r1'  , False      ),
        ('WRITE_MULTIPLE_BLOCK'   , 25    , False , 'r1'  , False      ),
        ('APP_CMD'                , 55    , False , 'r1'  , False      ),
        ('SET_BUS_WIDTH'          , 6     , True  , 'r1'  , False      ),
        ('SD_SEND_OP_COND'        , 41    , True  , 'r3'  , False      ),
        ('SEND_SCR'               , 51    , True  , 'r1'  , True       ),
    )

    #
    # Layout of SD card register values.
    #

    SD_REGISTERS = {
        'SDCardSpecificData' : types.SimpleNamespace(
            bit_count     = 128,
            version_field = 'CSD_STRUCTURE',
            layouts       = {
                2 : [ # @/pg 259/tbl 5-16/`SD`.
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
        'SDConfigurationRegister' : types.SimpleNamespace(
            bit_count     = 64,
            version_field = 'SCR_STRUCTURE',
            layouts       = {
                1 : [ # @/pg 266/tbl 5-17/`SD`.
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
        'SDSwitchFunctionStatus' : types.SimpleNamespace( # @/pg 110/tbl 4-13/`SD`.
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
    }

    for register_name, register in SD_REGISTERS.items():

        #
        # Process the fields of the registers.
        #

        for version, fields in register.layouts.items():

            for field_i, field in enumerate(fields):

                name, high_bit, low_bit, expected = field

                # The "src" byte-array starts from most-significant byte to least-significant, so indices and terms will be "backwards".
                start_byte_index = register.bit_count // 8 - 1 - low_bit  // 8
                last_byte_index  = register.bit_count // 8 - 1 - high_bit // 8
                shift_excess     = []

                if start_byte_index == last_byte_index:
                    # The entire field is contained in a single byte.
                    shift_excess += [(low_bit % 8, ((high_bit + 7) // 8) * 8 - high_bit - 1)]
                else:
                    # The field straddles across byte boundaries.
                    shift_excess += [(low_bit % 8, 0)]

                # For fields where there's whole bytes in the middle (and thus no shifts and masks needed).
                shift_excess += [(0, 0)] * (start_byte_index - last_byte_index - 1)

                if start_byte_index != last_byte_index:
                    # The field has some leftover most-significant bits in the last byte.
                    shift_excess += [(0, 7 - high_bit % 8)]

                # Put all the bytes together through masks and shifts to compute the field's value.
                terms    = []
                position = 0
                for shift, excess in shift_excess:

                    length = 8 - shift - excess
                    mask   = (1 << length) - 1
                    term   = f'src[{start_byte_index - len(terms)}]'

                    if shift  != 0 : term = f'({term} >> {shift})'
                    if excess != 0 : term = f'({term} & {mask})'
                    if position    : term = f'({term} << {position})'

                    position += length
                    terms    += [term]

                # Determine smallest type for the field.
                bit_count = high_bit - low_bit + 1
                if   bit_count <= 8  : type = 'u8'
                elif bit_count <= 16 : type = 'u16'
                else                 : type = 'u32'

                fields[field_i] = types.SimpleNamespace(
                    name        = name,
                    expected    = expected,
                    bits        = ' | '.join(terms),
                    struct_type = type,
                    struct_name = name if version is None else f'v{version}_{name}',
                )

        #
        # Find the version field.
        #

        if register.version_field is not None:

            version_fields = {
                version : { field.name : field for field in fields }[register.version_field]
                for version, fields in register.layouts.items()
            }

            if len({ version_field.bits for version_field in version_fields.values() }) >= 2:
                raise ValueError(
                    f'SD register {register_name} has version field "{register.version_field}" '
                    f'that\'s not consistent across different versions.'
                )

            register.version_field, *_ = version_fields.values()

    #
    # Make some enumerations.
    #

    Meta.enums('SD'   , 'u32', (f'SDMMC{unit}' for unit in SDMMC_UNITS))
    Meta.enums('SDCmd', 'u32', (cmd.name       for cmd  in SD_CMDS    ))

    for register_name, register in SD_REGISTERS.items():
        if register.version_field is not None:
            Meta.enums(
                f'{register_name}Version',
                'u32',
                ((version, version - 1) for version in register.layouts) # The SD document typically refer to the version number off-by-one.
            )

    for mcu in PER_MCU():
        if 'SDMMC_WAITRESP' in MCUS[mcu].database:
            Meta.enums('SDWaitResp', 'u32', MCUS[mcu]['SDMMC_WAITRESP'].value.keys())

    #
    # Table for each SD driver.
    #

    Meta.lut('SD_DRIVER_TABLE', ((
        ('SDMMC_TypeDef*'    , 'peripheral', f'SDMMC{unit}'              ),
        ('enum NVICInterrupt', 'interrupt' , f'NVICInterrupt_SDMMC{unit}'),
    ) for unit in SDMMC_UNITS))

    #
    # Table for the SD card commands.
    #

    for mcu in PER_MCU():
        if 'SDMMC_WAITRESP' in MCUS[mcu].database:
            Meta.lut('SD_CMDS', ((
                ('u8'             , 'code'         , cmd.code                                   ),
                ('b8'             , 'acmd'         , cmd.acmd                                   ),
                ('enum SDWaitResp', 'waitresp_kind', f'SDWaitResp_{c_repr(cmd.resp)}'           ),
                ('u8'             , 'waitresp_bits', MCUS[mcu]['SDMMC_WAITRESP'].value[cmd.resp]),
                ('b8'             , 'receiving'    , cmd.receiving                              ),
            ) for cmd in SD_CMDS))

    #
    # Make comprehensive structures for storing the fields of the SD card's registers.
    #

    for register_name, register in SD_REGISTERS.items():
        with Meta.enter(f'struct {register_name}'):

            # Simply list the fields if there's only one version.
            if register.version_field is None:
                Meta.line(f'{field.struct_type} {field.struct_name};' for field in register.layouts[None])
                continue

            # Version field determines which version of the register should be used.
            Meta.line(f'enum {register_name}Version {register.version_field.name};')

            # Make the fields from different versions of the register overlap in memory.
            with Meta.enter('union'):
                for version, fields in register.layouts.items():
                    with Meta.enter('struct'):
                        Meta.line(f'{field.struct_type} {field.struct_name};' for field in fields)
*/



//////////////////////////////////////////////////////////////// sd_initer.c ////////////////////////////////////////////////////////////////



enum SDIniterErr : u32
{
    SDIniterErr_none,
    SDIniterErr_bug,
    SDIniterErr_sorry,
};

enum SDCardState : u32 // @/pg 148/tbl 4-42/`SD`.
{
    SDCardState_idle  = 0,
    SDCardState_ready = 1,
    SDCardState_ident = 2, // "Identification".
    SDCardState_stby  = 3, // "Stand-by".
    SDCardState_tran  = 4, // "Transfer".
    SDCardState_data  = 5,
    SDCardState_rcv   = 6, // "Receive-data".
    SDCardState_prg   = 7, // "Programming".
    SDCardState_dis   = 8, // "Disconnect".
};

enum SDIniterState : u32
{
    SDIniterState_uninited,
    SDIniterState_GO_IDLE_STATE,
    SDIniterState_SEND_IF_COND,
    SDIniterState_SD_SEND_OP_COND,
    SDIniterState_ALL_SEND_CID,
    SDIniterState_SEND_RELATIVE_ADDR,
    SDIniterState_SEND_CSD,
    SDIniterState_SELECT_DESELECT_CARD,
    SDIniterState_SEND_SCR,
    SDIniterState_SWITCH_FUNC,
    SDIniterState_SET_BUS_WIDTH,
    SDIniterState_caller_set_bus_width,
    SDIniterState_done,
};

struct SDIniter
{
    enum SDIniterState state;
    u16                rca; // Relative card address. @/pg 67/sec 4.2.2/`SD`.
    i32                capacity_sector_count;

    struct
    {
        i32 mount_attempts;
        i32 support_attempts;
        u8  sd_config_reg[8];
        u8  switch_func_status[64];
    } local;

    struct SDIniterFeedback // To be filled out by the caller before updating with the results of the previous command.
    {
        b32 cmd_failed;
        u32 response[4]; // Card command response with the first word being the most-significant bits of the response.
    } feedback;

    struct SDIniterCmd
    {
        enum SDCmd cmd;
        u32        arg;
        u8*        data;
        i32        size;
    } cmd;
};
