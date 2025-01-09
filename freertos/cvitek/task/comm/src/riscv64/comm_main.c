/* Standard includes. */
#include <stdio.h>
#include <string.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "mmio.h"
#include "delay.h"

/* cvitek includes. */
#include "printf.h"
#include "rtos_cmdqu.h"
#include "cvi_mailbox.h"
#include "intr_conf.h"
#include "top_reg.h"
#include "memmap.h"
#include "comm.h"
#include "cvi_spinlock.h"
#include "uart.h"
#include "feetech.h"
#include "cv181x_reg_fmux_gpio.h"
// #include "hal_uart_dw.h"

/* Milk-V Duo */
#include "milkv_duo_io.h"

#define CVIMMAP_SHMEM_ADDR 0x9fd00000  /* offset 509.0MiB */
#define CVIMMAP_SHMEM_SIZE 0x100000  /* 1.0MiB */

#define UART2 2

#ifndef pdMS_TO_TICKS
    #define pdMS_TO_TICKS( xTimeInMs )    ( ( TickType_t ) ( ( ( uint64_t ) ( xTimeInMs ) * ( uint64_t ) configTICK_RATE_HZ ) / ( uint64_t ) 1000U ) )
#endif

#ifndef pdTICKS_TO_MS
    #define pdTICKS_TO_MS( xTimeInTicks )    ( ( TickType_t ) ( ( ( uint64_t ) ( xTimeInTicks ) * ( uint64_t ) 1000U ) / ( uint64_t ) configTICK_RATE_HZ ) )
#endif

// #define __DEBUG__
#ifdef __DEBUG__
#define debug_printf printf
#else
#define debug_printf(...)
#endif

typedef struct _TASK_CTX_S {
	char        name[32];
	u16         stack_size;
	UBaseType_t priority;
	void (*runTask)(void *pvParameters);
	u8            queLength;
	QueueHandle_t queHandle;
} TASK_CTX_S;

/****************************************************************************
 * Function prototypes
 ****************************************************************************/
void prvQueueISR(void);
void prvCmdQuRunTask(void *pvParameters);
void prvServosRunTask(void *pvParameters);


/****************************************************************************
 * Global parameters
 ****************************************************************************/
TASK_CTX_S gTaskCtx[2] = {
	{
		.name = "CMDQU",
		.stack_size = configMINIMAL_STACK_SIZE,
		.priority = tskIDLE_PRIORITY + 5,
		.runTask = prvCmdQuRunTask,
		.queLength = 30,
		.queHandle = NULL,
	},
	{
		.name = "SERVOS",
		.stack_size = configMINIMAL_STACK_SIZE,
		.priority = tskIDLE_PRIORITY + 3,
		.runTask = prvServosRunTask,
		.queLength = 1,
		.queHandle = NULL,
	},
};

/* mailbox parameters */
volatile struct mailbox_set_register *mbox_reg;
volatile struct mailbox_done_register *mbox_done_reg;
volatile unsigned long *mailbox_context; // mailbox buffer context is 64 Bytess

volatile BroadcastCommand g_broadcast_command = {
	.data_length = 0,
	.data = {0},
};

volatile ServoInfoBuffer g_read_servo_buffer = {
	.retry_count = 0,
	.read_count = 0,
	.last_read_ms = 0,
	.loop_count = 0,
	.servos = {0},
};

volatile ActiveServoList g_active_servo_list = {
	.len = 0,
	.servo_id = {0},
};

SemaphoreHandle_t g_servo_data_mutex;

static void is_servo_active(uint8_t id) {
	for (int i = 0; i < g_active_servo_list.len; i++) {
		if (g_active_servo_list.servo_id[i] == id) {
			return 1;
		}
	}
	return 0;
}

/****************************************************************************
 * Function definitions
 ****************************************************************************/
DEFINE_CVI_SPINLOCK(mailbox_lock, SPIN_MBOX);

void main_create_tasks(void)
{
	u8 i = 0;

#define TASK_INIT(_idx) \
do { \
	gTaskCtx[_idx].queHandle = xQueueCreate(gTaskCtx[_idx].queLength, sizeof(cmdqu_t)); \
	if (gTaskCtx[_idx].queHandle != NULL && gTaskCtx[_idx].runTask != NULL) { \
		xTaskCreate(gTaskCtx[_idx].runTask, gTaskCtx[_idx].name, gTaskCtx[_idx].stack_size, \
			    NULL, gTaskCtx[_idx].priority, NULL); \
	} \
} while(0)

	for (; i < ARRAY_SIZE(gTaskCtx); i++) {
		TASK_INIT(i);
	}
}

