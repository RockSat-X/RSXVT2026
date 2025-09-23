#include "SD_parse_register.meta"
/* #meta

    from deps.stpy.pxd.utils import justify

    #
    # Generic procedure.
    #

    with Meta.enter('#define SD_parse_register(DST, SRC)'):
        with Meta.enter(
            '_Generic',
            '(',
            ')((DST), (SRC))'
        ):
            Meta.line('(DST),')
            Meta.line(',\n'.join(
                f'struct {register_name}* : _SD_parse_{register_name}'
                for register_name, register in SD_REGISTERS.items()
            ))

    #
    # Specialized procedures.
    #

    for register_name, register in SD_REGISTERS.items():

        with Meta.enter(f'''
            static useret b32
            _SD_parse_{register_name}(struct {register_name}* dst, u8* src)
        '''):

            def parse(version):

                # Fields that will be retrieved and checked.
                assignments = []

                if version is not None:
                    assignments += [(register.version_field.name, register.version_field.bits, None)]

                assignments += [
                    (field.struct_name, field.bits, field.expected)
                    for field in register.layouts[version]
                ]

                # Get the fields.
                with Meta.enter('*dst =', f'(struct {register_name})\n{{', '};', indented = True):
                    for columns in justify(
                        (
                            ('<', name),
                            ('<', bits),
                        )
                        for name, bits, expected in assignments
                    ):
                        Meta.line('.{0} = {1},'.format(*columns))

                # Assert the fields.
                for columns in justify(
                    (
                        ('<', name    ),
                        ('<', expected),
                    )
                    for name, bits, expected in assignments
                    if expected is not None
                ):
                    Meta.line('if (dst->{0} != {1}) return false;'.format(*columns))

            # Parsing a register with only one layout.
            if register.version_field is None:
                parse(None)

            # Parsing a register with multiple layouts.
            else:

                Meta.line(f'memzero(dst);')

                with Meta.enter(f'switch ({register.version_field.bits})'):

                    for version in register.layouts:
                        with Meta.enter(f'case {register_name}Version_{version}:'):
                            parse(version)

                    Meta.line('default: return false;')

            Meta.line('return true;')
*/

