{
    'value' : (
        ('APB_UNITS', (1, 2, 3)), # @/pg 456/fig 52/`H533rm`.
        ('PLL_UNITS', (           # @/pg 456/fig 52/`H533rm`.
            (1, ('p', 'q', 'r')),
            (2, ('p', 'q', 'r')),
            (3, ('p', 'q', 'r')),
        )),
        ('UXARTS', ( # @/pg 52/tbl 10/`H533ds`. @/pg 456/fig 52/`H533rm`.
            (('usart', 1),),
            (('usart', 2),),
            (('usart', 3),),
            (('uart' , 4),),
            (('uart' , 5),),
            (('usart', 6),),
        )),
        ('GPIO_PORT_ENABLE_REGISTER', 'AHB2ENR'), # @/pg 518/sec 11.8.27/`H533rm`.
        ('GPIO_MODE', (                           # @/pg 586/sec 13.4.1/`H533rm`.
            ('input'    , '0b00'),
            ('output'   , '0b01'),
            ('alternate', '0b10'),
            ('analog'   , '0b11'),
        )),
    ),
    'minmax' : (
        ('pll_channel_freq',   1_000_000, 250_000_000), # @/pg 124/tbl 47/`H533ds`. TODO We're assuming a high internal voltage and wide range.
        ('pll_vco_freq'    , 128_000_000, 560_000_000), # " TODO Assuming wide frequency range.
        ('cpu_freq'        ,           0, 250_000_000), # @/pg 101/tbl 21/`HS33ds`. TODO We're assuming a high internal voltage. TODO 600MHz only when ECC is disabled.
        ('axi_ahb_freq'    ,           0, 250_000_000), # "
        ('apb_freq'        ,           0, 250_000_000), # "
    ),
    'registers' : {
        'SysTick' : {
            'LOAD' : {
                'RELOAD' : {
                    'name'   : 'systick_reload',
                    'minmax' : (1, (1 << 24) - 1),
                },
            },
            'VAL' : {
                'CURRENT' : {
                    'name'   : 'systick_counter',
                    'minmax' : (0, (1 << 32) - 1),
                },
            },
            'CTRL' : {
                'CLKSOURCE' : 'systick_use_cpu_ck',
                'TICKINT'   : 'systick_interrupt_enable',
                'ENABLE'    : 'systick_enable',
            },
        },
        'FLASH' : {
            'ACR' : {
                'WRHIGHFREQ' : { 'name' : 'flash_programming_delay', 'value'  : (0b00, 0b01, 0b10), },
                'LATENCY'    : { 'name' : 'flash_latency'          , 'minmax' : (0b0000, 0b1111)    },
            },
        },
        'PWR' : {
            'VOSCR' : {
                'VOS' : 'internal_voltage_scaling',
            },
            'VOSSR' : {
                'ACTVOS'    : 'current_active_vos',
                'ACTVOSRDY' : 'current_active_vos_ready',
            },
            'SCCR' : {
                'LDOEN'  : 'ldo_enable',
                'BYPASS' : 'power_management_bypass',
            },
        },
        'RCC' : {
            'CR' : {
                'PLL3RDY'  : 'pll3_ready'  ,
                'PLL3ON'   : 'pll3_enable' ,
                'PLL2RDY'  : 'pll2_ready'  ,
                'PLL2ON'   : 'pll2_enable' ,
                'PLL1RDY'  : 'pll1_ready'  ,
                'PLL1ON'   : 'pll1_enable' ,
                'HSI48RDY' : 'hsi48_ready' ,
                'HSI48ON'  : 'hsi48_enable',
                'CSIRDY'   : 'csi_ready'   ,
                'CSION'    : 'csi_enable'  ,
                'HSIRDY'   : 'hsi_ready'   ,
                'HSION'    : 'hsi_enable'  ,
            },
            'CFGR1' : {
                **{
                    field : {
                        'name' : name,
                        'value' : (
                            ('hsi_ck'   , '0b000'),
                            ('csi_ck'   , '0b001'),
                            ('hse_ck'   , '0b010'),
                            ('pll1_p_ck', '0b011'),
                        ),
                    }
                    for field, name in (
                        ('SWS', 'effective_scgu_clock_source'),
                        ('SW' , 'scgu_clock_source'          ),
                    )
                },
            },
            'CFGR2' : {
                **{
                    f'PPRE{unit}' : {
                        'name'  : f'apb{unit}_divider',
                        'value' : (
                            (1 , '0b000'),
                            (2 , '0b100'),
                            (4 , '0b101'),
                            (8 , '0b110'),
                            (16, '0b111'),
                        ),
                    }
                    for unit in (1, 2, 3)
                },
                'HPRE'  : {
                    'name'  : 'cpu_divider',
                    'value' : (
                        (1  , '0b0000'), # Low three bits are don't-care.
                        (2  , '0b1000'),
                        (4  , '0b1001'),
                        (8  , '0b1010'),
                        (16 , '0b1011'),
                        (64 , '0b1100'),
                        (128, '0b1101'),
                        (256, '0b1110'),
                        (512, '0b1111'),
                    ),
                },
            },
            **{
                f'PLL{unit}CFGR' : {
                    f'PLL{unit}REN' : f'pll{unit}r_enable',
                    f'PLL{unit}QEN' : f'pll{unit}q_enable',
                    f'PLL{unit}PEN' : f'pll{unit}p_enable',
                    f'PLL{unit}M'   : {
                        'name'   : f'pll{unit}_predivider',
                        'minmax' : (1, 63),
                    },
                    f'PLL{unit}SRC' : {
                        'name'  : f'pll{unit}_clock_source',
                        'value' : (
                            (None    , '0b00'),
                            ('hsi_ck', '0b01'),
                            ('csi_ck', '0b10'),
                            ('hse_ck', '0b11'),
                        ),
                    },
                    f'PLL{unit}RGE' : {
                        'name'  : f'pll{unit}_input_range',
                        'value' : (
                            ( 1_000_000, None),
                            ( 2_000_000, None), # TODO Can be '0b00', but only for medium VCO. @/pg 124/tbl 47/`H533rm`.
                            ( 4_000_000, 0b01),
                            ( 8_000_000, 0b10),
                            (16_000_000, 0b11),
                        ),
                    },
                }
                for unit in (1, 2, 3)
            },
            **{
                f'PLL{unit}DIVR' : {
                    f'PLL{unit}R' : { 'name' : f'pll{unit}r_divider'  , 'minmax' : (1, 128) },
                    f'PLL{unit}Q' : { 'name' : f'pll{unit}q_divider'  , 'minmax' : (1, 128) },
                    f'PLL{unit}P' : { 'name' : f'pll{unit}p_divider'  , 'minmax' : (1, 128) },
                    f'PLL{unit}N' : { 'name' : f'pll{unit}_multiplier', 'minmax' : (4, 512) },
                }
                for unit in (1, 2, 3)
            },
            'APB1LENR' : {
                'USART2EN' : 'uxart_2_enable',
            },
            'CCIPR1' : {
                **{
                    field : {
                        'name'  : f'uxart_{peripherals}_clock_source',
                        'value' : clock_source
                    }
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
                                ('hsi_ker_ck', '0b011'),
                                ('csi_ker_ck', '0b100'),
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
                                ('hsi_ker_ck', '0b011'),
                                ('csi_ker_ck', '0b100'),
                                # TODO: ('lse_ck'    , '0b101'),
                                (None        , '0b110'),
                            ),
                        ),
                    )
                    for field, peripherals in field_peripherals
                },
            },
            'CCIPR5' : {
                'CKPERSEL' : {
                    'name'  : 'per_ck_source',
                    'value' : (
                        ('hsi_ck', '0b00'),
                        ('csi_ck', '0b01'),
                        (None    , '0b11'),
                        # TODO hse_ck.
                    ),
                },
            },
        },
        'USART' : {
            'BRR': {
                'BRR' : {
                    'name'   : 'uxart_baud_divider',
                    'minmax' : (1, 1 << 16),
                },
            },
        },
    },
}
