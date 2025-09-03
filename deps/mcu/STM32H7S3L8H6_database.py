(
    (
        ('PLL_UNITS', ( # @/pg 354/fig 40/`H7S3rm`.
            (1, ('p', 'q',      's',    )),
            (2, ('p', 'q', 'r', 's', 't')),
            (3, ('p', 'q', 'r', 's',    )),
        )),
        ('APB_UNITS', (1, 2, 4, 5)),                                     # @/pg 378/fig 51/`H7S3rm`.
        ('TIMERS'   , (1, 2, 3, 4, 5, 6, 7, 9, 12, 13, 14, 15, 16, 17)), # @/pg 61/tbl 13/`H7S3ds`.
        ('SPIS'     , ((1,), (4, 5))),                                   # @/pg 392/fig 54/`H7S3rm`. TODO SPI23, SPI6
        ('UXARTS'   , (                                                  # @/pg 394/fig 56/`H7S3rm`. TODO LPUART1?
            (('usart', 1),),
            (('usart', 2), ('usart', 3), ('uart', 4), ('uart', 5), ('uart', 7), ('uart', 8)),
        )),
        ('IWDG_COUNTER_FREQ', 32_000), # @/pg 63/sec 3.36.5/`H7S3ds`.
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
        ('timer_hclk_ratio' , ( # @/pg 407/tbl 67/`H7S3rm`.
            (('0b000', '0b0'), 1    ),
            (('0b001', '0b0'), 1    ),
            (('0b010', '0b0'), 1    ),
            (('0b011', '0b0'), 1    ),
            (('0b100', '0b0'), 1    ),
            (('0b101', '0b0'), 1 / 2),
            (('0b110', '0b0'), 1 / 4),
            (('0b111', '0b0'), 1 / 8),
            (('0b000', '0b1'), 1    ),
            (('0b001', '0b1'), 1    ),
            (('0b010', '0b1'), 1    ),
            (('0b011', '0b1'), 1    ),
            (('0b100', '0b1'), 1    ),
            (('0b101', '0b1'), 1    ),
            (('0b110', '0b1'), 1 / 2),
            (('0b111', '0b1'), 1 / 4),
        )),
        ('sdmmc_kernel_freq', 0          , 200_000_000), # @/pg 39/tbl 6/`H7S3ds`.
        ('cpu_freq'         , 0          , 600_000_000), # @/pg 158/tbl 26/`H7S3ds`. TODO We're assuming a high internal voltage. TODO 600MHz only when ECC is disabled.
        ('axi_ahb_freq'     , 0          , 300_000_000), # "
        ('apb_freq'         , 0          , 150_000_000), # "
        ('pll_channel_freq' , 1_500_000  , 600_000_000), # @/pg 200/tbl 64/`H7S3ds`. TODO We're assuming a high internal voltage.
        ('pll_vco_freq'     , 192_000_000, 836_000_000), # "                         TODO Assuming wide frequency range.
    ),
    (
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
        ('FLASH',
            ('ACR',
                ('WRHIGHFREQ', 'flash_programming_delay', ('0b00', '0b11')),
                ('LATENCY'   , 'flash_latency'          , 0b0000, 0b1111  ),
            ),
        ),
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
        ('RCC',
            ('AHB4ENR',
                *(
                    (f'GPIO{port}EN', f'gpio{port}_enable')
                    for port in 'ABCDEFGHI'
                ),
            ),
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
                ('MCO1SEL', 'mco1_clock_source', (
                    ('hsi_ck'   , '0b000'),
                    ('lse_ck'   , '0b001'),
                    ('hse_ck'   , '0b010'),
                    ('pll1_q_ck', '0b011'),
                    ('hsi48_ck' , '0b100'),
                )),
                ('MCO1PRE', 'mco1_divider', 0b0000, 0b1111),
                ('TIMPRE', 'global_timer_prescaler', (
                    0b0,
                    0b1,
                )),
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
            ('CDCFGR',
                ('CPRE', 'cpu_divider', (
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
            ('BMCFGR',
                ('BMPRE', 'axi_ahb_divider', (
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
                ('DIVM1', 'pll1_predivider', 1, 63),
                ('DIVM2', 'pll2_predivider', 1, 63),
                ('DIVM3', 'pll3_predivider', 1, 63),
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
                ('I2C1_I3C1SEL', 'i2c_1_clock_source', (
                    ('apb1_ck'  , '0b00'),
                    ('pll3_r_ck', '0b01'),
                    ('hsi_ck'   , '0b10'),
                    ('csi_ck'   , '0b11'),
                )),
                ('UART234578SEL', f'uxart_{(('usart', 2), ('usart', 3), ('uart', 4), ('uart', 5), ('uart', 7), ('uart', 8))}_clock_source', (
                    ('apb2_ck'  , '0b000'),
                    ('pll2_q_ck', '0b001'),
                    ('pll3_q_ck', '0b010'),
                    ('hsi_ck'   , '0b011'),
                    ('csi_ck'   , '0b100'),
                    ('lse_ck'   , '0b101'),
                )),
            ),
            ('CCIPR3',
                ('SPI1SEL', 'spi_1_clock_source', ( # TODO SPI1SEL: i2s_ckin TODO SPI45: hse_ck
                    ('pll1_q_ck', '0b000'),
                    ('pll2_p_ck', '0b001'),
                    ('pll3_p_ck', '0b010'),
                    ('per_ck'   , '0b100'),
                )),
                ('SPI45SEL', 'spi_4_5_clock_source', (
                    ('apb2_ck'  , '0b000'),
                    ('pll2_q_ck', '0b001'),
                    ('pll3_q_ck', '0b010'),
                    ('hsi_ck'   , '0b011'),
                    ('csi_ck'   , '0b100'),
                )),
                ('USART1SEL', f'uxart_{(('usart', 1),)}_clock_source', (
                    ('apb2_ck'  , '0b000'),
                    ('pll2_q_ck', '0b001'),
                    ('pll3_q_ck', '0b010'),
                    ('hsi_ck'   , '0b011'),
                    ('csi_ck'   , '0b100'),
                    ('lse_ck'   , '0b101'),
                )),
            ),
            ('APB1ENR1',
                ('USART3EN', 'uxart_3_enable'),
            ),
        ),
        *(
            (f'TIM{unit}',
                ('PSC',
                    ('PSC', f'timer_{unit}_divider', 1, 1 << 16),
                ),
            )
            for unit in (1, 2, 3, 4, 5, 6, 7, 9, 12, 13, 14, 15, 16, 17)
        ),
        *(
            (f'TIM{unit}',
                ('ARR',
                    ('ARR', f'timer_{unit}_modulation', 1, 1 << 20),
                ),
            )
            for unit in (1, 6, 7, 9, 12, 13, 14, 15, 16, 17)
        ),
        *(
            (f'TIM{unit}',
                ('ARR',
                    ('ARR', f'timer_{unit}_modulation', 1, 1 << 32),
                ),
            )
            for unit in (2, 3, 4, 5)
        ),
        ('IWDG',
            ('KR',
                ('KEY', 'iwdg_key', (
                    '0xAAAA',
                    '0x5555',
                    '0xCCCC',
                )),
            ),
            ('PR',
                ('PR', 'iwdg_divider', (
                    (4  , '0b000'),
                    (8  , '0b001'),
                    (16 , '0b010'),
                    (32 , '0b011'),
                    (64 , '0b100'),
                    (128, '0b101'),
                    (256, '0b110'), # Bit-pattern 0b111 is also the same 256 prescaler.
                )),
            ),
            ('RLR',
                ('RL', 'iwdg_reload', 0, (1 << 12) - 1),
            ),
            ('SR',
                ('WVU', 'iwdg_window_value_updating' ),
                ('RVU', 'iwdg_reload_value_updating' ),
                ('PVU', 'iwdg_divider_value_updating'),
            ),
        ),
        ('I2C',
            ('TIMINGR',
                ('PRESC', 'i2c_PRESC', 0, (1 << 4) - 1),
                ('SCLH' , 'i2c_SCLH' , 0, (1 << 8) - 1),
                ('SCLL' , 'i2c_SCLL' , 0, (1 << 8) - 1),
            ),
        ),
        ('USART',
            ('BRR',
                ('BRR', 'uxart_baud_divider', 1, 1 << 16),
            ),
        ),
        ('SPI',
            ('CFG1',
                ('MBR', 'spi_baud_divider', (
                    (2  , '0b000'),
                    (4  , '0b001'),
                    (8  , '0b010'),
                    (16 , '0b011'),
                    (32 , '0b100'),
                    (64 , '0b101'),
                    (128, '0b110'),
                    (256, '0b111'),
                )),
            ),
        ),
        ('SDMMC',
            ('CLKCR',
                ('CLKDIV', 'sdmmc_CLKDIV', 0, 1023),
            ),
            ('DTIMER',
                ('DATATIME', 'sdmmc_DATATIME', 0, (1 << 32) - 1),
            ),
        ),
        ('DBGMCU',
            ('APB4FZR',
                ('DBG_IWDG', 'iwdg_stopped_during_debug'),
            ),
        ),
    ),
)
