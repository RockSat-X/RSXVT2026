////////////////////////////////////////////////////////////////////////////////



enum SDCmderState : u32
{
    SDCmderState_ready_for_next_command,
    SDCmderState_scheduled_command,
    SDCmderState_transferring_command,
    SDCmderState_response_d0_busy,
    SDCmderState_undergoing_data_transfer,
    SDCmderState_postcommand_d0_busy,
    SDCmderState_command_attempted,
};



enum SDCmderError : u32
{
    SDCmderError_none,
    SDCmderError_command_crc_fail,
    SDCmderError_command_timeout,
    SDCmderError_data_crc_fail,
    SDCmderError_data_timeout,
    SDCmderError_cmd_code_mismatch,
};



struct SDCmder
{

    struct
    {
        enum SDCmderState state;
        enum SDCmderError error;
        enum SDCmd        cmd;
        u32               arg;
        u8*               data;
        i32               remaining;
        i32               block_size;
        u16               rca; // Relative card address. @/pg 67/sec 4.2.2/`SD`.
    };

    struct
    {
        enum SDCmd effective_cmd;
        b32        stopping_transmission;
    };

};



////////////////////////////////////////////////////////////////////////////////



static useret enum SDTransferDataManually : u32
{
    SDTransferDataManually_done,
    SDTransferDataManually_data_timeout,
    SDTransferDataManually_bug,
}
_SDCmder_transfer_data_manually(SDMMC_TypeDef* SDMMC, struct SDCmder* cmder)
{

    #undef  ret
    #define ret(NAME) return SDTransferDataManually_##NAME



    // Some checks.

    if (!SDMMC)
        bug;

    if (!cmder)
        bug;

    if (cmder->error)
        bug;

    if (!cmder->data)
        bug;

    if (cmder->state != SDCmderState_undergoing_data_transfer)
        bug;

    if (cmder->remaining < cmder->block_size)
        bug;

    if (cmder->remaining % cmder->block_size)
        bug;



    // Manually push/pop all the data in/out of the FIFO.

    for
    (
        i32 word_index = 0;
        word_index < cmder->block_size;
        word_index += sizeof(u32)
    )
    {


        if (!CMSIS_GET(SDMMC, STA, DPSMACT))
            bug;

        while (true)
        {
            if (CMSIS_GET(SDMMC, STA, DTIMEOUT))
            {
                ret(data_timeout);
            }
            else if (SD_CMDS[cmder->cmd].receiving)
            {
                if (!CMSIS_GET(SDMMC, STA, RXFIFOE))
                {
                    *(u32*) (cmder->data + word_index) = CMSIS_GET(SDMMC, FIFO, FIFODATA);
                    break;
                }
            }
            else
            {
                if (!CMSIS_GET(SDMMC, STA, TXFIFOF))
                {
                    CMSIS_SET(SDMMC, FIFO, FIFODATA, *(u32*) (cmder->data + word_index));
                    break;
                }
            }
        }

    }



    // Since we manually transfer all the data, the
    // entire data transfer is pretty much finished here.

    cmder->data       = 0;
    cmder->remaining -= cmder->block_size;

    ret(done);

}



////////////////////////////////////////////////////////////////////////////////



