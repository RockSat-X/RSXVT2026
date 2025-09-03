(
    (
        ################################################################################
        #
        # Peripheral counts.
        # @/pg 354/fig 40/`H7S3rm`.
        #

        ('APBS', (
            1,
            2,
            4,
            5,
        )),

        ('PLLS', (
            (1, ('P', 'Q',      'S',    )),
            (2, ('P', 'Q', 'R', 'S', 'T')),
            (3, ('P', 'Q', 'R', 'S',    )),
        )),

        ('UXARTS', (
            (('USART', 1),),
            (('USART', 2), ('USART', 3), ('UART', 4), ('UART', 5), ('UART', 7), ('UART', 8)),
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
        # @/pg 39/tbl 6/`H7S3ds`.
        # TODO We're assuming a high internal voltage.
        # TODO Assuming wide frequency range.
        # TODO 600MHz only when ECC is disabled.
        #

        ('SDMMC_KERNEL_FREQ', 0          , 200_000_000),
        ('CPU_FREQ'         , 0          , 600_000_000),
        ('AXI_AHB_FREQ'     , 0          , 300_000_000),
        ('APB_FREQ'         , 0          , 150_000_000),
        ('PLL_CHANNEL_FREQ' , 1_500_000  , 600_000_000),
        ('PLL_VCO_FREQ'     , 192_000_000, 836_000_000),

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
                    '0b11'
                )),
                ('LATENCY', 'FLASH_LATENCY', 0b0000, 0b1111),
            ),

        ),

        ################################################################################

        ('PWR',

            ('SR1',
                ('ACTVOS'   , 'CURRENT_ACTIVE_VOS'      ),
                ('ACTVOSRDY', 'CURRENT_ACTIVE_VOS_READY'),
            ),

            ('CSR2',
                ('SDHILEVEL', 'SMPS_OUTPUT_LEVEL'      ),
                ('SMPSEXTHP', 'SMPS_FORCED_ON'         ),
                ('SDEN'     , 'SMPS_ENABLE'            ),
                ('LDOEN'    , 'LDO_ENABLE'             ),
                ('BYPASS'   , 'POWER_MANAGEMENT_BYPASS'),
            ),

            ('CSR4',
                ('VOS', 'INTERNAL_VOLTAGE_SCALING'),
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
            ),

            ('CFGR',
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

            *(
                (register,
                    (field, tag, (
                        (1  , '0b0000'), # Low three bits are don't-care.
                        (2  , '0b1000'),
                        (4  , '0b1001'),
                        (8  , '0b1010'),
                        (16 , '0b1011'),
                        (64 , '0b1100'),
                        (128, '0b1101'),
                        (256, '0b1110'),
                        (512, '0b1111'),
                    ))
                )
                for register, field, tag in (
                    ('CDCFGR', 'CPRE' , 'CPU_DIVIDER'    ),
                    ('BMCFGR', 'BMPRE', 'AXI_AHB_DIVIDER'),
                )
            ),

            ('APBCFGR',
                *(
                    (f'PPRE{unit}', f'APB{unit}_DIVIDER', (
                        (1 , '0b000'),
                        (2 , '0b100'),
                        (4 , '0b101'),
                        (8 , '0b110'),
                        (16, '0b111'),
                    ))
                    for unit in (1, 2, 4, 5)
                ),
            ),

            ('AHB4ENR',
                *(
                    (f'GPIO{port}EN', f'GPIO{port}_ENABLE')
                    for port in 'ABCDEFGHI'
                ),
            ),

            ('APB1ENR1',
                ('USART3EN', 'UXART_3_ENABLE'),
            ),

            ('PLLCFGR',
                *(
                    (f'PLL{unit}RGE', f'PLL{unit}_INPUT_RANGE', (
                        ( 1_000_000, None),
                        ( 2_000_000, None), # Can be '0b00', but only for medium VCO.
                        ( 4_000_000, 0b01),
                        ( 8_000_000, 0b10),
                        (16_000_000, 0b11),
                    ))
                    for unit in (1, 2, 3)
                ),
                *(
                    (f'PLL{unit}{channel}EN', f'PLL{unit}{channel}_ENABLE')
                    for unit, channels in (
                        (1, ('P', 'Q',      'S'     )),
                        (2, ('P', 'Q', 'R', 'S', 'T')),
                        (3, ('P', 'Q', 'R', 'S'     )),
                    )
                    for channel in channels
                ),
            ),

            ('PLLCKSELR',
                ('DIVM1' , 'PLL1_PREDIVIDER' , 1, 63),
                ('DIVM2' , 'PLL2_PREDIVIDER' , 1, 63),
                ('DIVM3' , 'PLL3_PREDIVIDER' , 1, 63),
                ('PLLSRC', 'PLL_CLOCK_SOURCE', (
                    ('HSI_CK' , '0b00'),
                    ('CSI_CK' , '0b01'),
                    ('HSE_CK' , '0b10'),
                    (None     , '0b11'),
                )),
            ),

            *(
                (f'PLL{unit}DIVR1',
                    ('DIVN', f'PLL{unit}_MULTIPLIER', 12, 420),
                )
                for unit in (1, 2, 3)
            ),

            *(
                (f'PLL{unit}DIVR{2 if channel in ('S', 'T') else 1}',
                    (f'DIV{channel}', f'PLL{unit}{channel}_DIVIDER', 1, 128),
                )
                for unit, channels in (
                    (1, ('P', 'Q',      'S'     )),
                    (2, ('P', 'Q', 'R', 'S', 'T')),
                    (3, ('P', 'Q', 'R', 'S'     )),
                )
                for channel in channels
            ),

            ('CCIPR1',
                ('CKPERSEL', 'PER_CK_SOURCE', (
                    ('HSI_CK' , '0b00'),
                    ('CSI_CK' , '0b01'),
                    ('HSE_CK' , '0b10'),
                    (None     , '0b11'),
                )),
                ('SDMMC12SEL', 'SDMMC_CLOCK_SOURCE', (
                    ('PLL2_S_CK', '0b0'),
                    ('PLL2_T_CK', '0b1'),
                )),
            ),

            ('CCIPR2',
                ('UART234578SEL', f'UXART_{(('USART', 2), ('USART', 3), ('UART', 4), ('UART', 5), ('UART', 7), ('UART', 8))}_CLOCK_SOURCE', (
                    ('APB2_CK'  , '0b000'),
                    ('PLL2_Q_CK', '0b001'),
                    ('PLL3_Q_CK', '0b010'),
                    ('HSI_CK'   , '0b011'),
                    ('CSI_CK'   , '0b100'),
                    ('LSE_CK'   , '0b101'),
                )),
            ),

        ),

        ################################################################################

        ('USART',
            ('BRR',
                ('BRR', 'UXART_BAUD_DIVIDER', 1, 1 << 16),
            ),
        ),

    ),
)
