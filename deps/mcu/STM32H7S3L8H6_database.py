(
    (
        ################################################################################
        #
        # Peripheral counts.
        # @/pg 354/fig 40/`H7S3rm`.
        #

        ('APB_UNITS', (
            1,
            2,
            4,
            5,
        )),

        ('PLL_UNITS', (
            (1, ('p', 'q',      's',    )),
            (2, ('p', 'q', 'r', 's', 't')),
            (3, ('p', 'q', 'r', 's',    )),
        )),

        ('UXARTS', (
            (('usart', 1),),
            (('usart', 2), ('usart', 3), ('uart', 4), ('uart', 5), ('uart', 7), ('uart', 8)),
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
        # @/pg 39/tbl 6/`H7S3ds`.
        # TODO We're assuming a high internal voltage.
        # TODO Assuming wide frequency range.
        # TODO 600MHz only when ECC is disabled.
        #

        ('sdmmc_kernel_freq', 0          , 200_000_000),
        ('cpu_freq'         , 0          , 600_000_000),
        ('axi_ahb_freq'     , 0          , 300_000_000),
        ('apb_freq'         , 0          , 150_000_000),
        ('pll_channel_freq' , 1_500_000  , 600_000_000),
        ('pll_vco_freq'     , 192_000_000, 836_000_000),

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
                    '0b00',
                    '0b11'
                )),
                ('LATENCY', 'flash_latency', 0b0000, 0b1111),
            ),

        ),

        ################################################################################

        ('PWR',

            ('SR1',
                ('ACTVOS'   , 'current_active_vos'      ),
                ('ACTVOSRDY', 'current_active_vos_ready'),
            ),

            ('CSR2',
                ('SDHILEVEL', 'smps_output_level'      ),
                ('SMPSEXTHP', 'smps_forced_on'         ),
                ('SDEN'     , 'smps_enable'            ),
                ('LDOEN'    , 'ldo_enable'             ),
                ('BYPASS'   , 'power_management_bypass'),
            ),

            ('CSR4',
                ('VOS', 'internal_voltage_scaling'),
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
            ),

            ('CFGR',
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

            *(
                (register,
                    (field, name, (
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
                for register, field, name in (
                    ('CDCFGR', 'CPRE' , 'cpu_divider'    ),
                    ('BMCFGR', 'BMPRE', 'axi_ahb_divider'),
                )
            ),

            ('APBCFGR',
                *(
                    (f'PPRE{unit}', f'apb{unit}_divider', (
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
                    (f'GPIO{port}EN', f'gpio{port}_enable')
                    for port in 'ABCDEFGHI'
                ),
            ),

            ('APB1ENR1',
                ('USART3EN', 'uxart_3_enable'),
            ),

            ('PLLCFGR',
                *(
                    (f'PLL{unit}RGE', f'pll{unit}_input_range', (
                        ( 1_000_000, None),
                        ( 2_000_000, None), # Can be '0b00', but only for medium VCO.
                        ( 4_000_000, 0b01),
                        ( 8_000_000, 0b10),
                        (16_000_000, 0b11),
                    ))
                    for unit in (1, 2, 3)
                ),
                *(
                    (f'PLL{unit}{channel.upper()}EN', f'pll{unit}{channel}_enable')
                    for unit, channels in (
                        (1, ('p', 'q',      's'     )),
                        (2, ('p', 'q', 'r', 's', 't')),
                        (3, ('p', 'q', 'r', 's'     )),
                    )
                    for channel in channels
                ),
            ),

            ('PLLCKSELR',
                ('DIVM1' , 'pll1_predivider' , 1, 63),
                ('DIVM2' , 'pll2_predivider' , 1, 63),
                ('DIVM3' , 'pll3_predivider' , 1, 63),
                ('PLLSRC', 'pll_clock_source', (
                    ('hsi_ck' , '0b00'),
                    ('csi_ck' , '0b01'),
                    ('hse_ck' , '0b10'),
                    (None     , '0b11'),
                )),
            ),

            *(
                (f'PLL{unit}DIVR1',
                    ('DIVN', f'pll{unit}_multiplier', 12, 420),
                )
                for unit in (1, 2, 3)
            ),

            *(
                (f'PLL{unit}DIVR{2 if channel in ('s', 't') else 1}',
                    (f'DIV{channel.upper()}', f'pll{unit}{channel}_divider', 1, 128),
                )
                for unit, channels in (
                    (1, ('p', 'q',      's'     )),
                    (2, ('p', 'q', 'r', 's', 't')),
                    (3, ('p', 'q', 'r', 's'     )),
                )
                for channel in channels
            ),

            ('CCIPR1',
                ('CKPERSEL', 'per_ck_source', (
                    ('hsi_ck' , '0b00'),
                    ('csi_ck' , '0b01'),
                    ('hse_ck' , '0b10'),
                    (None     , '0b11'),
                )),
                ('SDMMC12SEL', 'sdmmc_clock_source', (
                    ('pll2_s_ck', '0b0'),
                    ('pll2_t_ck', '0b1'),
                )),
            ),

            ('CCIPR2',
                ('UART234578SEL', f'uxart_{(('usart', 2), ('usart', 3), ('uart', 4), ('uart', 5), ('uart', 7), ('uart', 8))}_clock_source', (
                    ('apb2_ck'  , '0b000'),
                    ('pll2_q_ck', '0b001'),
                    ('pll3_q_ck', '0b010'),
                    ('hsi_ck'   , '0b011'),
                    ('csi_ck'   , '0b100'),
                    ('lse_ck'   , '0b101'),
                )),
            ),

        ),

        ################################################################################

        ('USART',
            ('BRR',
                ('BRR', 'uxart_baud_divider', 1, 1 << 16),
            ),
        ),

    ),
)
