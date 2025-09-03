(
    (

        ################################################################################
        #
        # Peripheral counts.
        # @/pg 456/fig 52/`H533rm`.
        # @/pg 15/tbl 2/`H533ds`.
        #

        ('APB_UNITS', (
            1,
            2,
            3,
        )),

        ('PLL_UNITS', (
            (1, ('p', 'q', 'r')),
            (2, ('p', 'q', 'r')),
            (3, ('p', 'q', 'r')),
        )),

        ('I2CS', (
            1,
            2,
            3,
        )),

        ('UXARTS', (
            (('usart', 1),),
            (('usart', 2),),
            (('usart', 3),),
            (('uart' , 4),),
            (('uart' , 5),),
            (('usart', 6),),
        )),

        ################################################################################
        #
        # Common values.
        #

        ('GPIO_MODE', (
            ('input'    , '0b00'),
            ('output'   , '0b01'),
            ('alternate', '0b10'),
            ('analog'   , '0b11'),
        )),

        ('GPIO_SPEED', (
            ('low'      , '0b00'),
            ('medium'   , '0b01'),
            ('high'     , '0b10'),
            ('very_high', '0b11'),
        )),

        ('GPIO_PULL', (
            (None  , '0b00'),
            ('up'  , '0b01'),
            ('down', '0b10'),
        )),

        ################################################################################
        #
        # Frequency limits.
        # @/pg 124/tbl 47/`H533ds`.
        # TODO We're assuming a high internal voltage and wide range.
        #

        ('pll_channel_freq',   1_000_000, 250_000_000),
        ('pll_vco_freq'    , 128_000_000, 560_000_000),
        ('cpu_freq'        ,           0, 250_000_000),
        ('axi_ahb_freq'    ,           0, 250_000_000),
        ('apb_freq'        ,           0, 250_000_000),

    ),
    (

        ################################################################################

        ('SysTick',

            ('LOAD',
                ('RELOAD', 'systick_reload', 1, (1 << 24) - 1),
            ),

            ('VAL',
                ('CURRENT', 'systick_counter', 0, (1 << 32) - 1),
            ),

            ('CTRL',
                ('CLKSOURCE', 'systick_use_cpu_ck'      ),
                ('TICKINT'  , 'systick_interrupt_enable'),
                ('ENABLE'   , 'systick_enable'          ),
            ),

        ),

        ################################################################################

        ('FLASH',

            ('ACR',
                ('WRHIGHFREQ', 'flash_programming_delay', (
                    0b00,
                    0b01,
                    0b10,
                )),
                ('LATENCY', 'flash_latency', 0b0000, 0b1111),
            ),

        ),

        ################################################################################

        ('PWR',

            ('VOSCR',
                ('VOS', 'internal_voltage_scaling'),
            ),

            ('VOSSR',
                ('ACTVOS'   , 'current_active_vos'      ),
                ('ACTVOSRDY', 'current_active_vos_ready'),
            ),

            ('SCCR',
                ('LDOEN' , 'ldo_enable'             ),
                ('BYPASS', 'power_management_bypass'),
            ),

        ),

        ################################################################################

        ('RCC',

            ('CR',
                ('PLL3RDY' , 'pll3_ready'  ),
                ('PLL3ON'  , 'pll3_enable' ),
                ('PLL2RDY' , 'pll2_ready'  ),
                ('PLL2ON'  , 'pll2_enable' ),
                ('PLL1RDY' , 'pll1_ready'  ),
                ('PLL1ON'  , 'pll1_enable' ),
                ('HSI48RDY', 'hsi48_ready' ),
                ('HSI48ON' , 'hsi48_enable'),
                ('CSIRDY'  , 'csi_ready'   ),
                ('CSION'   , 'csi_enable'  ),
                ('HSIRDY'  , 'hsi_ready'   ),
                ('HSION'   , 'hsi_enable'  ),
            ),

            ('CFGR1',
                *(
                    (field, name, (
                        ('hsi_ck'   , '0b000'),
                        ('csi_ck'   , '0b001'),
                        ('hse_ck'   , '0b010'),
                        ('pll1_p_ck', '0b011'),
                    ))
                    for field, name in (
                        ('SWS', 'effective_scgu_clock_source'),
                        ('SW' , 'scgu_clock_source'          ),
                    )
                ),
            ),

            ('CFGR2',
                *(
                    (f'PPRE{unit}', f'apb{unit}_divider', (
                        (1 , '0b000'),
                        (2 , '0b100'),
                        (4 , '0b101'),
                        (8 , '0b110'),
                        (16, '0b111'),
                    ))
                    for unit in (1, 2, 3)
                ),
                ('HPRE', 'cpu_divider', (
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
                    (f'PLL{unit}REN', f'pll{unit}r_enable'),
                    (f'PLL{unit}QEN', f'pll{unit}q_enable'),
                    (f'PLL{unit}PEN', f'pll{unit}p_enable'),
                    (f'PLL{unit}M'  , f'pll{unit}_predivider', 1, 63),
                    (f'PLL{unit}SRC', f'pll{unit}_clock_source', (
                        (None    , '0b00'),
                        ('hsi_ck', '0b01'),
                        ('csi_ck', '0b10'),
                        ('hse_ck', '0b11'),
                    )),
                    (f'PLL{unit}RGE', f'pll{unit}_input_range', (
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
                    (f'PLL{unit}R', f'pll{unit}r_divider'  , 1, 128),
                    (f'PLL{unit}Q', f'pll{unit}q_divider'  , 1, 128),
                    (f'PLL{unit}P', f'pll{unit}p_divider'  , 1, 128),
                    (f'PLL{unit}N', f'pll{unit}_multiplier', 4, 512),
                )
                for unit in (1, 2, 3)
            ),

            ('APB1LENR',
                ('USART2EN', 'uxart_2_enable'),
                ('I2C1EN'  , 'i2c1_enable'   ),
                ('I2C2EN'  , 'i2c2_enable'   ),
            ),

            ('CCIPR1',
                *(
                    (field, f'uxart_{peripherals}_clock_source', clock_source)
                    for field_peripherals, clock_source in
                    (
                        (
                            (
                                ('USART10SEL', (('usart', 10),)),
                                ('UART9SEL'  , (('uart' , 9 ),)),
                                ('UART8SEL'  , (('uart' , 8 ),)),
                                ('UART7SEL'  , (('uart' , 7 ),)),
                                ('USART6SEL' , (('usart', 6 ),)),
                                ('UART5SEL'  , (('uart' , 5 ),)),
                                ('UART4SEL'  , (('uart' , 4 ),)),
                                ('USART3SEL' , (('usart', 3 ),)),
                                ('USART2SEL' , (('usart', 2 ),)),
                            ),
                            (
                                ('apb1_ck'   , '0b000'),
                                ('pll2_q_ck' , '0b001'),
                                ('pll3_q_ck' , '0b010'),
                                ('hsi_ck'    , '0b011'),
                                ('csi_ck'    , '0b100'),
                                # TODO: ('lse_ck', '0b101'),
                                (None        , '0b110'),
                            ),
                        ),
                        (
                            (
                                ('USART1SEL', (('usart', 1),)),
                            ),
                            (
                                ('rcc_pclk2' , '0b000'),
                                ('pll2_q_ck' , '0b001'),
                                ('pll3_q_ck' , '0b010'),
                                ('hsi_ck'    , '0b011'),
                                ('csi_ck'    , '0b100'),
                                # TODO: ('lse_ck'    , '0b101'),
                                (None        , '0b110'),
                            ),
                        ),
                    )
                    for field, peripherals in field_peripherals
                ),
            ),

            ('CCIPR4',
                ('I2C3SEL', 'i2c3_clock_source', (
                    ('apb3_ck'   , '0b00'),
                    ('pll3_r_ck' , '0b01'),
                    ('hsi_ker_ck', '0b10'),
                    ('csi_ker_ck', '0b11'),
                )),
                ('I2C2SEL', 'i2c2_clock_source', (
                    ('apb1_ck'   , '0b00'),
                    ('pll3_r_ck' , '0b01'),
                    ('hsi_ker_ck', '0b10'),
                    ('csi_ker_ck', '0b11'),
                )),
                ('I2C1SEL', 'i2c1_clock_source', (
                    ('apb1_ck'   , '0b00'),
                    ('pll3_r_ck' , '0b01'),
                    ('hsi_ker_ck', '0b10'),
                    ('csi_ker_ck', '0b11'),
                )),
            ),

            ('CCIPR5',
                ('CKPERSEL', 'per_ck_source', (
                    ('hsi_ck', '0b00'),
                    ('csi_ck', '0b01'),
                    (None    , '0b11'),
                    # TODO hse_ck.
                )),
            ),

            ('AHB2ENR',
                *(
                    (f'GPIO{port}EN', f'gpio{port}_enable')
                    for port in 'ABCDEFGHI'
                ),
            ),

            ('APB1LRSTR',
                ('I2C1RST', 'i2c1_reset'),
                ('I2C2RST', 'i2c2_reset'),
            ),

        ),

        ################################################################################

        ('USART',
            ('BRR',
                ('BRR', 'uxart_baud_divider', 1, 1 << 16),
            ),
        ),

        ################################################################################

        ('I2C',
            ('TIMINGR',
                ('PRESC', 'i2c_prescaler', 0, 15 ),
                ('SCLH' , 'i2c_SCH'      , 0, 255),
                ('SCLL' , 'i2c_SCL'      , 0, 255),
            ),
        ),

    ),
)