void main_cvirtos(void)
{
	printf("create cvi task\n");

	/* Start the tasks and timer running. */
	request_irq(MBOX_INT_C906_2ND, prvQueueISR, 0, "mailbox", (void *)0);
	main_create_tasks();
    /* Start the tasks and timer running. */
    vTaskStartScheduler();

    /* If all is well, the scheduler will now be running, and the following
    line will never be reached.  If the following line does execute, then
    there was either insufficient FreeRTOS heap memory available for the idle
    and/or timer tasks to be created, or vTaskStartScheduler() was called from
    User mode.  See the memory management section on the FreeRTOS web site for
    more details on the FreeRTOS heap http://www.freertos.org/a00111.html.  The
    mode from which main() is called is set in the C start up code and must be
    a privileged mode (not user mode). */
    printf("cvi task end\n");
	
	for (;;)
        ;
}

void prvServosRunTask(void *pvParameters)
{
	// Initialize UART2
	int baudrate = 1000000;
	int uart_clock = 187500000;

	// https://github.com/milkv-duo/duo-pinmux/blob/main/duos/func.h
	// FMUX_GPIO_REG_IOCTRL_VIVO_CLK - B22, UART2_RX
	// { "B227", "UART2_RX"}, => pin B22, func = 7
	// FMUX_GPIO_REG_IOCTRL_VIVO_D6 - B15, UART2_TX
	// { "B156", "UART2_TX"}, => pin B15, func = 6
/*
	[root@milkv-duo]~# devmem 0x03001160 32
	0x00000007
	[root@milkv-duo]~# devmem 0x03001144 32
	0x00000006
	[root@milkv-duo]~# devmem 0x03001C2C 32
	0x00000048
	[root@milkv-duo]~# devmem 0x03001C10 32
	0x00000048
*/

	volatile uint32_t *uart2_rx_pinmux = (volatile uint32_t *)PINMUX_BASE + FMUX_GPIO_REG_IOCTRL_VIVO_CLK;
	volatile uint32_t *uart2_tx_pinmux = (volatile uint32_t *)PINMUX_BASE + FMUX_GPIO_REG_IOCTRL_VIVO_D6;

	*uart2_rx_pinmux = 7;
	*uart2_tx_pinmux = 6;

	hal_srv_uart_init(UART2, baudrate, uart_clock);

	g_servo_data_mutex = xSemaphoreCreateMutex();
 	
	TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(8); // 8ms interval, 125Hz
    uint32_t tick_count = 0;

	xLastWakeTime = xTaskGetTickCount();

    const TickType_t xFullReadInterval = pdMS_TO_TICKS(1000); // 1000ms = 1 second
	TickType_t xLastFullReadTime = xTaskGetTickCount();

	for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        TickType_t currentTime = xTaskGetTickCount();

        if (xSemaphoreTake(g_servo_data_mutex, 10) == pdTRUE) {
            g_read_servo_buffer.loop_count++;

			// run broadcast command every loop cycle
			servo_sync_write(g_broadcast_command.data, g_broadcast_command.data_length);

            // Read position and status for all servos
            for (int i = 0; i < g_active_servo_list.len; i++) {
                uint8_t id = g_active_servo_list.servo_id[i];
				g_read_servo_buffer.servos[i].id = id;

                if (servo_read_position_and_status(id, &g_read_servo_buffer.servos[i], 5) != 0) {
                    g_read_servo_buffer.fault_count++;
                } else {
                    g_read_servo_buffer.read_count++;
					g_read_servo_buffer.servos[i].last_read_ms = pdTICKS_TO_MS(xTaskGetTickCount());
                }
            }

            // Full read every second
            if ((currentTime - xLastFullReadTime) >= xFullReadInterval) {
                for (int i = 0; i < g_active_servo_list.len; i++) {
                    uint8_t id = g_active_servo_list.servo_id[i];
                    servo_read_info(id, &g_read_servo_buffer.servos[i], 1);
                }
                xLastFullReadTime = currentTime;
            }

			g_read_servo_buffer.last_read_ms = pdTICKS_TO_MS(xTaskGetTickCount());

            xSemaphoreGive(g_servo_data_mutex);
        }
    }
}

