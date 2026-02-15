#define SDCMDER_LOG_UPDATE_ITERATION  false
#define SDCMDER_LOG_EXECUTED_COMMANDS false



#include "SDCmderState.meta"
/* #meta
    make_named_enums(
        '''
            ready_for_next_command
            scheduled_command
            transferring_acmd_prefix
            scheduled_amcd_prefixed_command
            transferring_user_command
            outwaiting_busy_signal_for_user_command
            undergoing_data_transfer
            scheduled_stop_transmission_for_error
            transferring_stop_transmission_for_error
            outwaiting_busy_signal_for_stop_transmission
        '''
    )
*/

#include "SDCmderError.meta"
/* #meta
    make_named_enums(
        '''
            none
            command_timeout
            data_timeout
            bad_crc
        '''
    )
*/

struct SDCmder
{
    enum SDCmderState state;
    enum SDCmderError error;
    enum SDCmd        cmd;
    u32               argument;
    u8*               data;
    i32               remaining;
    i32               block_size;
    u16               rca; // Relative card address. @/pg 67/sec 4.2.2/`SD`.
};



////////////////////////////////////////////////////////////////////////////////



static useret enum SDCmderIterateResult : u32
{
    SDCmderIterateResult_again,
    SDCmderIterateResult_yield,
    SDCmderIterateResult_ready_for_next_command,
    SDCmderIterateResult_command_attempted,
    SDCmderIterateResult_card_glitch,
    SDCmderIterateResult_bug = BUG_CODE,
}
_SDCmder_iterate_once(SDMMC_TypeDef* SDMMC, struct SDCmder* cmder)
{

    if (!SDMMC)
        bug;

    if (!cmder)
        bug;



    #include "SDMMCInterruptEvent.meta"
    /* #meta
        make_named_enums(
            '''
                none
                end_of_busy_signal
                cmd12_aborted_data_transfer
                completed_transfer
                command_sent_with_no_response_expected
                command_sent_with_good_response
                data_timeout
                command_timeout
                data_with_bad_crc
                command_with_bad_crc
            '''
        )
    */

    enum SDMMCInterruptEvent interrupt_event  = {0};
    u32                      interrupt_status = SDMMC->STA;
    u32                      interrupt_enable = SDMMC->MASK;

    if
    (
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, IDMABTC   ) || // "IDMA buffer transfer complete".
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, IDMATE    ) || // "IDMA transfer error".
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CKSTOP    ) || // "SDMMC_CK stopped in Voltage switch procedure".
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, VSWEND    ) || // Voltage switch critical timing section completion".
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, ACKTIMEOUT) || // "Boot acknowledge timeout".
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, ACKFAIL   ) || // "... (boot acknowledgment check fail)".
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, SDIOIT    ) || // "SDIO interrupt received".
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DHOLD     ) || // Used for freezing the DPSM.
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, RXOVERR   ) || // RX-FIFO got too full (shouldn't happen).
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, TXUNDERR  ) || // Hardware found TX-FIFO empty (shouldn't happen).
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DBCKEND   )    // Data block sent/received (mostly used for SD I/O).
    )
    {
        bug; // Shouldn't happen, mostly because the feature isn't even used.
    }



    // "end of SDMMC_D0 Busy following a CMD response detected".

    else if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, BUSYD0END))
    {
        CMSIS_SET(SDMMC, ICR, BUSYD0ENDC, true);
        interrupt_event = SDMMCInterruptEvent_end_of_busy_signal;
    }



    // "Data transfer aborted by CMD12".

    else if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DABORT))
    {
        CMSIS_SET(SDMMC, ICR, DABORTC, true);
        interrupt_event = SDMMCInterruptEvent_cmd12_aborted_data_transfer;
    }



    // "Comamnd sent (no response required)".

    else if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CMDSENT))
    {
        CMSIS_SET(SDMMC, ICR, CMDSENTC, true);
        interrupt_event = SDMMCInterruptEvent_command_sent_with_no_response_expected;
    }



    // "Command response received (CRC check passed, or no CRC)".

    else if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CMDREND))
    {
        CMSIS_SET(SDMMC, ICR, CMDRENDC, true);
        interrupt_event = SDMMCInterruptEvent_command_sent_with_good_response;
    }



    // "Command response received (CRC check failed)".

    else if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CCRCFAIL))
    {
        CMSIS_SET(SDMMC, ICR, CCRCFAILC, true);
        interrupt_event = SDMMCInterruptEvent_command_with_bad_crc;
    }



    // The SD card took too long to process our command.

    else if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CTIMEOUT))
    {
        CMSIS_SET(SDMMC, ICR, CTIMEOUTC, true);
        interrupt_event = SDMMCInterruptEvent_command_timeout;
    }



    // The SD card took too long to send the data.

    else if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DTIMEOUT))
    {
        CMSIS_SET(SDMMC, ICR, DTIMEOUTC, true);
        interrupt_event = SDMMCInterruptEvent_data_timeout;
    }



    // "Data transfer ended correctly".

    else if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DATAEND))
    {
        CMSIS_SET(SDMMC, ICR, DATAENDC, true);
        interrupt_event = SDMMCInterruptEvent_completed_transfer;
    }



    // "Data block sent/received (CRC check failed)".

    else if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DCRCFAIL))
    {
        CMSIS_SET(SDMMC, ICR, DCRCFAILC, true);
        interrupt_event = SDMMCInterruptEvent_data_with_bad_crc;
    }



    // Nothing notable happened.

    else
    {

        if (interrupt_status & interrupt_enable)
            bug; // There shouldn't be any leftover interrupt events.

        interrupt_event = SDMMCInterruptEvent_none;

    }



    #if SDCMDER_LOG_UPDATE_ITERATION
    log("(%s) :: (%s)", SDCmderState_TABLE[cmder->state].name, SDMMCInterruptEvent_TABLE[interrupt_event].name);
    #endif



    // Handle the interrupt event.

    switch (cmder->state)
    {


        ////////////////////////////////////////
        //
        // The SD-cmder has nothing to do.
        //
        ////////////////////////////////////////

        case SDCmderState_ready_for_next_command: switch (interrupt_event)
        {

            case SDMMCInterruptEvent_none:
            {
                return SDCmderIterateResult_ready_for_next_command; // Tell user that the SD-cmder is on stand-by.
            } break;

            case SDMMCInterruptEvent_end_of_busy_signal                     : bug;
            case SDMMCInterruptEvent_cmd12_aborted_data_transfer            : bug;
            case SDMMCInterruptEvent_completed_transfer                     : bug;
            case SDMMCInterruptEvent_command_sent_with_no_response_expected : bug;
            case SDMMCInterruptEvent_command_sent_with_good_response        : bug;
            case SDMMCInterruptEvent_data_timeout                           : bug;
            case SDMMCInterruptEvent_command_timeout                        : bug;
            case SDMMCInterruptEvent_data_with_bad_crc                      : bug;
            case SDMMCInterruptEvent_command_with_bad_crc                   : bug;
            default                                                         : bug;

        } break;



        ////////////////////////////////////////
        //
        // The SD-cmder needs to set up the next command to be done.
        //
        ////////////////////////////////////////

        case SDCmderState_scheduled_amcd_prefixed_command       :
        case SDCmderState_scheduled_command                     :
        case SDCmderState_scheduled_stop_transmission_for_error : switch (interrupt_event)
        {

            case SDMMCInterruptEvent_none:
            {

                if (!iff(cmder->error, cmder->state == SDCmderState_scheduled_stop_transmission_for_error))
                    bug; // Unexpected error or unset error condition.

                if (!iff(CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DPSMACT), cmder->state == SDCmderState_scheduled_stop_transmission_for_error))
                    bug; // The data-path state-machine unexpected active/inactive.

                if (cmder->cmd == SDCmd_APP_CMD)
                    bug; // APP_CMD is already handled automatically.

                if (cmder->cmd == SDCmd_STOP_TRANSMISSION)
                    bug; // STOP_TRANSMISSION is already handled automatically.

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CPSMACT))
                    bug; // The command-path state-machine shouldn't be active.

                if (CMSIS_GET(SDMMC, CMD, CPSMEN))
                    bug; // The command-path state-machine shouldn't still be enabled...



                // The actual command that we should do depends on certain
                // things like whether or not we should send the ACMD prefix first.

                enum SDCmd actual_cmd            = {0};
                u32        actual_argument       = {0};
                b32        actually_transferring = {0};

                if (cmder->state == SDCmderState_scheduled_stop_transmission_for_error)
                {

                    actual_cmd            = SDCmd_STOP_TRANSMISSION;
                    actual_argument       = 0; // No argument necessary.
                    actually_transferring = false;
                    cmder->state          = SDCmderState_transferring_stop_transmission_for_error;

                    CMSIS_SET
                    (
                        SDMMC  , DCTRL,
                        FIFORST, true , // Flush the FIFO of any buffered data once DPSM becomes inactive.
                    );

                }
                else if (SD_CMDS[cmder->cmd].acmd && cmder->state != SDCmderState_scheduled_amcd_prefixed_command)
                {
                    actual_cmd            = SDCmd_APP_CMD;            // We need to send the APP_CMD prefix first.
                    actual_argument       = ((u32) cmder->rca) << 16; // It's okay if RCA is zero during initialization. @/pg 67/sec 4.2.2/`SD`.
                    actually_transferring = false;                    // The APP_CMD itself won't have a data transfer stage.
                    cmder->state          = SDCmderState_transferring_acmd_prefix;
                }
                else
                {
                    actual_cmd            = cmder->cmd;
                    actual_argument       = cmder->argument;
                    actually_transferring = !!cmder->remaining;
                    cmder->state          = SDCmderState_transferring_user_command;
                }



                // Set up the data-path state-machine.

                if (actually_transferring)
                {

                    i32 block_size_pow2 = __builtin_ctz((u32) cmder->block_size); // Get size of the data-block.

                    if (!IS_POWER_OF_TWO(cmder->block_size) || block_size_pow2 > 14)
                        bug; // Must be a power of two no greater than 2^14 = 16384 bytes.

                    if (cmder->remaining % cmder->block_size)
                        bug; // Data must be a multiple of the data-block or else it'll be truncated.

                    if (cmder->remaining % sizeof(u32))
                        bug; // We're assuming data is also a multiple of word size to make it easy.

                    if (cmder->remaining >= (1 << 25))
                        bug; // Transfer limit of the DATALENGTH field.

                    if (cmder->remaining <= 0)
                        bug; // Can't transfer no data...

                    if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DPSMACT))
                        bug; // The DPSM must be inactive in order to be configured.

                    CMSIS_SET
                    (
                        SDMMC     , DLEN            ,
                        DATALENGTH, cmder->remaining, // Let DPSM know the expected transfer amount.
                    );

                    CMSIS_SET
                    (
                        SDMMC     , DCTRL                          ,
                        DBLOCKSIZE, block_size_pow2                , // CRC16 is appended at end of each data-block, so this tells the DPSM when to check/send it.
                        DTMODE    , 0b00                           , // TODO Important for multiple reads/writes?
                        DTDIR     , !!SD_CMDS[cmder->cmd].receiving, // Whether we are receiving or transmitting data-blocks.
                    );

                }



                // Configure command packet and begin the command-path state-machine.

                CMSIS_SET
                (
                    SDMMC , ARG            ,
                    CMDARG, actual_argument, // The 32-bit argument for the command, if command needs it at all.
                );

                CMSIS_SET
                (
                    SDMMC   , CMD                                  ,
                    CMDINDEX, SD_CMDS[actual_cmd].code             , // Set the command code to send.
                    WAITRESP, SD_CMDS[actual_cmd].waitresp_code    , // Type of command response to expect.
                    CMDSTOP , actual_cmd == SDCmd_STOP_TRANSMISSION, // Lets the DPSM be signaled to go back to the idle state.
                    CMDTRANS, !!actually_transferring              , // If needed, the CPSM will send a DataEnable signal to DPSM to begin transferring.
                    CPSMEN  , true                                 , // Enable command-path state-machine.
                );

                #if SDCMDER_LOG_EXECUTED_COMMANDS
                log("Executing :: (%s)", SD_CMDS[actual_cmd].name);
                #endif



                return SDCmderIterateResult_again;

            } break;

            case SDMMCInterruptEvent_end_of_busy_signal                     : bug;
            case SDMMCInterruptEvent_cmd12_aborted_data_transfer            : bug;
            case SDMMCInterruptEvent_completed_transfer                     : bug;
            case SDMMCInterruptEvent_command_sent_with_no_response_expected : bug;
            case SDMMCInterruptEvent_command_sent_with_good_response        : bug;
            case SDMMCInterruptEvent_data_timeout                           : bug;
            case SDMMCInterruptEvent_command_timeout                        : bug;
            case SDMMCInterruptEvent_data_with_bad_crc                      : bug;
            case SDMMCInterruptEvent_command_with_bad_crc                   : bug;
            default                                                         : bug;

        } break;



        ////////////////////////////////////////
        //
        // The SD-cmder is in the process of sending the APP_CMD prefix.
        //
        ////////////////////////////////////////

        case SDCmderState_transferring_acmd_prefix: switch (interrupt_event)
        {



            case SDMMCInterruptEvent_none:
            {

                if (cmder->error)
                    bug; // No reason for an error yet...

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DPSMACT))
                    bug; // The data-path state-machine shouldn't have been active for APP_CMD itself.

                return SDCmderIterateResult_yield; // Nothing new yet.

            } break;



            case SDMMCInterruptEvent_end_of_busy_signal:
            {

                if (cmder->error)
                    bug; // No reason for an error yet...

                if (CMSIS_GET(SDMMC, STA, BUSYD0))
                    bug; // Shouldn't be busy anymore!



                // Although the APP_CMD isn't a command that should
                // result in a busy signal, we'll be liberal and let
                // it slide here.

                return SDCmderIterateResult_again;

            } break;



            case SDMMCInterruptEvent_command_sent_with_good_response:
            {

                if (cmder->error)
                    bug; // No reason for an error already...

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine should've been done by now.

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DPSMACT))
                    bug; // The data-path state-machine shouldn't have been active for APP_CMD itself.

                if (!(SDMMC->RESP1 & (1 << 5)))
                    return SDCmderIterateResult_card_glitch; // Next command not being recognized as ACMD..? @/pg 148/tbl 4-42/`SD`.

                if (SDMMC->RESPCMD != SD_CMDS[SDCmd_APP_CMD].code)
                    bug; // Response didn't match the expected command code..?

                if (CMSIS_GET(SDMMC, STA, BUSYD0))
                    bug; // The APP_CMD shouldn't make the card busy.



                // We're done sending the APP_CMD prefix;
                // we now move onto actually scheduling the user's desired command.

                cmder->state = SDCmderState_scheduled_amcd_prefixed_command;
                return SDCmderIterateResult_again;

            } break;



            case SDMMCInterruptEvent_command_timeout:
            case SDMMCInterruptEvent_command_with_bad_crc:
            {

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine should've turned itself off.

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DPSMACT))
                    bug; // The data-path state-machine shouldn't have been active for APP_CMD itself.

                if (cmder->error)
                    bug; // There shouldn't be any errors before this...

                cmder->error =
                    interrupt_event == SDMMCInterruptEvent_command_timeout
                        ? SDCmderError_command_timeout
                        : SDCmderError_bad_crc;

                cmder->state = SDCmderState_ready_for_next_command;
                return SDCmderIterateResult_command_attempted;

            } break;



            case SDMMCInterruptEvent_command_sent_with_no_response_expected : bug;
            case SDMMCInterruptEvent_cmd12_aborted_data_transfer            : bug;
            case SDMMCInterruptEvent_completed_transfer                     : bug;
            case SDMMCInterruptEvent_data_timeout                           : bug;
            case SDMMCInterruptEvent_data_with_bad_crc                      : bug;
            default                                                         : bug;

        } break;



        ////////////////////////////////////////
        //
        // The SD-cmder is in the process of sending the user's desired command.
        //
        ////////////////////////////////////////

        case SDCmderState_transferring_user_command: switch (interrupt_event)
        {



            case SDMMCInterruptEvent_none:
            {

                if (cmder->error)
                    bug; // No reason for an error yet...

                return SDCmderIterateResult_yield; // Nothing interesting yet...

            } break;



            case SDMMCInterruptEvent_end_of_busy_signal:
            {

                if (cmder->error)
                    bug; // No reason for an error yet...

                if (CMSIS_GET(SDMMC, STA, BUSYD0))
                    bug; // Shouldn't be busy anymore!

                return SDCmderIterateResult_again; // Cool, whatever.

            } break;



            case SDMMCInterruptEvent_command_sent_with_no_response_expected:
            case SDMMCInterruptEvent_command_sent_with_good_response:
            {

                if (cmder->error)
                    bug; // No reason for an error already...

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine should've been done by now.

                if
                (
                    SDMMC->RESPCMD != SD_CMDS[cmder->cmd].code
                    && SD_CMDS[cmder->cmd].waitresp_type != SDWaitRespType_none
                    && SD_CMDS[cmder->cmd].waitresp_type != SDWaitRespType_r2
                    && SD_CMDS[cmder->cmd].waitresp_type != SDWaitRespType_r3
                )
                    return SDCmderIterateResult_card_glitch; // Response must match the expected command code if the response has it.



                // The SD card can pull D0 pin low to indicate
                // it needs a bit of time to process the command,
                // so we wait for that first.

                cmder->state = SDCmderState_outwaiting_busy_signal_for_user_command;
                return SDCmderIterateResult_again;

            } break;



            case SDMMCInterruptEvent_command_timeout:
            case SDMMCInterruptEvent_command_with_bad_crc:
            {

                if (cmder->error)
                    bug; // There shouldn't be any errors before this...



                cmder->error =
                    interrupt_event == SDMMCInterruptEvent_command_timeout
                        ? SDCmderError_command_timeout
                        : SDCmderError_bad_crc;



                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DPSMACT))
                {

                    // Because we were expecting a data transfer, the DPSM is still active.
                    // To turn if off, we have to send a STOP_TRANSMISSION.

                    cmder->state = SDCmderState_scheduled_stop_transmission_for_error;
                    return SDCmderIterateResult_again;

                }
                else // We can immediately move onto doing the next command.
                {
                    cmder->state = SDCmderState_ready_for_next_command;
                    return SDCmderIterateResult_command_attempted;
                }

            } break;



            case SDMMCInterruptEvent_cmd12_aborted_data_transfer : bug;
            case SDMMCInterruptEvent_completed_transfer          : bug;
            case SDMMCInterruptEvent_data_timeout                : bug;
            case SDMMCInterruptEvent_data_with_bad_crc           : bug;
            default                                              : bug;

        } break;




        ////////////////////////////////////////
        //
        // Wait for the card to no longer signal that it's busy.
        //
        ////////////////////////////////////////

        case SDCmderState_outwaiting_busy_signal_for_user_command      :
        case SDCmderState_outwaiting_busy_signal_for_stop_transmission : switch (interrupt_event)
        {



            case SDMMCInterruptEvent_end_of_busy_signal:
            {

                if (CMSIS_GET(SDMMC, STA, BUSYD0))
                    bug; // Shouldn't be busy anymore!

                return SDCmderIterateResult_again; // Next iteration we'll move onto next state.

            } break;



            case SDMMCInterruptEvent_none:
            {

                if (CMSIS_GET(SDMMC, STA, BUSYD0))
                {
                    return SDCmderIterateResult_yield; // Still busy...
                }
                else if (cmder->state == SDCmderState_outwaiting_busy_signal_for_stop_transmission)
                {

                    if (CMSIS_GET(SDMMC, STA, DPSMACT))
                        bug; // Data-path state-machine should've been disabled by now.

                    cmder->state = SDCmderState_ready_for_next_command;
                    return SDCmderIterateResult_command_attempted;

                }
                else if (cmder->block_size) // Now move onto receiving/transmitting data-blocks?
                {
                    cmder->state = SDCmderState_undergoing_data_transfer;
                    return SDCmderIterateResult_again;
                }
                else // The caller's desired command has been successfully executed.
                {
                    cmder->state = SDCmderState_ready_for_next_command;
                    return SDCmderIterateResult_command_attempted;
                }

            } break;



            case SDMMCInterruptEvent_command_sent_with_no_response_expected : bug;
            case SDMMCInterruptEvent_command_sent_with_good_response        : bug;
            case SDMMCInterruptEvent_command_timeout                        : bug;
            case SDMMCInterruptEvent_command_with_bad_crc                   : bug;
            case SDMMCInterruptEvent_cmd12_aborted_data_transfer            : bug;
            case SDMMCInterruptEvent_completed_transfer                     : bug;
            case SDMMCInterruptEvent_data_timeout                           : bug;
            case SDMMCInterruptEvent_data_with_bad_crc                      : bug;
            default                                                         : bug;

        } break;



        ////////////////////////////////////////
        //
        // The SD-cmder is in the process of receiving/transmitting data-blocks.
        //
        ////////////////////////////////////////

        case SDCmderState_undergoing_data_transfer: switch (interrupt_event)
        {



            case SDMMCInterruptEvent_none:
            {

                if (cmder->error)
                    bug; // No reason for an error already...

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine should've been done by now.



                if (cmder->remaining) // TODO We're manually transferring data-blocks for now...
                {

                    if (!cmder->data)
                        bug; // Missing data..?

                    if (cmder->remaining < cmder->block_size && cmder->remaining % cmder->block_size)
                        bug; // Data not multiple of block-size..?

                    i32 word_index = 0;
                    for
                    (
                        ;
                        word_index < cmder->block_size;
                        word_index += sizeof(u32)
                    )
                    {
                        if (CMSIS_GET(SDMMC, STA, DPSMACT))
                        {
                            while (true)
                            {
                                if (CMSIS_GET(SDMMC, STA, DTIMEOUT))
                                {
                                    break;
                                }
                                else if (SD_CMDS[cmder->cmd].receiving)
                                {
                                    if (!CMSIS_GET(SDMMC, STA, RXFIFOE))
                                    {
                                        *(u32*) (cmder->data + word_index) = CMSIS_GET(SDMMC, FIFO, FIFODATA);
                                        break;
                                    }
                                }
                                else if (!CMSIS_GET(SDMMC, STA, TXFIFOF))
                                {
                                    CMSIS_SET(SDMMC, FIFO, FIFODATA, *(u32*) (cmder->data + word_index));
                                    break;
                                }
                            }
                        }
                        else
                        {
                            sorry
                        }
                    }

                    if (word_index == cmder->block_size)
                    {
                        cmder->data       = 0;
                        cmder->remaining -= cmder->block_size;
                    }

                    return SDCmderIterateResult_again;

                }
                else
                {
                    return SDCmderIterateResult_yield; // Nothing new yet.
                }

            } break;



            case SDMMCInterruptEvent_completed_transfer:
            {

                if (cmder->error)
                    bug; // No reason for an error.

                if (cmder->remaining)
                    bug; // There shouldn't be anything left to transmit.

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine should've been done by now.

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DPSMACT))
                    bug; // Data-path state-machine should've been done by now.



                // The caller's desired command has been successfully executed with a successful transfer.

                cmder->state = SDCmderState_ready_for_next_command;
                return SDCmderIterateResult_command_attempted;

            } break;



            case SDMMCInterruptEvent_data_timeout:
            {

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine should've been done by now.

                if (cmder->error)
                    bug; // There shouldn't be any errors before this...

                if (!CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DPSMACT))
                    bug; // Data-path state-machine should still be active for data time-out conditions.

                cmder->error = SDCmderError_data_timeout;
                cmder->state = SDCmderState_scheduled_stop_transmission_for_error;
                return SDCmderIterateResult_again;

            } break;



            case SDMMCInterruptEvent_data_with_bad_crc:
            {

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine should've been done by now.

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DPSMACT))
                    bug; // Data-path state-machine should've been deactivated by now.

                if (cmder->error)
                    bug; // There shouldn't be any errors before this...

                cmder->error = SDCmderError_bad_crc;
                cmder->state = SDCmderState_ready_for_next_command;
                return SDCmderIterateResult_command_attempted;

            } break;



            case SDMMCInterruptEvent_end_of_busy_signal                     : bug;
            case SDMMCInterruptEvent_command_sent_with_no_response_expected : bug;
            case SDMMCInterruptEvent_command_sent_with_good_response        : bug;
            case SDMMCInterruptEvent_command_timeout                        : bug;
            case SDMMCInterruptEvent_command_with_bad_crc                   : bug;
            case SDMMCInterruptEvent_cmd12_aborted_data_transfer            : bug;
            default                                                         : bug;

        } break;



        ////////////////////////////////////////
        //
        // The SD-cmder is attempting to reset the DPSM with STOP_TRANSMISSION.
        //
        ////////////////////////////////////////

        case SDCmderState_transferring_stop_transmission_for_error: switch (interrupt_event)
        {



            case SDMMCInterruptEvent_none:
            {

                if (!cmder->error)
                    bug; // Error not set..?

                return SDCmderIterateResult_yield; // Nothing new yet.

            } break;



            case SDMMCInterruptEvent_end_of_busy_signal:
            {

                if (!cmder->error)
                    bug; // Error not set..?

                if (CMSIS_GET(SDMMC, STA, BUSYD0))
                    bug; // Shouldn't be busy anymore!

                return SDCmderIterateResult_again; // Cool, whatever.

            } break;



            case SDMMCInterruptEvent_cmd12_aborted_data_transfer:
            {

                if (!cmder->error)
                    bug; // Error not set..?

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DPSMACT))
                    bug; // Data-path state-machine should've been disabled by now.

                return SDCmderIterateResult_again; // We still have to wait for the CPSM to be done with STOP_TRANSMISSION.

            } break;



            case SDMMCInterruptEvent_command_sent_with_good_response:
            {

                if (!cmder->error)
                    bug; // Error not set..?

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine shouldn't be enabled...

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DPSMACT))
                {
                    return SDCmderIterateResult_card_glitch; // We tried to disable the DPSM with STOP_TRANSMISSION, but it still didn't work...
                }
                else
                {
                    cmder->state = SDCmderState_outwaiting_busy_signal_for_stop_transmission;
                    return SDCmderIterateResult_again;
                }

            } break;



            case SDMMCInterruptEvent_command_timeout:
            case SDMMCInterruptEvent_command_with_bad_crc:
            {

                if (!cmder->error)
                    bug; // Error not set..?

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine shouldn't still be enabled...

                if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DPSMACT))
                {
                    return SDCmderIterateResult_card_glitch; // We tried to disable the DPSM with STOP_TRANSMISSION, but it still didn't work...
                }
                else
                {
                    cmder->state = SDCmderState_ready_for_next_command;
                    return SDCmderIterateResult_command_attempted;
                }

            } break;



            case SDMMCInterruptEvent_command_sent_with_no_response_expected : bug;
            case SDMMCInterruptEvent_completed_transfer                     : bug;
            case SDMMCInterruptEvent_data_timeout                           : bug;
            case SDMMCInterruptEvent_data_with_bad_crc                      : bug;
            default                                                         : bug;

        } break;



        default: bug;

    }

}