static useret enum SDIniterHandleFeedbackRet : u32
{
    SDIniterHandleFeedbackRet_ok,
    SDIniterHandleFeedbackRet_bug,
    SDIniterHandleFeedbackRet_card_likely_unmounted,
    SDIniterHandleFeedbackRet_support_issue,
}
_SD_initer_handle_feedback(struct SDIniter* initer)
{
    #undef  ret
    #define ret(KIND) ret_mk(SDIniterHandleFeedbackRet, KIND)

    ret_if(bug, !initer);

    initer->cmd = (struct SDIniterCmd) {0};

    switch (initer->state)
    {
        //
        // This is the beginning of the SD card initializer state machine. @/pg 67/sec 4.2/`SD`.
        //

        case SDIniterState_uninited:
        {

            ret_if(bug, initer->feedback.cmd_failed); // Feedback said SD command failed, but SD-initer hasn't given the caller any SD commands yet.

            initer->state = SDIniterState_GO_IDLE_STATE;

        } break;

        //
        // Make card reset into the idle state. @/pg 68/fig 4-1/`SD`.
        //

        case SDIniterState_GO_IDLE_STATE:
        {

            ret_if(support_issue, initer->feedback.cmd_failed);

            initer->state = SDIniterState_SEND_IF_COND;

        } break;

        //
        // Send the mandatory voltage-check command. @/pg 67/sec 4.2.2/`SD`.
        //

        #define SD_INTERFACE_CONDITION ( /* Argument to SEND_IF_COND (CMD8). @/pg 116/sec 4.3.13/`SD`. */ \
                (0      << (21 - 8)) |   /* We don't support the PCIe 1.2v bus.                        */ \
                (0      << (20 - 8)) |   /* We don't have PCIe availability.                           */ \
                (0b0001 << (16 - 8)) |   /* We're supplying 2.7-3.6v.                                  */ \
                (0xAA   << ( 8 - 8))     /* Arbitrary check-pattern.                                   */ \
            )

        case SDIniterState_SEND_IF_COND:
        {

            if (initer->feedback.cmd_failed)
            {
                ret_if(card_likely_unmounted, initer->local.mount_attempts + 1 >= 64);

                initer->local.mount_attempts += 1;
                initer->state                 = SDIniterState_GO_IDLE_STATE; // Try again.
            }
            else
            {
                // Response of SEND_IF_COND must echo back the expected voltage and check-pattern. @/pg 116/sec 4.3.13/`SD`."
                ret_if(support_issue, initer->feedback.response[0] != (SD_INTERFACE_CONDITION & ((1 << 12) - 1)));

                initer->local.mount_attempts = 0;
                initer->state                = SDIniterState_SD_SEND_OP_COND;
            }

        } break;

        //
        // Attempt to move from idle state into ready state.
        //

        case SDIniterState_SD_SEND_OP_COND:
        {

            u32 ocr        = initer->feedback.response[0]; // @/pg 71/fig 4-4/`SD`.
            b32 powered_up = ocr & (1 << 31);              // @/pg 249/tbl 5-1/`SD`.

            if (initer->feedback.cmd_failed || !powered_up) // Couldn't ready the card?
            {
                ret_if(card_likely_unmounted, initer->local.mount_attempts + 1 >= 1024);

                initer->local.mount_attempts += 1;
                initer->state                 = SDIniterState_SD_SEND_OP_COND; // Try again.
            }
            else
            {
                // Check the CCS bit where "CCS=0 is SDSC and CCS=1 is SDHC or SDXC". @/pg 117/sec 4.3.14/`SD`.
                // We don't support SDSC; if we were to, we'd need to account for the byte unit addressing. @/pg 130/tbl 4-23/`SD`.
                ret_if(support_issue, !(ocr & (1 << 30)));

                initer->local.mount_attempts = 0;
                initer->state                = SDIniterState_ALL_SEND_CID;
            }

        } break;

        //
        // Get the unique card identification number (CID), @/pg 69/sec 4.2.3/`SD`.
        //

        case SDIniterState_ALL_SEND_CID:
        {

            ret_if(support_issue, initer->feedback.cmd_failed);

            initer->state = SDIniterState_SEND_RELATIVE_ADDR; // The card is now in the identification state.

        } break;

        //
        // Get the relative card address (RCA) to bring the card into standby state. @/pg 69/sec 4.2.3/`SD`.
        //

        case SDIniterState_SEND_RELATIVE_ADDR:
        {

            ret_if(support_issue, initer->feedback.cmd_failed);
            ret_if(support_issue, ((initer->feedback.response[0] >> 9) & 0b1111) != SDCardState_ident); // Transitioned out of the identification state.

            initer->rca   = initer->feedback.response[0] >> 16; // The RCA is needed for some commands. @/pg 144/sec 4.9.5/`SD`.
            initer->state = SDIniterState_SEND_CSD;

        } break;

        //
        // Get and verify card-specific data. @/pg 251/sec 5.3.1/`SD`.
        //

        case SDIniterState_SEND_CSD:
        {

            ret_if(support_issue, initer->feedback.cmd_failed);

            union
            {
                u32 words[4];
                u8  bytes[4 * sizeof(u32)];
            } card_specific_data_data = {{
                __builtin_bswap32(initer->feedback.response[0]),
                __builtin_bswap32(initer->feedback.response[1]),
                __builtin_bswap32(initer->feedback.response[2]),
                __builtin_bswap32(initer->feedback.response[3]),
            }};

            struct SDCardSpecificData csd = {0};
            ret_if(support_issue, !SD_parse_register(&csd, card_specific_data_data.bytes));

            switch (csd.CSD_STRUCTURE)
            {
                case SDCardSpecificDataVersion_2:
                {
                    // Classes 0, 2, 4, 5, and 8 are mandatory. @/pg 123/sec 4.7.3/`SD`.
                    ret_if(support_issue, ~csd.v2_CCC & ((1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 8)));

                    // Memory capacity of the SD card in units of 512-byte sectors. @/pg 260/sec 5.3.3/`SD`.
                    initer->capacity_sector_count = (csd.v2_C_SIZE + 1) * 1024;
                } break;

                default: ret(support_issue);
            }

            initer->state = SDIniterState_SELECT_DESELECT_CARD;

        } break;

        //
        // Go into transfer state.
        //

        case SDIniterState_SELECT_DESELECT_CARD:
        {

            ret_if(support_issue, initer->feedback.cmd_failed);
            ret_if(support_issue, ((initer->feedback.response[0] >> 9) & 0b1111) != SDCardState_stby); // Transitioned out of the stand-by state. @/pg 76/fig 4-13/`SD`.

            initer->state = SDIniterState_SEND_SCR;

        } break;

        //
        // Get and verify SD configuration register (SCR).
        //

        case SDIniterState_SEND_SCR:
        {

            struct SDConfigurationRegister scr = {0};

            ret_if(support_issue, initer->feedback.cmd_failed);
            ret_if(support_issue, ((initer->feedback.response[0] >> 9) & 0b1111) != SDCardState_tran); // Transitioned out of the transmission state. @/pg 76/fig 4-13/`SD`.
            ret_if(support_issue, !SD_parse_register(&scr, initer->local.sd_config_reg));

            switch (scr.SCR_STRUCTURE)
            {
                case SDConfigurationRegisterVersion_1:
                {
                    // Only SD cards with version 2.00 of the physical layer specification are currently supported. @/pg 267/tbl 5-19/`SD`
                    ret_if(support_issue, scr.v1_SD_SPEC != 2);

                    // Only SD cards with 1-bit and 4-bit busses are currently supported. @/pg 270/tbl 5-21/`SD`.
                    ret_if(support_issue, ~scr.v1_SD_BUS_WIDTHS & ((1 << 0) | (1 << 2)));
                } break;

                default: ret(support_issue);
            }

            initer->state = SDIniterState_SWITCH_FUNC;

        } break;

        //
        // Ensure the card supports high-speed mode so we can go up to 48MHz. @/pg 106/sec 4.3.10.3/`SD`.
        //

        case SDIniterState_SWITCH_FUNC:
        {

            struct SDSwitchFunctionStatus sfs = {0};

            ret_if(support_issue, initer->feedback.cmd_failed);
            ret_if(support_issue, !SD_parse_register(&sfs, initer->local.switch_func_status));

            switch (sfs.version)
            {
                case SDSwitchFunctionStatusVersion_1:
                case SDSwitchFunctionStatusVersion_2:
                {
                    // Only SD cards with high-speed mode are currently supported. @/pg 111/tbl 4-13/`SD`.
                    ret_if(support_issue, ~sfs.v2_group_1_support & (1 << 1));

                    // SD cards couldn't be switched to high-speed mode. @/pg 107/tbl 4-11/`SD`.
                    ret_if(support_issue, sfs.v2_group_1_selection != 0x1);
                } break;

                default: ret(support_issue);
            }

            initer->state = SDIniterState_SET_BUS_WIDTH;

        } break;

        //
        // Notify the SD card that the bus width will be set to 4. @/pg 133/tbl 4-31/`SD`. TODO Customize bus width.
        //

        case SDIniterState_SET_BUS_WIDTH:
        {

            ret_if(support_issue, initer->feedback.cmd_failed);
            ret_if(support_issue, ((initer->feedback.response[0] >> 9) & 0b1111) != SDCardState_tran); // Transitioned out of the transfer state. @/pg 76/fig 4-13/`SD`.

            initer->state = SDIniterState_caller_set_bus_width;

        } break;

        //
        // Caller now configures the bus width.
        //

        case SDIniterState_caller_set_bus_width:
        {

            ret_if(bug, initer->feedback.cmd_failed); // No reason for the caller to fail configuring the bus width.

            initer->state = SDIniterState_done;

        } break;

        case SDIniterState_done : ret(bug); // We're already done; there's nothing left to do!
        default                 : ret(bug);
    }

    ret(ok);
}

