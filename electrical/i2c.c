extern void
I2C_reinit(void)
{

    // TODO Ad-hoc.

    #define I2C1_TIMINGR_PRESC_init 11
    #define I2C1_TIMINGR_SCL_init   250
    CMSIS_SET(RCC, CCIPR4, I2C1SEL, 0b10);


    // Reset-cycle the peripheral.

    CMSIS_SET(RCC, APB1LRSTR, I2C1RST, true );
    CMSIS_SET(RCC, APB1LRSTR, I2C1RST, false);



    // Enable the interrupts.

    NVIC_ENABLE(I2C1_EV);
    NVIC_ENABLE(I2C1_ER);



    // Clock the peripheral.

    CMSIS_SET(RCC, APB1LENR, I2C1EN, true);



    // Configure the peripheral.

    CMSIS_SET
    (
        I2C1  , TIMINGR                , // TODO Handle other timing requirements?
        PRESC , I2C1_TIMINGR_PRESC_init, // Set the time base unit.
        SCLDEL, 0                      , // TODO Important?
        SDADEL, 0                      , // TODO Important?
        SCLH  , I2C1_TIMINGR_SCL_init  , // Determines the amount of high time.
        SCLL  , I2C1_TIMINGR_SCL_init  , // Determines the amount of low time.
    );

    CMSIS_SET
    (
        I2C1  , CR1   , // Interrupts for:
        ERRIE , true  , //     - Error.
        TCIE  , true  , //     - Transfer completed.
        STOPIE, true  , //     - STOP signal.
        NACKIE, true  , //     - NACK signal.
        RXIE  , true  , //     - Reception of data.
        TXIE  , true  , //     - Transmission of data.
        DNF   , 15    , // Max out the digital filtering.
        PE    , true  , // Enable the peripheral.
    );



    // TODO Ad-hoc.

    u8  address       = 0x78;
    b32 is_read       = true;
    i32 transfer_size = 8;

    CMSIS_SET
    (
        I2C1  , CR2          ,
        SADD  , address      ,
        RD_WRN, is_read      ,
        NBYTES, transfer_size,
        START , true         ,
    );

}

INTERRUPT_I2C1_EV
{
    sorry
}

INTERRUPT_I2C1_ER
{
    sorry
}