////////////////////////////////////////////////////////////////////////////////



static useret enum SDCmderUpdateResult : u32
{
    SDCmderUpdateResult_yield,
    SDCmderUpdateResult_ready_for_next_command,
    SDCmderUpdateResult_command_attempted,
    SDCmderUpdateResult_card_glitch,
    SDCmderUpdateResult_bug = BUG_CODE,
}
SDCmder_update(SDMMC_TypeDef* SDMMC, struct SDCmder* cmder)
{
    while (true)
    {

        enum SDCmderIterateResult result = _SDCmder_iterate_once(SDMMC, cmder);

        switch (result)
        {

            case SDCmderIterateResult_again:
            {
                // The state-machine still needs to be updated.
            } break;

            case SDCmderIterateResult_yield:
            {

                #if SDCMDER_LOG_UPDATE_ITERATION
                log("> YIELDING");
                #endif

                return SDCmderUpdateResult_yield; // No further updates can be done right now.

            } break;

            case SDCmderIterateResult_ready_for_next_command:
            {

                #if SDCMDER_LOG_UPDATE_ITERATION
                log("> READY");
                #endif

                return SDCmderUpdateResult_ready_for_next_command;

            } break;

            case SDCmderIterateResult_command_attempted:
            {

                #if SDCMDER_LOG_UPDATE_ITERATION
                log("> ATTEMPTED :: (%s)", SDCmderError_TABLE[cmder->error].name);
                #endif

                return SDCmderUpdateResult_command_attempted;

            } break;

            case SDCmderIterateResult_card_glitch:
            {

                #if SDCMDER_LOG_UPDATE_ITERATION
                log("> CARD GLITCH");
                #endif

                return SDCmderUpdateResult_card_glitch;

            } break;

            case SDCmderIterateResult_bug : bug;
            default                       : bug;

        }

    }
}