void prvCmdQuRunTask(void *pvParameters)
{
	/* Remove compiler warning about unused parameter. */
	(void)pvParameters;

	cmdqu_t rtos_cmdq;
	cmdqu_t *cmdq;
	cmdqu_t *rtos_cmdqu_t;
	static int stop_ip = 0;
	int ret = 0;
	int flags;
	int valid;
	int send_to_cpu = SEND_TO_CPU1;
	volatile char *shared_data = CVIMMAP_SHMEM_ADDR;

	unsigned int reg_base = MAILBOX_REG_BASE;

	/* to compatible code with linux side */
	cmdq = &rtos_cmdq;
	mbox_reg = (struct mailbox_set_register *) reg_base;
	mbox_done_reg = (struct mailbox_done_register *) (reg_base + 2);
	mailbox_context = (unsigned long *) (MAILBOX_REG_BUFF);

	cvi_spinlock_init();
	printf("prvCmdQuRunTask run\n");
	unsigned int n = 0;

	for (;;) {
		xQueueReceive(gTaskCtx[0].queHandle, &rtos_cmdq, portMAX_DELAY);

		switch (rtos_cmdq.cmd_id) {
			case SYS_CMD_MSG_TEST:
				{
					// rtos_cmdq.param_ptr &= 0xFFFFFFFF;
					TickType_t currentTime = xTaskGetTickCount();
					inv_dcache_range(shared_data, 100);
					// printf("Received message: %s\n", (char*)shared_data);
					char message[100];
        			snprintf(message, sizeof(message), "Hello from small core %d tkrate %d! Got %s", currentTime, configTICK_RATE_HZ, ((char*) shared_data));
        			strcpy(shared_data, message);
					flush_dcache_range(shared_data, 100);
					rtos_cmdq.cmd_id = SYS_CMD_MSG_TEST;
					rtos_cmdq.resv.valid.rtos_valid = 1;
					rtos_cmdq.resv.valid.linux_valid = 0;
					currentTime = xTaskGetTickCount();
					// printf("t: %d", currentTime);
					goto send_label;
				}
				break;
			case SYS_CMD_SERVO_READ:
				{
					volatile ServoCommand *shared_servo_command = (volatile ServoCommand *)CVIMMAP_SHMEM_ADDR;
					ServoCommand local_command;

					inv_dcache_range(shared_servo_command, sizeof(ServoCommand));
					memcpy(&local_command, (void *)shared_servo_command, sizeof(ServoCommand));

					if (xSemaphoreTake(g_servo_data_mutex, portMAX_DELAY) == pdTRUE) {
						uint8_t response[256] = {0};  // Use the full 256-byte buffer
						int result = servo_read_command(&local_command, response, 3);

						// Write the response to shared memory
						memcpy((void *)shared_data, response, 256);
						flush_dcache_range(shared_data, 256);

						xSemaphoreGive(g_servo_data_mutex);
					}

					rtos_cmdq.cmd_id = SYS_CMD_SERVO_READ;
					rtos_cmdq.resv.valid.rtos_valid = 1;
					rtos_cmdq.resv.valid.linux_valid = 0;
					goto send_label;
				}
				break;
			case SYS_CMD_SERVO_WRITE:
                {
                    volatile ServoCommand *shared_servo_command = (volatile ServoCommand *)CVIMMAP_SHMEM_ADDR;
                    ServoCommand local_command;

					int result = -1;

                    inv_dcache_range(shared_servo_command, sizeof(ServoCommand));
                    memcpy(&local_command, (void *)shared_servo_command, sizeof(ServoCommand));
					// printf("local_command.id: %d, address: %d, length: %d; data[0]: %d, data[1]: %d, data[2]: %d, data[3]: %d, data[4]: %d, data[5]: %d\n", local_command.id, local_command.address, local_command.length, local_command.data[0], local_command.data[1], local_command.data[2], local_command.data[3], local_command.data[4], local_command.data[5]);

					if (xSemaphoreTake(g_servo_data_mutex, portMAX_DELAY) == pdTRUE) {
						result = servo_write_command(&local_command, 5);
						vTaskDelay(1);
						xSemaphoreGive(g_servo_data_mutex);
					}

                    // Send back the result
                    *(volatile int *)CVIMMAP_SHMEM_ADDR = result;
                    flush_dcache_range(CVIMMAP_SHMEM_ADDR, sizeof(int));

                    rtos_cmdq.cmd_id = SYS_CMD_SERVO_WRITE;
                    rtos_cmdq.resv.valid.rtos_valid = 1;
                    rtos_cmdq.resv.valid.linux_valid = 0;
                    goto send_label;
                }
                break;
			case SYS_CMD_SERVO_INFO:
                {
                    volatile ServoInfoBuffer *shared_servo_data = (volatile ServoInfoBuffer *)CVIMMAP_SHMEM_ADDR;
                    
                    if (xSemaphoreTake(g_servo_data_mutex, portMAX_DELAY) == pdTRUE) {
                        memcpy((void *)shared_servo_data, (void *)&g_read_servo_buffer, sizeof(g_read_servo_buffer));
						g_read_servo_buffer.loop_count = 0; // Reset the counter after reading
						g_read_servo_buffer.read_count = 0;
						g_read_servo_buffer.fault_count = 0;
						g_read_servo_buffer.last_read_ms = pdTICKS_TO_MS(xTaskGetTickCount());
                        xSemaphoreGive(g_servo_data_mutex);
                    }

                    flush_dcache_range(shared_servo_data, sizeof(g_read_servo_buffer));

                    rtos_cmdq.cmd_id = SYS_CMD_SERVO_INFO;
                    rtos_cmdq.resv.valid.rtos_valid = 1;
                    rtos_cmdq.resv.valid.linux_valid = 0;
                    goto send_label;
                }
                break;
			case SYS_CMD_SERVO_BROADCAST:
				{
					volatile BroadcastCommand *shared_command = (volatile BroadcastCommand *)CVIMMAP_SHMEM_ADDR;

					inv_dcache_range(shared_command, sizeof(BroadcastCommand));
					
					if (xSemaphoreTake(g_servo_data_mutex, portMAX_DELAY) == pdTRUE) {
						// Store the command for periodic execution
						memcpy((void *)&g_broadcast_command, (void *)shared_command, sizeof(BroadcastCommand));
						xSemaphoreGive(g_servo_data_mutex);
					}

					// Return success
					*(volatile int *)CVIMMAP_SHMEM_ADDR = 0;
					flush_dcache_range(CVIMMAP_SHMEM_ADDR, 4);

					rtos_cmdq.cmd_id = SYS_CMD_SERVO_BROADCAST;
					rtos_cmdq.resv.valid.rtos_valid = 1;
					rtos_cmdq.resv.valid.linux_valid = 0;
					goto send_label;
				}
				break;
			case SYS_CMD_SET_ACTIVE_SERVO_LIST:
				{
					volatile ActiveServoList *shared_active_servo_list = (volatile ActiveServoList *)CVIMMAP_SHMEM_ADDR;

					inv_dcache_range(shared_active_servo_list, sizeof(ActiveServoList));
					memcpy((void *)&g_active_servo_list, (void *)shared_active_servo_list, sizeof(g_active_servo_list));
					memset((void *)g_read_servo_buffer.servos, 0, sizeof(g_read_servo_buffer.servos));

					rtos_cmdq.cmd_id = SYS_CMD_SET_ACTIVE_SERVO_LIST;
					rtos_cmdq.resv.valid.rtos_valid = 1;
					rtos_cmdq.resv.valid.linux_valid = 0;
					goto send_label;
				}
				break;
			default:
send_label:
				/* used to send command to linux*/
				rtos_cmdqu_t = (cmdqu_t *) mailbox_context;

				debug_printf("RTOS_CMDQU_SEND %d\n", send_to_cpu);
				debug_printf("ip_id=%d cmd_id=%d param_ptr=%x\n", cmdq->ip_id, cmdq->cmd_id, (unsigned int)cmdq->param_ptr);
				debug_printf("mailbox_context = %x\n", mailbox_context);
				debug_printf("linux_cmdqu_t = %x\n", rtos_cmdqu_t);
				debug_printf("cmdq->ip_id = %d\n", cmdq->ip_id);
				debug_printf("cmdq->cmd_id = %d\n", cmdq->cmd_id);
				debug_printf("cmdq->block = %d\n", cmdq->block);
				debug_printf("cmdq->para_ptr = %x\n", cmdq->param_ptr);

				drv_spin_lock_irqsave(&mailbox_lock, flags);
				if (flags == MAILBOX_LOCK_FAILED) {
					printf("[%s][%d] drv_spin_lock_irqsave failed! ip_id = %d , cmd_id = %d\n" , cmdq->ip_id , cmdq->cmd_id);
					break;
				}

				for (valid = 0; valid < MAILBOX_MAX_NUM; valid++) {
					if (rtos_cmdqu_t->resv.valid.linux_valid == 0 && rtos_cmdqu_t->resv.valid.rtos_valid == 0) {
						// mailbox buffer context is 4 bytes write access
						int *ptr = (int *)rtos_cmdqu_t;

						cmdq->resv.valid.rtos_valid = 1;
						*ptr = ((cmdq->ip_id << 0) | (cmdq->cmd_id << 8) | (cmdq->block << 15) |
								(cmdq->resv.valid.linux_valid << 16) |
								(cmdq->resv.valid.rtos_valid << 24));
						rtos_cmdqu_t->param_ptr = cmdq->param_ptr;
						debug_printf("rtos_cmdqu_t->linux_valid = %d\n", rtos_cmdqu_t->resv.valid.linux_valid);
						debug_printf("rtos_cmdqu_t->rtos_valid = %d\n", rtos_cmdqu_t->resv.valid.rtos_valid);
						debug_printf("rtos_cmdqu_t->ip_id =%x %d\n", &rtos_cmdqu_t->ip_id, rtos_cmdqu_t->ip_id);
						debug_printf("rtos_cmdqu_t->cmd_id = %d\n", rtos_cmdqu_t->cmd_id);
						debug_printf("rtos_cmdqu_t->block = %d\n", rtos_cmdqu_t->block);
						debug_printf("rtos_cmdqu_t->param_ptr addr=%x %x\n", &rtos_cmdqu_t->param_ptr, rtos_cmdqu_t->param_ptr);
						debug_printf("*ptr = %x\n", *ptr);
						// clear mailbox
						mbox_reg->cpu_mbox_set[send_to_cpu].cpu_mbox_int_clr.mbox_int_clr = (1 << valid);
						// trigger mailbox valid to rtos
						mbox_reg->cpu_mbox_en[send_to_cpu].mbox_info |= (1 << valid);
						mbox_reg->mbox_set.mbox_set = (1 << valid);
						break;
					}
					rtos_cmdqu_t++;
				}
				drv_spin_unlock_irqrestore(&mailbox_lock, flags);
				if (valid >= MAILBOX_MAX_NUM) {
				    printf("No valid mailbox is available\n");
				    return -1;
				}
				break;
		}
	}
}

