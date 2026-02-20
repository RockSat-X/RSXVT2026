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
            undergoing_data_transfer_of_single_data_block
            waiting_for_users_next_data_block
            scheduled_stop_transmission
            transferring_stop_transmission
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

struct SDCmder // @/`Scheduling a Command with SD-Cmder`.
{
    struct
    {
        enum SDCmderState state;
        enum SDCmd        cmd;
        u32               argument;
        i32               total_blocks_to_transfer;
        i32               bytes_per_block;
        u8*               data_block_pointer;
        i32               data_block_count;
        b32               stop_requesting_for_data_blocks;
        u16               rca;
    };
    struct
    {
        enum SDCmderError error;
        i32               blocks_processed_so_far;
        i32               current_byte_index_of_block_data;
    };
};



////////////////////////////////////////////////////////////////////////////////



static void
_SDCmder_set_up_data_path_state_machine(SDMMC_TypeDef* SDMMC, struct SDCmder* cmder, b32 enable)
{

    if (CMSIS_GET(SDMMC, STA, CPSMACT))
        bug; // The command-path state-machine isn't expected to be active.

    if (CMSIS_GET(SDMMC, STA, DPSMACT))
        bug; // The DPSM must be inactive in order to be configured.

    if (cmder->total_blocks_to_transfer <= 0)
        bug; // Can't be transferring nothing!

    if (cmder->bytes_per_block == 0)
        bug; // Avoid undefined behavior with `__builtin_ctz`.

    if (!cmder->data_block_pointer)
        bug; // No source/destination given...

    if ((u32) cmder->data_block_pointer % sizeof(u32))
        bug; // Unaligned source/destination addresss...

    if (cmder->data_block_count <= 0)
        bug; // Non-sensical amount of data-blocks given/expected...

    if (cmder->blocks_processed_so_far + cmder->data_block_count > cmder->total_blocks_to_transfer)
        bug; // Transferring more data-blocks than expected...

    i32 bytes_per_block_pow2 = __builtin_ctz((u32) cmder->bytes_per_block); // Get size of the data-block.
    i32 data_length          = cmder->data_block_count * cmder->bytes_per_block;

    if (!IS_POWER_OF_TWO(cmder->bytes_per_block) || bytes_per_block_pow2 > 14)
        bug; // Must be a power of two no greater than 2^14 = 16384 bytes.

    if (data_length % sizeof(u32))
        bug; // We're assuming data is also a multiple of word size to make it easy.

    if (data_length >= (1 << 25))
        bug; // Transfer limit of the DATALENGTH field.

    if (data_length <= 0)
        bug; // Can't transfer no data...



    CMSIS_SET
    (
        SDMMC     , DLEN       ,
        DATALENGTH, data_length, // Amount of bytes to transfer.
    );

    CMSIS_SET
    (
        SDMMC    , IDMABASER                      ,
        IDMABASER, (u32) cmder->data_block_pointer, // The source/destination for the internal DMA.
    );

    CMSIS_SET
    (
        SDMMC , IDMACTRL,
        IDMAEN, true    , // Enable internal DMA.
    );

    CMSIS_SET
    (
        SDMMC     , DCTRL                          ,
        DBLOCKSIZE, bytes_per_block_pow2           , // CRC16 is appended at end of each data-block, so this tells the DPSM when to check/send it.
        DTDIR     , !!SD_CMDS[cmder->cmd].receiving, // Whether we are receiving or transmitting data-blocks.
        DTEN      , !!enable                       , // Whether the DPSM should be activated with or without the CPSM.
    );

}



