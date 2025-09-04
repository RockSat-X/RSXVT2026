(
    (

        ################################################################################
        #
        # Peripheral counts.
        # @/pg 456/fig 52/`H533rm`.
        # @/pg 15/tbl 2/`H533ds`.
        #

        ('APBS', (
            1,
            2,
            3,
        )),

        ('PLLS', (
            (1, ('P', 'Q', 'R')),
            (2, ('P', 'Q', 'R')),
            (3, ('P', 'Q', 'R')),
        )),

        ('I2CS', (
            1,
            2,
            3,
        )),

        ('UXARTS', (
            (('USART', 1),),
            (('USART', 2),),
            (('USART', 3),),
            (('UART' , 4),),
            (('UART' , 5),),
            (('USART', 6),),
        )),

        ################################################################################
        #
        # Common values.
        #

        ('GPIO_MODE', (
            ('INPUT'    , '0b00'),
            ('OUTPUT'   , '0b01'),
            ('ALTERNATE', '0b10'),
            ('ANALOG'   , '0b11'),
        )),

        ('GPIO_SPEED', (
            ('LOW'      , '0b00'),
            ('MEDIUM'   , '0b01'),
            ('HIGH'     , '0b10'),
            ('VERY_HIGH', '0b11'),
        )),

        ('GPIO_PULL', (
            (None  , '0b00'),
            ('UP'  , '0b01'),
            ('DOWN', '0b10'),
        )),

        ################################################################################
        #
        # Frequency limits.
        # @/pg 124/tbl 47/`H533ds`.
        # TODO We're assuming a high internal voltage and wide range.
        #

        ('PLL_CHANNEL_FREQ',   1_000_000, 250_000_000),
        ('PLL_VCO_FREQ'    , 128_000_000, 560_000_000),
        ('CPU_FREQ'        ,           0, 250_000_000),
        ('AXI_AHB_FREQ'    ,           0, 250_000_000),
        ('APB_FREQ'        ,           0, 250_000_000),

    ),
    (

        ################################################################################

        ('SysTick',

            ('LOAD',
                ('RELOAD', 'SYSTICK_RELOAD', 1, (1 << 24) - 1),
            ),

            ('VAL',
                ('CURRENT', 'SYSTICK_COUNTER', 0, (1 << 32) - 1),
            ),

            ('CTRL',
                ('CLKSOURCE', 'SYSTICK_USE_CPU_CK'      ),
                ('TICKINT'  , 'SYSTICK_INTERRUPT_ENABLE'),
                ('ENABLE'   , 'SYSTICK_ENABLE'          ),
            ),

        ),

        ################################################################################

        ('FLASH',

            ('ACR',
                ('WRHIGHFREQ', 'FLASH_PROGRAMMING_DELAY', (
                    '0b00',
                    '0b01',
                    '0b10',
                )),
                ('LATENCY', 'FLASH_LATENCY', 0b0000, 0b1111),
            ),

        ),

        ################################################################################

        ('PWR',

            ('VOSCR',
                ('VOS', 'INTERNAL_VOLTAGE_SCALING'),
            ),

            ('VOSSR',
                ('ACTVOS'   , 'CURRENT_ACTIVE_VOS'      ),
                ('ACTVOSRDY', 'CURRENT_ACTIVE_VOS_READY'),
            ),

            ('SCCR',
                ('LDOEN' , 'LDO_ENABLE'             ),
                ('BYPASS', 'POWER_MANAGEMENT_BYPASS'),
            ),

        ),

        ################################################################################

        ('RCC',

            ('CR',
                ('PLL3RDY' , 'PLL3_READY'  ),
                ('PLL3ON'  , 'PLL3_ENABLE' ),
                ('PLL2RDY' , 'PLL2_READY'  ),
                ('PLL2ON'  , 'PLL2_ENABLE' ),
                ('PLL1RDY' , 'PLL1_READY'  ),
                ('PLL1ON'  , 'PLL1_ENABLE' ),
                ('HSI48RDY', 'HSI48_READY' ),
                ('HSI48ON' , 'HSI48_ENABLE'),
                ('CSIRDY'  , 'CSI_READY'   ),
                ('CSION'   , 'CSI_ENABLE'  ),
                ('HSIRDY'  , 'HSI_READY'   ),
                ('HSION'   , 'HSI_ENABLE'  ),
            ),

            ('CFGR1',
                *(
                    (field, tag, (
                        ('HSI_CK'   , '0b000'),
                        ('CSI_CK'   , '0b001'),
                        ('HSE_CK'   , '0b010'),
                        ('PLL1_P_CK', '0b011'),
                    ))
                    for field, tag in (
                        ('SWS', 'EFFECTIVE_SCGU_CLOCK_SOURCE'),
                        ('SW' , 'SCGU_CLOCK_SOURCE'          ),
                    )
                ),
            ),

            ('CFGR2',
                *(
                    (f'PPRE{unit}', f'APB{unit}_DIVIDER', (
                        (1 , '0b000'),
                        (2 , '0b100'),
                        (4 , '0b101'),
                        (8 , '0b110'),
                        (16, '0b111'),
                    ))
                    for unit in (1, 2, 3)
                ),
                ('HPRE', 'CPU_DIVIDER', (
                    (1  , '0b0000'), # Low three bits are don't-care.
                    (2  , '0b1000'),
                    (4  , '0b1001'),
                    (8  , '0b1010'),
                    (16 , '0b1011'),
                    (64 , '0b1100'),
                    (128, '0b1101'),
                    (256, '0b1110'),
                    (512, '0b1111'),
                )),
            ),

            *(
                (f'PLL{unit}CFGR',
                    (f'PLL{unit}REN', f'PLL{unit}R_ENABLE'),
                    (f'PLL{unit}QEN', f'PLL{unit}Q_ENABLE'),
                    (f'PLL{unit}PEN', f'PLL{unit}P_ENABLE'),
                    (f'PLL{unit}M'  , f'PLL{unit}_PREDIVIDER', 1, 63),
                    (f'PLL{unit}SRC', f'PLL{unit}_CLOCK_SOURCE', (
                        (None    , '0b00'),
                        ('HSI_CK', '0b01'),
                        ('CSI_CK', '0b10'),
                        ('HSE_CK', '0b11'),
                    )),
                    (f'PLL{unit}RGE', f'PLL{unit}_INPUT_RANGE', (
                        ( 1_000_000, None),
                        ( 2_000_000, None), # TODO Can be '0b00', but only for medium VCO. @/pg 124/tbl 47/`H533rm`.
                        ( 4_000_000, 0b01),
                        ( 8_000_000, 0b10),
                        (16_000_000, 0b11),
                    )),
                )
                for unit in (1, 2, 3)
            ),

            *(
                (f'PLL{unit}DIVR',
                    (f'PLL{unit}R', f'PLL{unit}R_DIVIDER'  , 1, 128),
                    (f'PLL{unit}Q', f'PLL{unit}Q_DIVIDER'  , 1, 128),
                    (f'PLL{unit}P', f'PLL{unit}P_DIVIDER'  , 1, 128),
                    (f'PLL{unit}N', f'PLL{unit}_MULTIPLIER', 4, 512),
                )
                for unit in (1, 2, 3)
            ),

            ('APB1LENR',
                ('USART2EN', 'UXART_2_ENABLE'),
                ('I2C1EN'  , 'I2C1_ENABLE'   ),
                ('I2C2EN'  , 'I2C2_ENABLE'   ),
            ),

            ('CCIPR1',
                *(
                    (field, f'UXART_{peripherals}_CLOCK_SOURCE', clock_source)
                    for field_peripherals, clock_source in
                    (
                        (
                            (
                                ('USART10SEL', (('USART', 10),)),
                                ('UART9SEL'  , (('UART' , 9 ),)),
                                ('UART8SEL'  , (('UART' , 8 ),)),
                                ('UART7SEL'  , (('UART' , 7 ),)),
                                ('USART6SEL' , (('USART', 6 ),)),
                                ('UART5SEL'  , (('UART' , 5 ),)),
                                ('UART4SEL'  , (('UART' , 4 ),)),
                                ('USART3SEL' , (('USART', 3 ),)),
                                ('USART2SEL' , (('USART', 2 ),)),
                            ),
                            (
                                ('APB1_CK'   , '0b000'),
                                ('PLL2_Q_CK' , '0b001'),
                                ('PLL3_Q_CK' , '0b010'),
                                ('HSI_CK'    , '0b011'),
                                ('CSI_CK'    , '0b100'),
                                ('LSE_CK'    , '0b101'),
                                (None        , '0b110'),
                            ),
                        ),
                        (
                            (
                                ('USART1SEL', (('USART', 1),)),
                            ),
                            (
                                ('RCC_PCLK2' , '0b000'),
                                ('PLL2_Q_CK' , '0b001'),
                                ('PLL3_Q_CK' , '0b010'),
                                ('HSI_CK'    , '0b011'),
                                ('CSI_CK'    , '0b100'),
                                ('LSE_CK'    , '0b101'),
                                (None        , '0b110'),
                            ),
                        ),
                    )
                    for field, peripherals in field_peripherals
                ),
            ),

            ('CCIPR4',
                ('I2C3SEL', 'I2C3_KERNEL_SOURCE', (
                    ('APB3_CK'  , '0b00'),
                    ('PLL3_R_CK', '0b01'),
                    ('HSI_CK'   , '0b10'),
                    ('CSI_CK'   , '0b11'),
                )),
                ('I2C2SEL', 'I2C2_KERNEL_SOURCE', (
                    ('APB1_CK'  , '0b00'),
                    ('PLL3_R_CK', '0b01'),
                    ('HSI_CK'   , '0b10'),
                    ('CSI_CK'   , '0b11'),
                )),
                ('I2C1SEL', 'I2C1_KERNEL_SOURCE', (
                    ('APB1_CK'  , '0b00'),
                    ('PLL3_R_CK', '0b01'),
                    ('HSI_CK'   , '0b10'),
                    ('CSI_CK'   , '0b11'),
                )),
            ),

            ('CCIPR5',
                ('CKPERSEL', 'PER_CK_SOURCE', (
                    ('HSI_CK', '0b00'),
                    ('CSI_CK', '0b01'),
                    ('HSE_CK', '0b10'),
                    (None    , '0b11'),
                )),
            ),

            ('AHB2ENR',
                *(
                    (f'GPIO{port}EN', f'GPIO{port}_ENABLE')
                    for port in 'ABCDEFGHI'
                ),
            ),

            ('APB1LRSTR',
                ('I2C1RST'  , 'I2C1_RESET'  ),
                ('I2C2RST'  , 'I2C2_RESET'  ),
                ('UART5RST' , 'UART5_RESET' ),
                ('UART4RST' , 'UART4_RESET' ),
                ('USART3RST', 'USART3_RESET'),
                ('USART2RST', 'USART2_RESET'),
            ),

        ),

        ################################################################################

        ('USART',
            ('BRR',
                ('BRR', 'UXART_BAUD_DIVIDER', 1, 1 << 16),
            ),
        ),

        ################################################################################

        ('I2C',
            ('TIMINGR',
                ('PRESC', 'I2C_PRESC', 0, 15 ),
                ('SCLH' , 'I2C_SCLH' , 0, 255),
                ('SCLL' , 'I2C_SCLL' , 0, 255),
            ),
        ),

    ),
)
