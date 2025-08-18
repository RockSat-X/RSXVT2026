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
                'SWS' : {
                    'name'  : 'effective_scgu_clock_source',
                    'value' : (
                        ('hsi_ck'   , '0b000'),
                        ('csi_ck'   , '0b001'),
                        ('hse_ck'   , '0b010'),
                        ('pll1_p_ck', '0b011'),
                    ),
                },
                'SW' : {
                    'name'  : 'scgu_clock_source',
                    'value' : (
                        ('hsi_ck'   , '0b000'),
                        ('csi_ck'   , '0b001'),
                        ('hse_ck'   , '0b010'),
                        ('pll1_p_ck', '0b011'),
                    ),
                },
            },
            'CFGR2' : {
                'PPRE3' : { 'name' : 'apb3_divider', 'value' : ((1, '0b000'), (2, '0b100'), (4, '0b101'), (8, '0b110'), (16, '0b111')) },
                'PPRE2' : { 'name' : 'apb2_divider', 'value' : ((1, '0b000'), (2, '0b100'), (4, '0b101'), (8, '0b110'), (16, '0b111')) },
                'PPRE1' : { 'name' : 'apb1_divider', 'value' : ((1, '0b000'), (2, '0b100'), (4, '0b101'), (8, '0b110'), (16, '0b111')) },
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
            'PLL1CFGR' : {
                'PLL1REN' : 'pll1r_enable',
                'PLL1QEN' : 'pll1q_enable',
                'PLL1PEN' : 'pll1p_enable',
                'PLL1M'   : {
                    'name'   : 'pll1_predivider',
                    'minmax' : (1, 63),
                },
                'PLL1SRC' : {
                    'name'  : 'pll1_clock_source',
                    'value' : (
                        (None    , '0b00'),
                        ('hsi_ck', '0b01'),
                        ('csi_ck', '0b10'),
                        ('hse_ck', '0b11'),
                    ),
                },
                'PLL1RGE' : {
                    'name'  : 'pll1_input_range',
                    'value' : (
                        ( 1_000_000, None),
                        ( 2_000_000, None), # TODO Can be '0b00', but only for medium VCO. @/pg 124/tbl 47/`H533rm`.
                        ( 4_000_000, 0b01),
                        ( 8_000_000, 0b10),
                        (16_000_000, 0b11),
                    ),
                },
            },
            'PLL2CFGR' : {
                'PLL2REN' : 'pll2r_enable',
                'PLL2QEN' : 'pll2q_enable',
                'PLL2PEN' : 'pll2p_enable',
                'PLL2M'   : {
                    'name'   : 'pll2_predivider',
                    'minmax' : (1, 63),
                },
                'PLL2SRC' : {
                    'name'  : 'pll2_clock_source',
                    'value' : (
                        (None    , '0b00'),
                        ('hsi_ck', '0b01'),
                        ('csi_ck', '0b10'),
                        ('hse_ck', '0b11'),
                    ),
                },
                'PLL2RGE' : {
                    'name'  : 'pll2_input_range',
                    'value' : (
                        ( 1_000_000, None),
                        ( 2_000_000, None), # TODO Can be '0b00', but only for medium VCO. @/pg 124/tbl 47/`H533rm`.
                        ( 4_000_000, 0b01),
                        ( 8_000_000, 0b10),
                        (16_000_000, 0b11),
                    ),
                },
            },
            'PLL3CFGR' : {
                'PLL3REN' : 'pll3r_enable',
                'PLL3QEN' : 'pll3q_enable',
                'PLL3PEN' : 'pll3p_enable',
                'PLL3M'   : {
                    'name'   : 'pll3_predivider',
                    'minmax' : (1, 63),
                },
                'PLL3SRC' : {
                    'name'  : 'pll3_clock_source',
                    'value' : (
                        (None    , '0b00'),
                        ('hsi_ck', '0b01'),
                        ('csi_ck', '0b10'),
                        ('hse_ck', '0b11'),
                    ),
                },
                'PLL3RGE' : {
                    'name'  : 'pll3_input_range',
                    'value' : (
                        ( 1_000_000, None),
                        ( 2_000_000, None), # TODO Can be '0b00', but only for medium VCO. @/pg 124/tbl 47/`H533rm`.
                        ( 4_000_000, 0b01),
                        ( 8_000_000, 0b10),
                        (16_000_000, 0b11),
                    ),
                },
            },
            'PLL1DIVR' : {
                'PLL1R' : { 'name' : 'pll1r_divider'  , 'minmax' : (1, 128) },
                'PLL1Q' : { 'name' : 'pll1q_divider'  , 'minmax' : (1, 128) },
                'PLL1P' : { 'name' : 'pll1p_divider'  , 'minmax' : (1, 128) },
                'PLL1N' : { 'name' : 'pll1_multiplier', 'minmax' : (4, 512) },
            },
            'PLL2DIVR' : {
                'PLL2R' : { 'name' : 'pll2r_divider'  , 'minmax' : (1, 128) },
                'PLL2Q' : { 'name' : 'pll2q_divider'  , 'minmax' : (1, 128) },
                'PLL2P' : { 'name' : 'pll2p_divider'  , 'minmax' : (1, 128) },
                'PLL2N' : { 'name' : 'pll2_multiplier', 'minmax' : (4, 512) },
            },
            'PLL3DIVR' : {
                'PLL3R' : { 'name' : 'pll3r_divider'  , 'minmax' : (1, 128) },
                'PLL3Q' : { 'name' : 'pll3q_divider'  , 'minmax' : (1, 128) },
                'PLL3P' : { 'name' : 'pll3p_divider'  , 'minmax' : (1, 128) },
                'PLL3N' : { 'name' : 'pll3_multiplier', 'minmax' : (4, 512) },
            },
            'APB1LENR' : {
                'USART2EN' : 'uxart_2_enable',
            },
            'CCIPR1' : {
                'USART2SEL' : {
                    'name'  : f'uxart_{(('usart', 2),)}_clock_source',
                    'value' : (
                        ('apb1_ck'   , '0b000'),
                        ('pll2_q_ck' , '0b001'),
                        ('pll3_q_ck' , '0b010'),
                        ('hsi_ker_ck', '0b011'),
                        ('csi_ker_ck', '0b100'),
                        (None        , '0b110'),
                        # TODO: ('lse_ck', '0b101'),
                    ),
                },
                'USART3SEL' : {
                    'name'  : f'uxart_{(('usart', 3),)}_clock_source',
                    'value' : (
                        ('apb1_ck'   , '0b000'),
                        ('pll2_q_ck' , '0b001'),
                        ('pll3_q_ck' , '0b010'),
                        ('hsi_ker_ck', '0b011'),
                        ('csi_ker_ck', '0b100'),
                        (None        , '0b110'),
                        # TODO: ('lse_ck', '0b101'),
                    ),
                },
                'USART6SEL' : {
                    'name'  : f'uxart_{(('usart', 6),)}_clock_source',
                    'value' : (
                        ('apb1_ck'   , '0b000'),
                        ('pll2_q_ck' , '0b001'),
                        ('pll3_q_ck' , '0b010'),
                        ('hsi_ker_ck', '0b011'),
                        ('csi_ker_ck', '0b100'),
                        (None        , '0b110'),
                        # TODO: ('lse_ck', '0b101'),
                    ),
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
