#define SD_INITER_MAX_MOUNTING_RETRIES  64
#define SD_INITER_MAX_READY_RETRIES     1024
#define SD_INITER_MAX_BUS_ERROR_RETRIES 64



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
    SDIniterState_execute_GO_IDLE_STATE,
    SDIniterState_execute_SEND_IF_COND,
    SDIniterState_execute_SD_SEND_OP_COND,
    SDIniterState_execute_ALL_SEND_CID,
    SDIniterState_execute_SEND_RELATIVE_ADDR,
    SDIniterState_execute_SEND_CSD,
    SDIniterState_execute_SELECT_DESELECT_CARD,
    SDIniterState_execute_SEND_SCR,
    SDIniterState_execute_SWITCH_FUNC,
    SDIniterState_execute_SET_BUS_WIDTH,
    SDIniterState_user_set_bus_width,
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
        i32 bus_attempts;
        u8  sd_configuration_register[8];
        u8  switch_function_status[64];
    } local;

    struct SDIniterCommand
    {
        enum SDCmd cmd;
        u32        arg;
        u8*        data;
        i32        size;
    } command;

    struct SDIniterFeedback // @/`SD Initer Feedback System`.
    {
        b32 failed;
        u32 response[4]; // Begins with the first 32 most-significant bits.
    } feedback;

};



////////////////////////////////////////////////////////////////////////////////