static useret enum SDIniterUpdateRet : u32
{
    SDIniterUpdateRet_do_cmd,
    SDIniterUpdateRet_caller_set_bus_width,
    SDIniterUpdateRet_card_likely_unmounted,
    SDIniterUpdateRet_card_likely_unsupported,
    SDIniterUpdateRet_done,
    SDIniterUpdateRet_bug,
}
_SD_initer_update(struct SDIniter* initer)
{
    #undef  ret
    #define ret(KIND) ret_mk(SDIniterUpdateRet, KIND)

    ret_if(bug, !initer);

    //
    // Handle feedback.
    //

    enum SDIniterHandleFeedbackRet feedback_ret = _SD_initer_handle_feedback(initer);
    switch (feedback_ret)
    {
        case SDIniterHandleFeedbackRet_support_issue:
        {
            ret_if(card_likely_unsupported, initer->local.support_attempts + 1 >= 64);

            initer->local.support_attempts += 1;
            initer->state                   = SDIniterState_GO_IDLE_STATE; // Redo the initialization again.
        } break;

        case SDIniterHandleFeedbackRet_ok                    : break;
        case SDIniterHandleFeedbackRet_card_likely_unmounted : ret(card_likely_unmounted);
        case SDIniterHandleFeedbackRet_bug                   : ret(bug);
        default                                              : ret(bug);
    }

    //
    // Determine what the caller should do next.
    //

    switch (initer->state)
    {
        #undef  ret_do_cmd
        #define ret_do_cmd(...)                                     \
            do                                                      \
            {                                                       \
                initer->cmd = (struct SDIniterCmd) { __VA_ARGS__ }; \
                ret(do_cmd);                                        \
            }                                                       \
            while (false)

        case SDIniterState_GO_IDLE_STATE        : ret_do_cmd(.cmd = SDCmd_GO_IDLE_STATE       ,                              );
        case SDIniterState_SEND_IF_COND         : ret_do_cmd(.cmd = SDCmd_SEND_IF_COND        , .arg = SD_INTERFACE_CONDITION);
        case SDIniterState_ALL_SEND_CID         : ret_do_cmd(.cmd = SDCmd_ALL_SEND_CID        ,                              );
        case SDIniterState_SEND_RELATIVE_ADDR   : ret_do_cmd(.cmd = SDCmd_SEND_RELATIVE_ADDR  ,                              );
        case SDIniterState_SEND_CSD             : ret_do_cmd(.cmd = SDCmd_SEND_CSD            , .arg = initer->rca << 16     );
        case SDIniterState_SELECT_DESELECT_CARD : ret_do_cmd(.cmd = SDCmd_SELECT_DESELECT_CARD, .arg = initer->rca << 16     );
        case SDIniterState_SET_BUS_WIDTH        : ret_do_cmd(.cmd = SDCmd_SET_BUS_WIDTH       , .arg = 0b10                  ); // @/pg 133/tbl 4-32/`SD`.

        case SDIniterState_SD_SEND_OP_COND:
            ret_do_cmd
            (
                .cmd = SDCmd_SD_SEND_OP_COND,
                .arg =
                    (                                         // @/pg 71/fig 4-3/`SD`.
                        (1 << 30) |                           // We support high-capacity cards.
                        (1 << 28) |                           // XPC bit where we get a nice performance boost at the cost of power usage.
                        0b00000000'11111111'10000000'00000000 // Our voltage window range is 2.7-3.6V. @/pg 249/tbl 5-1/`SD`
                    ),
            );

        case SDIniterState_SEND_SCR:
            ret_do_cmd
            (
                .cmd  = SDCmd_SEND_SCR,
                .arg  = 0,
                .data = initer->local.sd_config_reg,
                .size = sizeof(initer->local.sd_config_reg),
            );

        case SDIniterState_SWITCH_FUNC:
            ret_do_cmd
            (
                .cmd = SDCmd_SWITCH_FUNC,
                .arg =
                    (
                        (1   << 31) | // We're going to change some functions around... @/pg 136/tbl 4-32/`SD`.
                        (0xF << 12) | // No influence on power-limit group. @/pg 107/tbl 4-11/`SD`.
                        (0xF <<  8) | // No influence on driver-strength group.
                        (0x1 <<  0)   // We want high-speed! @/pg 115/sec 4.3.11/`SD`.
                    ),
                .data = initer->local.switch_func_status,
                .size = sizeof(initer->local.switch_func_status),
            );

        case SDIniterState_caller_set_bus_width : ret(caller_set_bus_width);
        case SDIniterState_done                 : ret(done);
        case SDIniterState_uninited             : ret(bug);
        default                                 : ret(bug);
    }
}
