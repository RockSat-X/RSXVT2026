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
    while (true)
    {

        u32 i2c_status = I2C1->ISR;



        // Bus error.
        // @/pg 2116/sec 48.4.17/`H533rm`.

        if (CMSIS_GET_FROM(i2c_status, I2C, ISR, BERR))
        {
            sorry
        }



        // Arbitration loss.
        // @/pg 2116/sec 48.4.17/`H533rm`.

        else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, ARLO))
        {
            sorry
        }



        // Data overrun/overflow.
        // @/pg 2117/sec 48.4.17/`H533rm`.

        else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, OVR))
        {
            sorry
        }



        // Packet error checking.
        // @/pg 2117/sec 48.4.17/`H533rm`.

        else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, PECERR))
        {
            sorry
        }



        // Timeout.
        // @/pg 2117/sec 48.4.17/`H533rm`.

        else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, TIMEOUT))
        {
            sorry
        }



        // Alert.
        // @/pg 2117/sec 48.4.17/`H533rm`.

        else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, ALERT))
        {
            sorry
        }



        // A NACK was received.
        // @/pg 2085/sec 48.4.8/`H533rm`.

        else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, NACKF))
        {
            sorry
        }



        // A STOP was sent.
        // @/pg 2085/sec 48.4.8/`H533rm`.

        else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, STOPF))
        {
            sorry
        }



        // Transfer completed.
        // @/pg 2095/sec 48.4.9/`H533rm`.

        else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, TC))
        {
            sorry
        }



        // Transfer reloaded.
        // @/pg 2095/sec 48.4.9/`H533rm`.

        else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, TCR))
        {
            sorry
        }



        // Received data.
        // @/pg 2089/sec 48.4.8/`H533rm`.

        else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, RXNE))
        {
            sorry
        }



        // Data can be sent.
        // @/pg 2085/sec 48.4.8/`H533rm`.

        else if (CMSIS_GET_FROM(i2c_status, I2C, ISR, TXIS))
        {
            sorry
        }

    }
}

INTERRUPT_I2C1_ER
{
    sorry
}