static useret enum SDIniterHandleFeedbackResult : u32
{
    SDIniterHandleFeedbackResult_okay,
    SDIniterHandleFeedbackResult_card_likely_unmounted,
    SDIniterHandleFeedbackResult_voltage_check_failed,
    SDIniterHandleFeedbackResult_could_not_ready_card,
    SDIniterHandleFeedbackResult_unsupported_card,
    SDIniterHandleFeedbackResult_maybe_bus_problem,
    SDIniterHandleFeedbackResult_card_glitch,
    SDIniterHandleFeedbackResult_bug,
}
_SDIniter_handle_feedback(struct SDIniter* initer)
{

    if (!initer)
        return SDIniterHandleFeedbackResult_bug;

    switch (initer->state)
    {


        ////////////////////////////////////////
        //
        // This is the beginning of the SD card initializer state machine.
        //
        // @/pg 67/sec 4.2/`SD`.
        //
        ////////////////////////////////////////

        case SDIniterState_uninited:
        {
            if (initer->feedback.failed)
            {
                return SDIniterHandleFeedbackResult_bug; // There's no reason for a failed command already...
            }
            else
            {

                *initer =
                    (struct SDIniter)
                    {

                        .state = SDIniterState_execute_GO_IDLE_STATE,
                    };

                return SDIniterHandleFeedbackResult_okay;

            }
        } break;



        ////////////////////////////////////////
        //
        // Make SD card reset into the idle state.
        //
        // @/pg 68/fig 4-1/`SD`.
        //
        ////////////////////////////////////////

        case SDIniterState_execute_GO_IDLE_STATE:
        {
            if (initer->feedback.failed)
            {
                return SDIniterHandleFeedbackResult_bug; // There's no reason for the GO_IDLE_STATE command to fail.
            }
            else
            {
                initer->state = SDIniterState_execute_SEND_IF_COND;
                return SDIniterHandleFeedbackResult_okay;
            }
        } break;



        ////////////////////////////////////////
        //
        // Send the mandatory voltage-check command.
        //
        // @/pg 67/sec 4.2.2/`SD`.
        //
        ////////////////////////////////////////

        #define SD_INITER_INTERFACE_CONDITION ( /* Argument to SEND_IF_COND (CMD8). @/pg 116/sec 4.3.13/`SD`. */ \
                (0      << (21 - 8)) |          /* We don't support the PCIe 1.2v bus.                        */ \
                (0      << (20 - 8)) |          /* We don't have PCIe availability.                           */ \
                (0b0001 << (16 - 8)) |          /* We're supplying 2.7-3.6v.                                  */ \
                (0xAA   << ( 8 - 8))            /* Arbitrary check-pattern.                                   */ \
            )

        case SDIniterState_execute_SEND_IF_COND:
        {
            if (initer->feedback.failed)
            {
                if (initer->local.mount_attempts < SD_INITER_MAX_MOUNTING_RETRIES)
                {

                    initer->local.mount_attempts += 1;
                    initer->state                 = SDIniterState_execute_GO_IDLE_STATE;

                    return SDIniterHandleFeedbackResult_okay; // Try restarting and mounting again...

                }
                else
                {
                    return SDIniterHandleFeedbackResult_card_likely_unmounted;
                }
            }
            else if (initer->feedback.response[0] != (SD_INITER_INTERFACE_CONDITION & ((1 << 12) - 1)))
            {

                // Response of SEND_IF_COND must echo back the expected voltage
                // and check-pattern; we got something different, so...
                //
                // @/pg 116/sec 4.3.13/`SD`."

                return SDIniterHandleFeedbackResult_voltage_check_failed;

            }
            else // SD card successfully replied back!
            {
                initer->local.mount_attempts = 0;
                initer->state                = SDIniterState_execute_SD_SEND_OP_COND;
                return SDIniterHandleFeedbackResult_okay;
            }
        } break;



        ////////////////////////////////////////
        //
        // Attempt to move from idle state into ready state.
        //
        ////////////////////////////////////////

        #define SD_INITER_OPERATING_CONDITION (       /* Argument to SD_SEND_OP_COND (ACMD41) @/pg 71/fig 4-3/`SD`.  */ \
                (1 << 30) |                           /* We support high-capacity cards.                             */ \
                (1 << 28) |                           /* XPC bit for higher performance at the cost of power usage.  */ \
                0b00000000'11111111'10000000'00000000 /* Our voltage window range is 2.7-3.6V. @/pg 249/tbl 5-1/`SD` */ \
            )

        case SDIniterState_execute_SD_SEND_OP_COND:
        {

            u32 ocr        = initer->feedback.response[0]; // @/pg 71/fig 4-4/`SD`.
            b32 powered_up = ocr & (1U << 31);             // @/pg 249/tbl 5-1/`SD`.
            b32 ccs        = ocr & (1U << 30);             // "CCS=0 is SDSC and CCS=1 is SDHC or SDXC". @/pg 117/sec 4.3.14/`SD`.

            if (initer->feedback.failed || !powered_up) // Couldn't ready the card?
            {
                if (initer->local.mount_attempts < SD_INITER_MAX_READY_RETRIES)
                {

                    initer->local.mount_attempts += 1;
                    initer->state                 = SDIniterState_execute_SD_SEND_OP_COND;

                    return SDIniterHandleFeedbackResult_okay; // Give the card a bit more time...

                }
                else
                {
                    return SDIniterHandleFeedbackResult_could_not_ready_card;
                }
            }
            else if (!ccs)
            {

                // We don't support standard capacity SD cards; if we were to,
                // we'd need to account for the byte unit addressing.
                //
                // @/pg 130/tbl 4-23/`SD`.

                return SDIniterHandleFeedbackResult_unsupported_card;

            }
            else
            {
                initer->local.mount_attempts = 0;
                initer->state                = SDIniterState_execute_ALL_SEND_CID;
                return SDIniterHandleFeedbackResult_okay;
            }

        } break;



        ////////////////////////////////////////
        //
        // Get the unique card identification number (CID).
        //
        // @/pg 69/sec 4.2.3/`SD`.
        //
        ////////////////////////////////////////

        case SDIniterState_execute_ALL_SEND_CID:
        {
            if (initer->feedback.failed)
            {
                return SDIniterHandleFeedbackResult_maybe_bus_problem; // Probably due to something outside our control.
            }
            else
            {
                initer->state = SDIniterState_execute_SEND_RELATIVE_ADDR;
                return SDIniterHandleFeedbackResult_okay;
            }
        } break;



        ////////////////////////////////////////
        //
        // Get the relative card address (RCA) to bring the card into standby state;
        // it's also needed for certain SD commands.
        //
        // @/pg 69/sec 4.2.3/`SD`.
        //
        ////////////////////////////////////////

        case SDIniterState_execute_SEND_RELATIVE_ADDR:
        {
            if (initer->feedback.failed)
            {
                return SDIniterHandleFeedbackResult_maybe_bus_problem; // Probably due to something outside our control.
            }
            else if (((initer->feedback.response[0] >> 9) & 0b1111) != SDCardState_ident)
            {
                return SDIniterHandleFeedbackResult_card_glitch; // SD card should've came out of the identification state...
            }
            else
            {
                initer->rca   = (u16) (initer->feedback.response[0] >> 16); // @/pg 144/sec 4.9.5/`SD`.
                initer->state = SDIniterState_execute_SEND_CSD;
                return SDIniterHandleFeedbackResult_okay;
            }
        } break;



        ////////////////////////////////////////
        //
        // Get and verify card-specific data.
        //
        // @/pg 251/sec 5.3.1/`SD`.
        //
        ////////////////////////////////////////

        case SDIniterState_execute_SEND_CSD:
        {
            if (initer->feedback.failed)
            {
                return SDIniterHandleFeedbackResult_maybe_bus_problem; // Probably due to something outside our control.
            }
            else
            {

                union
                {
                    u32 words[4];
                    u8  bytes[4 * sizeof(u32)];
                } card_specific_data_data =
                    {
                        {
                            __builtin_bswap32(initer->feedback.response[0]),
                            __builtin_bswap32(initer->feedback.response[1]),
                            __builtin_bswap32(initer->feedback.response[2]),
                            __builtin_bswap32(initer->feedback.response[3]),
                        },
                    };

                struct SDCardSpecificData csd = {0};

                if (!SD_parse_register(&csd, card_specific_data_data.bytes))
                {
                    return SDIniterHandleFeedbackResult_card_glitch; // Register has invalid values.
                }
                else switch (csd.CSD_STRUCTURE)
                {

                    case SDCardSpecificDataVersion_2:
                    {
                        if (~csd.v2_CCC & ((1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 8)))
                        {
                            return SDIniterHandleFeedbackResult_card_glitch; // Missing mandatory classes. @/pg 123/sec 4.7.3/`SD`.
                        }
                        else
                        {
                            initer->capacity_sector_count = ((i32) csd.v2_C_SIZE + 1) * 1024; // @/pg 260/sec 5.3.3/`SD`.
                            initer->state                 = SDIniterState_execute_SELECT_DESELECT_CARD;
                            return SDIniterHandleFeedbackResult_okay;
                        }
                    } break;

                    default: return SDIniterHandleFeedbackResult_unsupported_card;

                }

            }
        } break;



        ////////////////////////////////////////
        //
        // Go into transfer state.
        //
        ////////////////////////////////////////

        case SDIniterState_execute_SELECT_DESELECT_CARD:
        {
            if (initer->feedback.failed)
            {
                return SDIniterHandleFeedbackResult_maybe_bus_problem; // Probably due to something outside our control.
            }
            else if (((initer->feedback.response[0] >> 9) & 0b1111) != SDCardState_stby)
            {
                return SDIniterHandleFeedbackResult_card_glitch; // SD card should've came out of the stand-by state...
            }
            else
            {
                initer->state = SDIniterState_execute_SEND_SCR;
                return SDIniterHandleFeedbackResult_okay;
            }
        } break;



        ////////////////////////////////////////
        //
        // Get and verify SD configuration register (SCR).
        //
        ////////////////////////////////////////

        case SDIniterState_execute_SEND_SCR:
        {
            if (initer->feedback.failed)
            {
                return SDIniterHandleFeedbackResult_maybe_bus_problem; // Probably due to something outside our control.
            }
            else if (((initer->feedback.response[0] >> 9) & 0b1111) != SDCardState_tran)
            {
                return SDIniterHandleFeedbackResult_card_glitch; // SD card should've came out of the transmission state...
            }
            else
            {

                struct SDConfigurationRegister scr = {0};

                if (!SD_parse_register(&scr, initer->local.sd_configuration_register))
                {
                    return SDIniterHandleFeedbackResult_card_glitch; // Register has invalid values.
                }
                else switch (scr.SCR_STRUCTURE)
                {

                    case SDConfigurationRegisterVersion_1:
                    {
                        if (scr.v1_SD_SPEC != 2) // @/pg 267/tbl 5-19/`SD`.
                        {
                            return SDIniterHandleFeedbackResult_unsupported_card; // Unsupported specification version.
                        }
                        else if (~scr.v1_SD_BUS_WIDTHS & ((1 << 0) | (1 << 2))) // @/pg 270/tbl 5-21/`SD`.
                        {
                            return SDIniterHandleFeedbackResult_unsupported_card; // We need 1-bit and 4-bit data busses.
                        }
                        else
                        {
                            initer->state = SDIniterState_execute_SWITCH_FUNC;
                            return SDIniterHandleFeedbackResult_okay;
                        }
                    } break;

                    default: return SDIniterHandleFeedbackResult_unsupported_card;

                }

            }
        } break;



        ////////////////////////////////////////
        //
        // Ensure the card supports high-speed mode so we can go up to 48MHz.
        // @/pg 106/sec 4.3.10.3/`SD`.
        //
        ////////////////////////////////////////

        case SDIniterState_execute_SWITCH_FUNC:
        {
            if (initer->feedback.failed)
            {
                return SDIniterHandleFeedbackResult_maybe_bus_problem; // Probably due to something outside our control.
            }
            else
            {

                struct SDSwitchFunctionStatus sfs = {0};

                if (!SD_parse_register(&sfs, initer->local.switch_function_status))
                {
                    return SDIniterHandleFeedbackResult_card_glitch; // Register has invalid values.
                }
                else switch (sfs.version)
                {

                    case SDSwitchFunctionStatusVersion_1:
                    case SDSwitchFunctionStatusVersion_2:
                    {
                        if (~sfs.v2_group_1_support & (1 << 1)) // @/pg 111/tbl 4-13/`SD`.
                        {
                            return SDIniterHandleFeedbackResult_unsupported_card; // We need high-speed mode.
                        }
                        else if (sfs.v2_group_1_selection != 0x1) // @/pg 107/tbl 4-11/`SD`.
                        {
                            return SDIniterHandleFeedbackResult_unsupported_card; // We failed to switch to high-speed mode.
                        }
                        else
                        {
                            initer->state = SDIniterState_execute_SET_BUS_WIDTH;
                            return SDIniterHandleFeedbackResult_okay;
                        }
                    } break;

                    default: return SDIniterHandleFeedbackResult_unsupported_card;

                }

            }
        } break;



        ////////////////////////////////////////
        //
        // Notify the SD card that the bus width will be set to 4.
        //
        // @/pg 133/tbl 4-31/`SD`.
        //
        ////////////////////////////////////////

        case SDIniterState_execute_SET_BUS_WIDTH:
        {
            if (initer->feedback.failed)
            {
                return SDIniterHandleFeedbackResult_maybe_bus_problem; // Probably due to something outside our control.
            }
            else if (((initer->feedback.response[0] >> 9) & 0b1111) != SDCardState_tran)
            {
                return SDIniterHandleFeedbackResult_card_glitch; // SD card should've came out of the transfer state...
            }
            else
            {
                initer->state = SDIniterState_user_set_bus_width;
                return SDIniterHandleFeedbackResult_okay;
            }
        } break;



        ////////////////////////////////////////
        //
        // Caller now configures the bus width.
        //
        ////////////////////////////////////////

        case SDIniterState_user_set_bus_width:
        {
            if (initer->feedback.failed)
            {
                return SDIniterHandleFeedbackResult_bug; // No reason for the user to fail configuring the bus width.
            }
            else
            {
                initer->state = SDIniterState_done;
                return SDIniterHandleFeedbackResult_okay;
            }
        } break;



        case SDIniterState_done : return SDIniterHandleFeedbackResult_bug; // We're already done; there's nothing left to do!
        default                 : return SDIniterHandleFeedbackResult_bug;

    }

}



