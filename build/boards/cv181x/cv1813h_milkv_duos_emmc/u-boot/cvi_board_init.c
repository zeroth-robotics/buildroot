static void set_rtc_register_for_power(void)
{
	printf("set_rtc_register_for_power\n");
	mmio_write_32(0x050260D0, 0x7);
}

int cvi_board_init(void)
{
	// Camera
	PINMUX_CONFIG(CAM_MCLK0, CAM_MCLK0);
	PINMUX_CONFIG(IIC3_SCL, IIC3_SCL);
	PINMUX_CONFIG(IIC3_SDA, IIC3_SDA);
	PINMUX_CONFIG(PAD_MIPIRX4P, XGPIOC_3);
	PINMUX_CONFIG(PAD_MIPIRX4N, XGPIOC_2);

	// I2C2 for Camera2
	PINMUX_CONFIG(IIC2_SDA, IIC2_SDA);
	PINMUX_CONFIG(IIC2_SCL, IIC2_SCL);

	// LED
	PINMUX_CONFIG(IIC0_SDA, XGPIOA_29);

	// I2C4 for TP
	PINMUX_CONFIG(VIVO_D1, IIC4_SCL);
	PINMUX_CONFIG(VIVO_D0, IIC4_SDA);

	// SPI3
	PINMUX_CONFIG(VIVO_D8, SPI3_SDO);
	PINMUX_CONFIG(VIVO_D7, SPI3_SDI);
	PINMUX_CONFIG(VIVO_D6, SPI3_SCK);
	PINMUX_CONFIG(VIVO_D5, SPI3_CS_X);

	// USB
	PINMUX_CONFIG(USB_VBUS_EN, XGPIOB_5);

	// WIFI/BT
	PINMUX_CONFIG(CLK32K, PWR_GPIO_10);
	PINMUX_CONFIG(UART2_RX, UART4_RX);
	PINMUX_CONFIG(UART2_TX, UART4_TX);
	PINMUX_CONFIG(UART2_CTS, UART4_CTS);
	PINMUX_CONFIG(UART2_RTS, UART4_RTS);

	// GPIOs
	PINMUX_CONFIG(JTAG_CPU_TCK, XGPIOA_18);
	PINMUX_CONFIG(JTAG_CPU_TMS, XGPIOA_19);
	PINMUX_CONFIG(JTAG_CPU_TRST, XGPIOA_20);
	PINMUX_CONFIG(IIC0_SCL, XGPIOA_28);

	// EPHY LEDs
	PINMUX_CONFIG(PWR_WAKEUP0, EPHY_LNK_LED);
	PINMUX_CONFIG(PWR_BUTTON1, EPHY_SPD_LED);

	// UART2
	PINMUX_CONFIG(VIVO_CLK, UART2_RX);
	PINMUX_CONFIG(VIVO_D6, UART2_TX);

	// UART dividers & uart0 fix
    // per 8.9.27 of sg2000_trm_en.pdf v1.01; setting UART* clock source to
    // disppll (1.188GHz) and dividing by 15 (to 79.2MHz)
    volatile uint32_t *reg_div_clk_cam0_200 = (volatile uint32_t *)0x030020a8;
    *reg_div_clk_cam0_200 = 0xF0109;

    // fix uboot logs, TODO: fix uboot dts and move clock init to appropriate place to avoid this bs
    volatile uint32_t *uart0_dll = (volatile uint32_t *)0x04140020;
    *uart0_dll = 43;

	set_rtc_register_for_power();

	return 0;
}

