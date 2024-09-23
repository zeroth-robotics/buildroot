#include <stdint.h>
#include <stdio.h>
// #include "FreeRTOS.h"
// #include "task.h"
// #include "semphr.h"
#include "drv_pwm.h"


struct cvi_pwm_dev
{
    const char *name;
    uint32_t reg_base;
};

static const uint64_t count_unit = 100000000;  // 100M count per second
static const uint64_t NSEC_COUNT = 1000000000;  // ns

static void cvi_pwm_set_config(uint32_t reg_base, struct pwm_configuration *cfg)
{
    uint64_t duty_clk, period_clk;

    cvi_pwm_set_polarity_high_ch(reg_base, (cfg->channel & PWM_MAX_CH));

    duty_clk = (cfg->pulse * count_unit) / NSEC_COUNT;
    cvi_pwm_set_high_period_ch(reg_base, (cfg->channel & PWM_MAX_CH), duty_clk);

    period_clk = (cfg->period * count_unit) / NSEC_COUNT;
    cvi_pwm_set_period_ch(reg_base, (cfg->channel & PWM_MAX_CH), period_clk);

    cvi_pwm_output_en_ch(reg_base, cfg->channel & PWM_MAX_CH);
}

// 定义 PWM 设备
static struct cvi_pwm_dev cvi_pwm[] =
{
    {
        .name = "pwm0",
        .reg_base = CVI_PWM0_BASE,
    },
    {
        .name = "pwm1",
        .reg_base = CVI_PWM1_BASE,
    },
    {
        .name = "pwm2",
        .reg_base = CVI_PWM2_BASE,
    },
    {
        .name = "pwm3",
        .reg_base = CVI_PWM3_BASE,
    },
};

void pwm_init(uint8_t pwm_id, struct pwm_configuration *cfg)
{
    cvi_pwm_set_config(cvi_pwm[pwm_id].reg_base, cfg);
}