void prvQueueISR(void)
{
	// printf("prvQueueISR\n");
	unsigned char set_val;
	unsigned char valid_val;
	int i;
	cmdqu_t *cmdq;
	BaseType_t YieldRequired = pdFALSE;

	set_val = mbox_reg->cpu_mbox_set[RECEIVE_CPU].cpu_mbox_int_int.mbox_int;

	if (set_val) {
		for(i = 0; i < MAILBOX_MAX_NUM; i++) {
			valid_val = set_val  & (1 << i);

			if (valid_val) {
				cmdqu_t rtos_cmdq;
				cmdq = (cmdqu_t *)(mailbox_context) + i;

				debug_printf("mailbox_context =%x\n", mailbox_context);
				debug_printf("sizeof mailbox_context =%x\n", sizeof(cmdqu_t));
				/* mailbox buffer context is send from linux, clear mailbox interrupt */
				mbox_reg->cpu_mbox_set[RECEIVE_CPU].cpu_mbox_int_clr.mbox_int_clr = valid_val;
				// need to disable enable bit
				mbox_reg->cpu_mbox_en[RECEIVE_CPU].mbox_info &= ~valid_val;

				// copy cmdq context (8 bytes) to buffer ASAP
				*((unsigned long *) &rtos_cmdq) = *((unsigned long *)cmdq);
				/* need to clear mailbox interrupt before clear mailbox buffer */
				*((unsigned long*) cmdq) = 0;

				/* mailbox buffer context is send from linux*/
				if (rtos_cmdq.resv.valid.linux_valid == 1) {
					debug_printf("cmdq=%x\n", cmdq);
					debug_printf("cmdq->ip_id =%d\n", rtos_cmdq.ip_id);
					debug_printf("cmdq->cmd_id =%d\n", rtos_cmdq.cmd_id);
					debug_printf("cmdq->param_ptr =%x\n", rtos_cmdq.param_ptr);
					debug_printf("cmdq->block =%x\n", rtos_cmdq.block);
					debug_printf("cmdq->linux_valid =%d\n", rtos_cmdq.resv.valid.linux_valid);
					debug_printf("cmdq->rtos_valid =%x\n", rtos_cmdq.resv.valid.rtos_valid);

					xQueueSendFromISR(gTaskCtx[0].queHandle, &rtos_cmdq, &YieldRequired);

					portYIELD_FROM_ISR(YieldRequired);
				} else
					printf("rtos cmdq is not valid %d, ip=%d , cmd=%d\n",
						rtos_cmdq.resv.valid.rtos_valid, rtos_cmdq.ip_id, rtos_cmdq.cmd_id);
			}
		}
	}
}