static useret enum SDCmderUpdate : u32
{
    SDCmderUpdate_again,
    SDCmderUpdate_ready_for_next_command,
    SDCmderUpdate_command_attempted,
    SDCmderUpdate_yield,
    SDCmderUpdate_sdmmc_needs_reinit,
    SDCmderUpdate_bug,
}
SDCmder_update(SDMMC_TypeDef* SDMMC, struct SDCmder* cmder)
{

    #undef  ret
    #define ret(NAME) return SDCmderUpdate_##NAME

    if (!SDMMC)
        bug;

    if (!cmder)
        bug;

    u32 status_snapshot = SDMMC->STA;

    #define STATUS_POP(FLAG)                                                                                  \
        (                                                                                                     \
            CMSIS_GET_FROM(status_snapshot, SDMMC, STA, FLAG) && /* Flag set?                              */ \
            (CMSIS_SET(SDMMC, ICR, FLAG##C, true), true)         /* Clear flag and let the condition pass. */ \
        )

    switch (cmder->state)
    {



        ////////////////////////////////////////////////////////////////////////////////
        //
        // Nothing to do right now.
        //



        case SDCmderState_ready_for_next_command:
        {
            ret(ready_for_next_command);
        } break;



        ////////////////////////////////////////////////////////////////////////////////
        //
        // We have a command to do!
        //



        case SDCmderState_scheduled_command:
        {

            if (status_snapshot & SDMMC->MASK)
                // There shouldn't be any
                // leftover interrupt events.
                bug;

            if (CMSIS_GET_FROM(status_snapshot, SDMMC, STA, BUSYD0))
                // Nor should the SDMMC state
                // machine think the D0 line is busy.
                bug;

            if (cmder->cmd == SDCmd_APP_CMD)
                // APP_CMD and STOP_TRANSMISSION are handled
                // automatically, so there should be no
                // reason to execute this command directly.
                bug;

            if (cmder->cmd == SDCmd_STOP_TRANSMISSION)
                bug;

            if (cmder->effective_cmd == SDCmd_APP_CMD && !SD_CMDS[cmder->cmd].acmd)
                // APP_CMD prefixing shouldn't be done
                // for commands that don't need it.
                bug;

            if (CMSIS_GET_FROM(status_snapshot, SDMMC, STA, CPSMACT))
                // Nothing should be active, except maybe
                // for the DPSM when we need to stop it.
                bug;

            if (!iff(cmder->stopping_transmission, CMSIS_GET_FROM(status_snapshot, SDMMC, STA, DPSMACT)))
                // TODO What if doing multiple reads/writes?
                bug;

            if (!iff(cmder->stopping_transmission && CMSIS_GET_FROM(status_snapshot, SDMMC, STA, DPSMACT), cmder->error))
                // There should be only errors if we're having
                // to send a STOP_TRANSMISSION to reset the DPSM.
                bug;



            // Determine the actual SD command to execute,
            // because we might need to send APP_CMD as a prefix or a STOP_TRANSMISSION as a suffix.

            u32 actual_arg        = {0};
            b32 transferring_data = {0};

            if (cmder->stopping_transmission)
            {
                if (cmder->effective_cmd == SDCmd_STOP_TRANSMISSION)
                    bug; // Should only be sent once.
                cmder->effective_cmd = SDCmd_STOP_TRANSMISSION;
                actual_arg           = 0;
                transferring_data    = false;
            }
            else if (SD_CMDS[cmder->cmd].acmd && cmder->effective_cmd != SDCmd_APP_CMD)
            {
                cmder->effective_cmd = SDCmd_APP_CMD;            // We need to send the APP_CMD prefix first.
                actual_arg           = ((u32) cmder->rca) << 16; // The relative card address (RCA) is 0x0000 in idle state. @/pg 67/sec 4.2.2/`SD`.
                transferring_data    = false;
            }
            else
            {
                cmder->effective_cmd = cmder->cmd;
                actual_arg           = cmder->arg;
                transferring_data    = !!cmder->remaining;
            }



            // Set up the data-path state machine.

            if (transferring_data)
            {

                // Get size of the data-block.
                i32 block_size_pow2 = __builtin_ctz(cmder->block_size);

                if (IS_POWER_OF_TWO(cmder->block_size) && block_size_pow2 > 14)
                    // Must be a power of two no
                    // greater than 2^14 = 16384 bytes.
                    // @/pg 2807/sec 59.10.9/`H7S3rm`.
                    // @/pg 2486/sec 60.10.9/`H730rm`.
                    bug;

                if (cmder->remaining % cmder->block_size)
                    // Amount of data must be a multiple of the
                    // data-block or else it'll be truncated.
                    // @/pg 2807/sec 59.10.9/`H7S3rm`.
                    // @/pg 2486/sec 60.10.9/`H730rm`.
                    bug;

                if (cmder->remaining % sizeof(u32))
                    // Assuming amount of data is also a multiple of word
                    // size so we won't have to deal with padding bytes.
                    bug;

                if (cmder->remaining >= (1 << 25))
                    // Transfer limit of the DATALENGTH field.
                    // @/pg 2805/sec 59.10.8/`H7S3rm`.
                    // @/pg 2484/sec 60.10.8/`H730rm`.
                    bug;

                if (cmder->remaining < 1)
                    // There must be data to be transferred.
                    bug;

                if (CMSIS_GET_FROM(status_snapshot, SDMMC, STA, DPSMACT))
                    // The DPSM must be inactive in order to be configured.
                    // @/pg 2807/sec 59.10.9/`H7S3rm`.
                    // @/pg 2486/sec 60.10.9/`H730rm`.
                    bug;



                // This is how the data-path state machine know
                // when we have transmitted/received all the data.

                CMSIS_SET(SDMMC, DLEN, DATALENGTH, cmder->remaining);

                CMSIS_SET
                (
                    SDMMC     , DCTRL                          ,
                    DBLOCKSIZE, block_size_pow2                , // CRC16 is appended at end of each block, so this tells the hardware when to check/send it.
                    DTMODE    , 0b00                           , // TODO Important for multiple reads/writes?
                    DTDIR     , !!SD_CMDS[cmder->cmd].receiving, // Whether we are receiving or transmitting data-blocks.
                );

            }



            // Configure command packet and begin
            // the command-path state machine.
            // @/pg 2751/fig 860/`H7S3rm`.
            // @/pg 2430/fig 738/`H730rm`.

            if (CMSIS_GET(SDMMC, CMD, CPSMEN))
                // The command-path state machine
                // shouldn't already be enabled...
                bug;

            CMSIS_SET(SDMMC, ARG, CMDARG, actual_arg);
            CMSIS_SET
            (
                SDMMC   , CMD                                            ,
                CMDINDEX, SD_CMDS[cmder->effective_cmd].code             , // Set the command code to send.
                WAITRESP, SD_CMDS[cmder->effective_cmd].waitresp_code    , // Type of command response to expect.
                CMDSTOP , cmder->effective_cmd == SDCmd_STOP_TRANSMISSION, // Lets the DPSM be signaled to go back to the idle state.
                CMDTRANS, !!transferring_data                            , // If needed, the CPSM will send a DataEnable signal to DPSM to begin transmitting/receiving data.
                CPSMEN  , true                                           , // Enable command-path state machine.
            );
            cmder->state = SDCmderState_transferring_command;

            ret(again);

        } break;



        ////////////////////////////////////////////////////////////////////////////////
        //
        // We're in the process of transferring the command...
        //



        case SDCmderState_transferring_command:
        {

            if (!iff(cmder->error, cmder->effective_cmd == SDCmd_STOP_TRANSMISSION))
                bug;



            if (STATUS_POP(DTIMEOUT))
            {

                if (cmder->effective_cmd != SDCmd_STOP_TRANSMISSION)
                    bug;

                // The data timeout is likely from the previous
                // command that we're stopping the transmission of.

                ret(yield);

            }
            else if (STATUS_POP(CCRCFAIL))
            {
                cmder->error = SDCmderError_command_crc_fail;
            }
            else if (STATUS_POP(CTIMEOUT))
            {
                cmder->error = SDCmderError_command_timeout;
            }
            else if (STATUS_POP(CMDREND))
            {
                // Hip hip hooray! SD card responded on time!
            }
            else if (STATUS_POP(CMDSENT))
            {
                // Hip hip hooray! SD command succesfully sent!
            }
            else if (STATUS_POP(BUSYD0END))
            {
                ret(yield); // It was just the busy signal; we don't care.
            }
            else if (status_snapshot & SDMMC->MASK)
            {
                bug; // There should be no other interrupt event.
            }
            else
            {
                ret(yield); // No SDMMC interrupt event of interest actually happened.
            }



            if (cmder->error)
            {
                cmder->state = SDCmderState_command_attempted;
            }
            else
            {
                // In certain situations, DAT0 will be pulled low
                // to indicate the card is busy, so we'll go wait
                // it out (even if there's no busy signal right now).
                // @/pg 77/sec 4.3/`SD`.
                cmder->state = SDCmderState_response_d0_busy;
            }

            ret(again);

        } break;



        ////////////////////////////////////////////////////////////////////////////////
        //
        // The SD card is busy interpreting our command...
        //



        case SDCmderState_response_d0_busy:
        {

            if (!iff(cmder->error, cmder->effective_cmd == SDCmd_STOP_TRANSMISSION))
                bug;



            // Busy signal was active for too long.
            if (STATUS_POP(DTIMEOUT))
            {
                cmder->error = SDCmderError_data_timeout;
            }

            // Busy signal was lifted!
            else if (!CMSIS_GET_FROM(status_snapshot, SDMMC, STA, BUSYD0))
            {
                STATUS_POP(BUSYD0END);
            }

            // There should be no other interrupt event.
            else if (status_snapshot & SDMMC->MASK)
            {
                bug;
            }

            // No SDMMC interrupt event of interest actually happened.
            else
            {
                ret(yield);
            }



            if (CMSIS_GET_FROM(status_snapshot, SDMMC, STA, CPSMACT))
                // Command-path state machine
                // should've been done by now.
                bug;



            if
            (
                SDMMC->RESPCMD != SD_CMDS[cmder->effective_cmd].code                // Response must match the expected command code...
                && SD_CMDS[cmder->effective_cmd].waitresp_type != SDWaitRespType_r2 // ... unless the response doesn't have the command-index field anyways.
                && SD_CMDS[cmder->effective_cmd].waitresp_type != SDWaitRespType_r3 // "
            )
            {
                cmder->error = SDCmderError_cmd_code_mismatch;
            }



            // Abort due to an error condition.
            if (cmder->error)
            {
                cmder->state = SDCmderState_command_attempted;
            }

            // Begin second phase of the application-specific command.
            else if (cmder->effective_cmd == SDCmd_APP_CMD)
            {

                if (!(SDMMC->RESP1 & (1 << 5)))
                    // Ensure the next command is
                    // interpreted as ACMD.
                    // @/pg 148/tbl 4-42/`SD`.
                    bug;

                cmder->state = SDCmderState_scheduled_command;

            }

            // We finished sending the command and we
            // now move onto the data transfer stage.
            else if (cmder->block_size)
            {
                cmder->state = SDCmderState_undergoing_data_transfer;
            }

            // The command only needed to be sent and nothing else.
            else
            {
                cmder->state = SDCmderState_command_attempted;
            }

            ret(again);

        } break;



        ////////////////////////////////////////////////////////////////////////////////
        //
        // We're currently transferring data...
        //



        case SDCmderState_undergoing_data_transfer:
        {

            if (cmder->error)
                bug;



            if (cmder->remaining)
            {
                enum SDTransferDataManually transfer_ret = _SDCmder_transfer_data_manually(SDMMC, cmder);
                switch (transfer_ret)
                {
                    case SDTransferDataManually_done         : break;
                    case SDTransferDataManually_data_timeout : break;
                    case SDTransferDataManually_bug          : bug;
                    default                                  : bug;
                }
            }



            if (STATUS_POP(RXOVERR))
            {
                // should've stopped to prevent this...
                bug;
            }
            else if (STATUS_POP(TXUNDERR))
            {
                // The clock should've stopped to prevent this...
                bug;
            }
            else if (STATUS_POP(DCRCFAIL))
            {
                cmder->error = SDCmderError_data_crc_fail;
            }
            else if (STATUS_POP(DTIMEOUT))
            {
                cmder->error = SDCmderError_data_timeout;
            }
            else if (STATUS_POP(DATAEND))
            {
                if (SD_CMDS[cmder->cmd].receiving && cmder->remaining)
                    // There shouldn't be
                    // anything left to transmit.
                    bug;
            }
            else if (STATUS_POP(DBCKEND)) // Data-block successfully sent/received with matching CRC16?
            {
                if (!cmder->remaining)
                    // There should be some data left,
                    // or else DATAEND would've been set.
                    bug;
            }
            else if (status_snapshot & SDMMC->MASK)
            {
                // There should be no
                // other interrupt event.
                bug;
            }
            else if (CMSIS_GET_FROM(status_snapshot, SDMMC, STA, BUSYD0))
            {
                // SDMMC shouldn't be thinking the card is busy.
                bug;
            }
            else
            {
                // No SDMMC interrupt event
                // of interest actually happened.
                ret(yield);
            }



            if (cmder->error || !cmder->remaining)
            {
                if (CMSIS_GET_FROM(status_snapshot, SDMMC, STA, CPSMACT))
                    // The CPSM should be
                    // disabled at this point.
                    bug;

                if (!iff(CMSIS_GET_FROM(status_snapshot, SDMMC, STA, DPSMACT), (cmder->error == SDCmderError_data_timeout)))
                    // The DPSM should only be still enabled
                    // if and only if there's a data timeout.
                    // We'll need to send a STOP_TRANSMISSION
                    // later to get it back to idle state.
                    bug;

                if (!cmder->error && cmder->remaining)
                    // We should've transmitted/received
                    // everything if everything's swell.
                    bug;

                // Either we're aborting or we transferred everything.
                cmder->state = SDCmderState_command_attempted;
            }

            ret(again);

        } break;



        ////////////////////////////////////////////////////////////////////////////////
        //
        // The SD card is busy executing our command...
        //



        case SDCmderState_postcommand_d0_busy:
        {

            if (STATUS_POP(DTIMEOUT))
            {
                 // Busy signal was active for too long.
                 // TODO Different error?
                 // TODO This would be overwriting any errors, this ok?
                cmder->error = SDCmderError_data_timeout;
            }
            else if (!CMSIS_GET_FROM(status_snapshot, SDMMC, STA, BUSYD0))
            {
                // Busy signal lifted!
                STATUS_POP(BUSYD0END);
            }
            else if (status_snapshot & SDMMC->MASK)
            {
                // There should be no other interrupt event.
                bug;
            }
            else
            {
                // No SDMMC interrupt event of interest actually happened.
                ret(yield);
            }

            // The busy signal has now been
            // waited out (or an error occured).
            // We can get back to being done
            // with the command now.
            cmder->state = SDCmderState_command_attempted;

            ret(again);

        } break;



        ////////////////////////////////////////////////////////////////////////////////
        //
        // We have finished attempting the command.
        //



        case SDCmderState_command_attempted:
        {

            if (!iff(cmder->stopping_transmission, cmder->effective_cmd == SDCmd_STOP_TRANSMISSION))
                bug;

            if (CMSIS_GET_FROM(status_snapshot, SDMMC, STA, BUSYD0END) || CMSIS_GET_FROM(status_snapshot, SDMMC, STA, BUSYD0))
            {
                cmder->state = SDCmderState_postcommand_d0_busy; // @/`Busy Command Attempted`.
                ret(again);
            }
            else if (status_snapshot & SDMMC->MASK)
            {
                bug; // There should be no other interrupt event.
            }
            else if (CMSIS_GET_FROM(status_snapshot, SDMMC, STA, DPSMACT)) // @/`Sending STOP_TRANSMISSION`.
            {

                if (cmder->stopping_transmission)
                    ret(sdmmc_needs_reinit); // @/`Unacknowledged STOP_TRANSMISSION Command`.
                if (!cmder->error)
                    bug;
                cmder->state                 = SDCmderState_scheduled_command;
                cmder->stopping_transmission = true;

                ret(again);

            }
            else
            {
                cmder->state = SDCmderState_ready_for_next_command;
                ret(command_attempted); // The caller's desired command has been fully attempted.
            }

        } break;



        default: bug;

    }

}



////////////////////////////////////////////////////////////////////////////////



// @/`Busy Command Attempted`:
//
// Even when we're finished with the command, we still need to
// take the precaution of waiting out the busy signal reported by SDMMC.
// As far as I can tell, typically happens when there's some sort of
// communication error happening on the data line that confuses SDMMC,
// but it might be entirely possible that it happens normally too.



// @/`Sending STOP_TRANSMISSION`:
//
// Whenever there's an error, the CPSM will be put back into the idle
// state automatically, but the DPSM will not be until a STOP_TRANSMISSION
// is sent if the DPSM is being used.
//
// @/pg 2756/tbl 622/`H7S3rm`.
// @/pg 2435/tbl 484/`H730rm`.



// @/`Unacknowledged STOP_TRANSMISSION Command`:
//
// When sending data to the SD card, we have to consider the edge case
// where the SD card can become disconnected. When this happens, the SD
// command (which will enable the DPSM) will generally end up with a
// timed-out response. The RM then states that the firmware will need to
// send a STOP_TRANSMISSION to reset the DPSM back to the idle state. For
// SD commands that receive data, enabling the CPSM for STOP_TRANSMISSION
// seems to always disable the DPSM thereafter, even if the STOP_TRANSMISSION
// command itself doesn't actually succeed (because the SD card is gone).
// For some reason, however, this does not seem to apply when the original
// SD command was going to transmit data. In other words, there seems to be
// a difference for the DPSM when it is in the Wait_R vs. Wait_S states when
// trying to get it back to idle using STOP_TRANSMISSION but the STOP_TRANSMISSION
// is not acknowledged.
//
// I've tried resetting the FIFO and also sending the STOP_TRANSMISSION command
// multiple times, but nothing seem to make a difference in resetting the DPSM,
// but it just might be bugged. This very well could be an errata, since this
// is probably hitting a really specific edge case of SDMMC, as the way I'm
// testing it is by wiggling the SD card in and out of the connector. More tests
// could be done to narrow done on the specifics of what's causing this issue,
// but for now, a hard reset of the SDMMC peripheral seems like the easiest way
// to get around this.
//
// @/pg 2759/fig 864/`H7S3rm`.
// @/pg 2438/fig 742/`H730rm`.
