extern void
I2C_reinit(void)
{



    // Reset-cycle the peripheral.

    CMSIS_SET(RCC, APB1LRSTR, I2C1RST, true );
    CMSIS_SET(RCC, APB1LRSTR, I2C1RST, false);



}