////////////////////////////////////////////////////////////////////////////////



static useret enum SDIniterUpdateResult : u32
{
    SDIniterUpdateResult_execute_command,
    SDIniterUpdateResult_done,
    SDIniterUpdateResult_user_set_bus_width,
    SDIniterUpdateResult_maybe_bus_problem,
    SDIniterUpdateResult_voltage_check_failed,
    SDIniterUpdateResult_could_not_ready_card,
    SDIniterUpdateResult_unsupported_card,
    SDIniterUpdateResult_card_glitch,
    SDIniterUpdateResult_card_likely_unmounted,
    SDIniterUpdateResult_bug,
}
SDIniter_update(struct SDIniter* initer)
{

    if (!initer)
        return SDIniterUpdateResult_bug;



    initer->command = (struct SDIniterCommand) {0};



    // Handle feedback.

    enum SDIniterHandleFeedbackResult feedback_result = _SDIniter_handle_feedback(initer);

    switch (feedback_result)
    {

        case SDIniterHandleFeedbackResult_maybe_bus_problem:
        {
            if (initer->local.bus_attempts < SD_INITER_MAX_BUS_ERROR_RETRIES)
            {
                initer->local.bus_attempts += 1;
                initer->state               = SDIniterState_execute_GO_IDLE_STATE;
            }
            else
            {
                return SDIniterUpdateResult_maybe_bus_problem;
            }
        } break;

        case SDIniterHandleFeedbackResult_okay                  : break;
        case SDIniterHandleFeedbackResult_card_likely_unmounted : return SDIniterUpdateResult_card_likely_unmounted;
        case SDIniterHandleFeedbackResult_voltage_check_failed  : return SDIniterUpdateResult_voltage_check_failed;
        case SDIniterHandleFeedbackResult_could_not_ready_card  : return SDIniterUpdateResult_could_not_ready_card;
        case SDIniterHandleFeedbackResult_unsupported_card      : return SDIniterUpdateResult_unsupported_card;
        case SDIniterHandleFeedbackResult_card_glitch           : return SDIniterUpdateResult_card_glitch;
        case SDIniterHandleFeedbackResult_bug                   : return SDIniterUpdateResult_bug;
        default                                                 : return SDIniterUpdateResult_bug;

    }



    // Determine what the user should do next.

    switch (initer->state)
    {

        case SDIniterState_execute_GO_IDLE_STATE:
        {

            initer->command =
                (struct SDIniterCommand)
                {
                    .cmd = SDCmd_GO_IDLE_STATE,
                };

            return SDIniterUpdateResult_execute_command;

        } break;

        case SDIniterState_execute_SEND_IF_COND:
        {

            initer->command =
                (struct SDIniterCommand)
                {
                    .cmd = SDCmd_SEND_IF_COND,
                    .arg = SD_INITER_INTERFACE_CONDITION,
                };

            return SDIniterUpdateResult_execute_command;

        } break;

        case SDIniterState_execute_ALL_SEND_CID:
        {

            initer->command =
                (struct SDIniterCommand)
                {
                    .cmd = SDCmd_ALL_SEND_CID,
                };

            return SDIniterUpdateResult_execute_command;

        } break;

        case SDIniterState_execute_SEND_RELATIVE_ADDR:
        {

            initer->command =
                (struct SDIniterCommand)
                {
                    .cmd = SDCmd_SEND_RELATIVE_ADDR,
                };

            return SDIniterUpdateResult_execute_command;

        } break;

        case SDIniterState_execute_SEND_CSD:
        {

            initer->command =
                (struct SDIniterCommand)
                {
                    .cmd = SDCmd_SEND_CSD,
                    .arg = initer->rca << 16,
                };

            return SDIniterUpdateResult_execute_command;

        } break;

        case SDIniterState_execute_SELECT_DESELECT_CARD:
        {

            initer->command =
                (struct SDIniterCommand)
                {
                    .cmd = SDCmd_SELECT_DESELECT_CARD,
                    .arg = initer->rca << 16,
                };

            return SDIniterUpdateResult_execute_command;

        } break;

        case SDIniterState_execute_SET_BUS_WIDTH:
        {

            initer->command =
                (struct SDIniterCommand)
                {
                    .cmd = SDCmd_SET_BUS_WIDTH,
                    .arg = 0b10, // @/pg 133/tbl 4-32/`SD`.
                };

            return SDIniterUpdateResult_execute_command;

        } break;

        case SDIniterState_execute_SD_SEND_OP_COND:
        {

            initer->command =
                (struct SDIniterCommand)
                {
                    .cmd = SDCmd_SD_SEND_OP_COND,
                    .arg = SD_INITER_OPERATING_CONDITION,
                };

            return SDIniterUpdateResult_execute_command;

        } break;

        case SDIniterState_execute_SEND_SCR:
        {

            initer->command =
                (struct SDIniterCommand)
                {
                    .cmd  = SDCmd_SEND_SCR,
                    .arg  = 0,
                    .data = initer->local.sd_configuration_register,
                    .size = sizeof(initer->local.sd_configuration_register),
                };

            return SDIniterUpdateResult_execute_command;

        } break;

        case SDIniterState_execute_SWITCH_FUNC:
        {

            initer->command =
                (struct SDIniterCommand)
                {
                    .cmd = SDCmd_SWITCH_FUNC,
                    .arg =
                        (
                            (1U  << 31) | // We're going to change some functions around... @/pg 136/tbl 4-32/`SD`.
                            (0xF << 12) | // No influence on power-limit group. @/pg 107/tbl 4-11/`SD`.
                            (0xF <<  8) | // No influence on driver-strength group.
                            (0x1 <<  0)   // We want high-speed! @/pg 115/sec 4.3.11/`SD`.
                        ),
                    .data = initer->local.switch_function_status,
                    .size = sizeof(initer->local.switch_function_status),
                };

            return SDIniterUpdateResult_execute_command;

        } break;

        case SDIniterState_user_set_bus_width : return SDIniterUpdateResult_user_set_bus_width;
        case SDIniterState_done               : return SDIniterUpdateResult_done;
        case SDIniterState_uninited           : return SDIniterUpdateResult_bug;
        default                               : return SDIniterUpdateResult_bug;

    }

}



////////////////////////////////////////////////////////////////////////////////



// @/`SD Initer Feedback System`:
//
// The purpose of the SD initer is to have a state machine that
// initializes the SD card without worrying about the control flow
// of executing the SD commands.
//
// The principle of operation for the SD initer is that the user
// invokes `SDIniter_update` to be told what to do, which is often
// the SD command to execute. Information like which specific command,
// the argument for it, how much data to transfer, etc. is provided.
//
// It is up to the user to figure out how to exactly carry out that SD
// command. Like for instance, it is not the SD initer's concern that
// a particular SD command is an application-specific command (ACMDx),
// which is a command that needs to be prefixed with another SD command.
// The user has to handle this themselves.
//
// After the execution of the SD command, the user provides the SD initer
// with the results, like whether or not the command was successful. From
// here, the SD initer can take this "feedback" and decide what to do next.
//
// With this design, the SD initer is an entirely pure state machine.
// This then means it's pretty much hardware independent, and open
// itself to being easily ported other MCUs with slightly different
// hardware setup for communicating with SD cards.
