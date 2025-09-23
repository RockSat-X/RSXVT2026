#include "sd.meta"



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