static useret enum SDCmderIterateResult : u32
{
    SDCmderIterateResult_again,
    SDCmderIterateResult_yield,
    SDCmderIterateResult_ready_for_next_command,
    SDCmderIterateResult_need_user_to_provide_next_data_block,
    SDCmderIterateResult_command_attempted,
    SDCmderIterateResult_card_glitch,
    SDCmderIterateResult_bug = BUG_CODE,
}
_SDCmder_iterate(SDMMC_TypeDef* SDMMC, struct SDCmder* cmder)
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



    // "end of SDMMC_D0 Busy following a CMD response detected".
    //
    // This interrupt event is of little importance,
    // so if it happens, we can just immediately clear it
    // and use `BUSYD0` to see if the card is still busy.

    CMSIS_SET(SDMMC, ICR, BUSYD0ENDC, true);
    CMSIS_SET_FROM(interrupt_status, SDMMC, STA, BUSYD0END, false);



    if
    (
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, IDMABTC   ) || // "IDMA buffer transfer complete".
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, IDMATE    ) || // "IDMA transfer error".
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, CKSTOP    ) || // "SDMMC_CK stopped in Voltage switch procedure".
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, VSWEND    ) || // Voltage switch critical timing section completion".
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, ACKTIMEOUT) || // "Boot acknowledge timeout".
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, ACKFAIL   ) || // "... (boot acknowledgment check fail)".
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, SDIOIT    ) || // "SDIO interrupt received".
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, RXOVERR   ) || // RX-FIFO got too full (shouldn't happen).
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, TXUNDERR  ) || // Hardware found TX-FIFO empty (shouldn't happen).
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DHOLD     ) || // "Data transfer Hold".
        CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DBCKEND   )    // "Data block sent/received".
    )
    {
        bug; // Shouldn't happen, mostly because the feature isn't even used.
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

        case SDCmderState_scheduled_amcd_prefixed_command :
        case SDCmderState_scheduled_command               :
        case SDCmderState_scheduled_stop_transmission     : switch (interrupt_event)
        {

            case SDMMCInterruptEvent_none:
            {

                if (cmder->cmd == SDCmd_APP_CMD)
                    bug; // APP_CMD is already handled automatically.

                if (cmder->cmd == SDCmd_STOP_TRANSMISSION)
                    bug; // STOP_TRANSMISSION is already handled automatically.

                if (CMSIS_GET(SDMMC, STA, CPSMACT))
                    bug; // The command-path state-machine shouldn't be active.

                if (!implies(CMSIS_GET(SDMMC, STA, CPSMACT), cmder->state == SDCmderState_scheduled_stop_transmission))
                    bug; // The data-path state-machine begin active still is only okay for STOP_TRANSMISSION commands...

                if (CMSIS_GET(SDMMC, CMD, CPSMEN))
                    bug; // The command-path state-machine shouldn't still be enabled...

                if (CMSIS_GET(SDMMC, DCTRL, DTEN))
                    bug; // This bit should've been cleared...


                // The actual command that we should do depends on certain
                // things like whether or not we should send the ACMD prefix first.

                enum SDCmd actual_cmd            = {0};
                u32        actual_argument       = {0};
                b32        actually_transferring = {0};

                if (cmder->state == SDCmderState_scheduled_stop_transmission)
                {

                    actual_cmd            = SDCmd_STOP_TRANSMISSION;
                    actual_argument       = 0; // No argument necessary.
                    actually_transferring = false;
                    cmder->state          = SDCmderState_transferring_stop_transmission;

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
                    actually_transferring = !!cmder->total_blocks_to_transfer;
                    cmder->state          = SDCmderState_transferring_user_command;
                }



                // Set up the data-path state-machine.

                if (actually_transferring)
                {
                    _SDCmder_set_up_data_path_state_machine(SDMMC, cmder, false);
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

                if (CMSIS_GET(SDMMC, STA, DPSMACT))
                    bug; // The data-path state-machine shouldn't have been active for APP_CMD itself.

                return SDCmderIterateResult_yield; // Nothing new yet.

            } break;



            case SDMMCInterruptEvent_command_sent_with_good_response:
            {

                if (cmder->error)
                    bug; // No reason for an error already...

                if (CMSIS_GET(SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine should've been done by now.

                if (CMSIS_GET(SDMMC, STA, DPSMACT))
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

                if (CMSIS_GET(SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine should've turned itself off.

                if (CMSIS_GET(SDMMC, STA, DPSMACT))
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



            case SDMMCInterruptEvent_command_sent_with_no_response_expected:
            case SDMMCInterruptEvent_command_sent_with_good_response:
            {

                if (cmder->error)
                    bug; // No reason for an error already...

                if (CMSIS_GET(SDMMC, STA, CPSMACT))
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



                if (CMSIS_GET(SDMMC, STA, DPSMACT))
                {

                    // Because we were expecting a data transfer, the DPSM is still active.
                    // To turn if off, we have to send a STOP_TRANSMISSION.

                    cmder->state = SDCmderState_scheduled_stop_transmission;
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
        // Wait for the card to no longer signal that it's busy from the user's command.
        //
        ////////////////////////////////////////

        case SDCmderState_outwaiting_busy_signal_for_user_command: switch (interrupt_event)
        {

            case SDMMCInterruptEvent_none:
            {
                if (CMSIS_GET(SDMMC, STA, BUSYD0))
                {
                    return SDCmderIterateResult_yield; // Still busy...
                }
                else if (cmder->total_blocks_to_transfer) // Now move onto receiving/transmitting data-blocks?
                {
                    cmder->state = SDCmderState_undergoing_data_transfer_of_single_data_block;
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

        case SDCmderState_undergoing_data_transfer_of_single_data_block: switch (interrupt_event)
        {

            case SDMMCInterruptEvent_none:
            {

                if (cmder->error)
                    bug; // No reason for an error already...

                if (CMSIS_GET(SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine should've been done by now.

                if (!cmder->data_block_pointer)
                    bug; // Missing source/destination..?

                if (cmder->data_block_count <= 0)
                    bug; // Non-sensical transfer amount..?

                return SDCmderIterateResult_yield;

            } break;



            case SDMMCInterruptEvent_completed_transfer:
            {

                if (cmder->error)
                    bug; // No reason for an error already...

                if (CMSIS_GET(SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine should've been done by now.

                if (CMSIS_GET(SDMMC, STA, DPSMACT))
                    bug; // Data-path state-machine should've been done by now.

                if (!cmder->data_block_pointer)
                    bug; // Missing source/destination..?

                if (cmder->data_block_count <= 0)
                    bug; // Non-sensical transfer amount..?



                // The IDMA finished transferring the data-block.

                cmder->blocks_processed_so_far += cmder->data_block_count;
                cmder->data_block_pointer       = nullptr;
                cmder->data_block_count         = 0;



                // Still got more data-blocks to transfer?

                if (cmder->blocks_processed_so_far < cmder->total_blocks_to_transfer)
                {
                    cmder->state = SDCmderState_waiting_for_users_next_data_block;
                    return SDCmderIterateResult_again;
                }



                // Command is associated with ending with STOP_TRANSMISSION?

                else if (SD_CMDS[cmder->cmd].end_with_stop_transmission)
                {
                    cmder->state = SDCmderState_scheduled_stop_transmission;
                    return SDCmderIterateResult_again;
                }



                // The data-transfer for the user's command completed successfully.

                else
                {
                    cmder->state = SDCmderState_ready_for_next_command;
                    return SDCmderIterateResult_command_attempted;
                }

            } break;



            case SDMMCInterruptEvent_data_timeout:
            case SDMMCInterruptEvent_data_with_bad_crc:
            {

                if (CMSIS_GET(SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine should've been done by now.

                if (cmder->error)
                    bug; // There shouldn't be any errors before this...

                cmder->error =
                    interrupt_event == SDMMCInterruptEvent_data_timeout
                        ? SDCmderError_data_timeout
                        : SDCmderError_bad_crc;

                cmder->state = SDCmderState_scheduled_stop_transmission;
                return SDCmderIterateResult_again;

            } break;



            case SDMMCInterruptEvent_command_sent_with_no_response_expected : bug;
            case SDMMCInterruptEvent_command_sent_with_good_response        : bug;
            case SDMMCInterruptEvent_command_timeout                        : bug;
            case SDMMCInterruptEvent_command_with_bad_crc                   : bug;
            case SDMMCInterruptEvent_cmd12_aborted_data_transfer            : bug;
            default                                                         : bug;

        } break;



        ////////////////////////////////////////
        //
        // The SD-cmder still has more data-blocks to transfer,
        // but the source/destination of the data has not been given yet.
        //
        ////////////////////////////////////////

        case SDCmderState_waiting_for_users_next_data_block: switch (interrupt_event)
        {

            case SDMMCInterruptEvent_none:
            {

                if (cmder->error)
                    bug; // No reason for an error already...

                if (CMSIS_GET(SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine should've been done by now.

                if (CMSIS_GET(SDMMC, STA, DPSMACT))
                    bug; // Data-path state-machine should've been done by now.

                if (!iff(cmder->data_block_pointer, cmder->data_block_count >= 1))
                    bug; // If a data-block pointer is given, there must be a valid amount (and vice-versa).



                // We can now begin the transfer of the next data-block.

                if (cmder->data_block_pointer)
                {
                    _SDCmder_set_up_data_path_state_machine(SDMMC, cmder, true);
                    cmder->state = SDCmderState_undergoing_data_transfer_of_single_data_block;
                    return SDCmderIterateResult_again;
                }



                // The user wants to stop the data transfer of data-blocks prematurely.

                else if (cmder->stop_requesting_for_data_blocks)
                {
                    cmder->state = SDCmderState_scheduled_stop_transmission;
                    return SDCmderIterateResult_again;
                }



                // Tell the user it should provide us with the
                // data address in order to advance the data transfer process.

                else
                {
                    return SDCmderIterateResult_need_user_to_provide_next_data_block;
                }

            } break;

            case SDMMCInterruptEvent_completed_transfer                     : bug;
            case SDMMCInterruptEvent_data_timeout                           : bug;
            case SDMMCInterruptEvent_data_with_bad_crc                      : bug;
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

        case SDCmderState_transferring_stop_transmission: switch (interrupt_event)
        {



            case SDMMCInterruptEvent_none:
            {
                return SDCmderIterateResult_yield; // Nothing new yet.
            } break;



            case SDMMCInterruptEvent_cmd12_aborted_data_transfer:
            {

                if (CMSIS_GET(SDMMC, STA, DPSMACT))
                    bug; // Data-path state-machine should've been disabled by now.

                return SDCmderIterateResult_again; // We still have to wait for the CPSM to be done with STOP_TRANSMISSION.

            } break;



            case SDMMCInterruptEvent_command_sent_with_good_response:
            {

                if (CMSIS_GET(SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine shouldn't be enabled...

                cmder->state = SDCmderState_outwaiting_busy_signal_for_stop_transmission;
                return SDCmderIterateResult_again;

            } break;



            case SDMMCInterruptEvent_command_timeout:
            case SDMMCInterruptEvent_command_with_bad_crc:
            {

                if (CMSIS_GET(SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine shouldn't still be enabled...

                cmder->error =
                    interrupt_event == SDMMCInterruptEvent_command_timeout
                        ? SDCmderError_command_timeout
                        : SDCmderError_bad_crc;

                cmder->state = SDCmderState_outwaiting_busy_signal_for_stop_transmission;
                return SDCmderIterateResult_again;

            } break;



            case SDMMCInterruptEvent_command_sent_with_no_response_expected : bug;
            case SDMMCInterruptEvent_completed_transfer                     : bug;
            case SDMMCInterruptEvent_data_timeout                           : bug;
            case SDMMCInterruptEvent_data_with_bad_crc                      : bug;
            default                                                         : bug;

        } break;



        ////////////////////////////////////////
        //
        // Wait for the card to no longer signal that it's busy from STOP_TRANSMISSION.
        //
        ////////////////////////////////////////

        case SDCmderState_outwaiting_busy_signal_for_stop_transmission: switch (interrupt_event)
        {



            case SDMMCInterruptEvent_none:
            {

                if (CMSIS_GET(SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine shouldn't still be enabled...

                if (CMSIS_GET(SDMMC, STA, BUSYD0))
                {
                    return SDCmderIterateResult_yield; // Still busy...
                }
                else if (CMSIS_GET_FROM(interrupt_status, SDMMC, STA, DPSMACT)) // Reading from `interrupt_status` to ensure `cmd12_aborted_data_transfer` was handled.
                {
                    return SDCmderIterateResult_yield; // Still waiting for the DPSM to be turned off...
                }
                else
                {
                    cmder->state = SDCmderState_ready_for_next_command;
                    return SDCmderIterateResult_command_attempted;
                }

            } break;



            case SDMMCInterruptEvent_cmd12_aborted_data_transfer:
            {

                if (CMSIS_GET(SDMMC, STA, CPSMACT))
                    bug; // Command-path state-machine shouldn't still be enabled...

                if (CMSIS_GET(SDMMC, STA, DPSMACT))
                    bug; // Data-path state-machine should've been disabled by now.

                return SDCmderIterateResult_again;

            } break;



            case SDMMCInterruptEvent_command_sent_with_no_response_expected : bug;
            case SDMMCInterruptEvent_command_sent_with_good_response        : bug;
            case SDMMCInterruptEvent_command_timeout                        : bug;
            case SDMMCInterruptEvent_command_with_bad_crc                   : bug;
            case SDMMCInterruptEvent_completed_transfer                     : bug;
            case SDMMCInterruptEvent_data_timeout                           : bug; // @/`SD Bus Anomalies`.
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
    SDCmderUpdateResult_need_user_to_provide_next_data_block,
    SDCmderUpdateResult_command_attempted,
    SDCmderUpdateResult_card_glitch,
    SDCmderUpdateResult_bug = BUG_CODE,
}
SDCmder_update(SDMMC_TypeDef* SDMMC, struct SDCmder* cmder)
{
    while (true)
    {

        enum SDCmderIterateResult result = _SDCmder_iterate(SDMMC, cmder);

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

            case SDCmderIterateResult_need_user_to_provide_next_data_block:
            {

                #if SDCMDER_LOG_UPDATE_ITERATION
                log("> WAITING FOR USER DATA");
                #endif

                return SDCmderUpdateResult_need_user_to_provide_next_data_block;

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



////////////////////////////////////////////////////////////////////////////////



// @/`Scheduling a Command with SD-Cmder`:
//
// To schedule a command, the `.state` field should be set to
// `SDCmderState_scheduled_command` after configuring the settings
// for the SD command for the SD-cmder to carry out.
//
// This obviously includes `.cmd` and `.argument` to describe
// which SD command to execute and the 32-bit argument that
// goes along with it.
//
// If there is a data-transfer associated with the command,
// then `.total_blocks_to_transfer` should be set to some positive integer.
// The size of each data-block in the data-transfer is determined
// by `.bytes_per_block`.
//
// The user should immediately provide `.data_block_pointer` so that
// SD-cmder knows the source/destination of the data. The user should
// also set `.data_block_count` to some positive integer to indicate
// the amount of data-blocks as pointed by `.data_block_pointer`.
//
// If the initial value of `.data_block_count` is less than
// `.total_blocks_to_transfer`, then after some time, SD-cmder will
// request the user to update the `.data_block_pointer` and `.data_block_count`
// with a new source/destination and length. This repeats until all data-blocks
// are finally transferred, or if the user wants to end the data-transfer early,
// then they should set `.stop_requesting_for_data_blocks` to `true`.
//
// So for the best performance, `.data_block_count` should be
// as large as possible so that SDMMC's IDMA can automatically
// transfer as much data in one go without bothering the CPU so often.
// Obviously, this comes at the trade-off of requiring the user to have
// a large buffer reserved for `.data_block_pointer`.
//
// The `.rca` field should also be filled with the SD card's
// relative card address; this is needed for ACMDs where they're
// prefixed with APP_CMD and the argument for that is the RCA.
// @/pg 67/sec 4.2.2/`SD`.



// @/`SD Bus Anomalies`:
//
// The SD-cmder is written to be as robust as possible towards error conditions.
// As an example, the SD card can be ejected and reinserted repeatedly and the
// SD-cmder (and the SD driver as a whole) is capable of recovering from such
// bus errors. However, there are certain unavoidable conditions we cannot handle.
//
// One example that I can think that might be happening (this is speculative)
// is if the D0 data-line is pulled low when the DPSM is in a certain state.
// If the stars are aligned, the DPSM thinks this low D0 signal indicates a
// start bit, but the reality could be that it was just stray noise that was
// somehow injected (or perhaps due to the card being ejected/inserted).
// Either way, the DPSM might end up issuing a data-timeout error condition
// even though it shouldn't have. There isn't really a good way to determine
// whether or not this error condition is really due to a bug within the code
// (to which we'd like to flag it as so) or because of some obscure hardware issue.
//
// Either way, it's a rare error condition, and the user will be notified of the
// bug, and hopefully resetting the SDMMC peripheral will put everything back in
// its place.
