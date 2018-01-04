#include <bc_common.h>
#include <bc_dma.h>
#include <bc_irq.h>
#include <bc_scheduler.h>
#include <bc_fifo.h>
#include <stm32l083xx.h>

#define _BC_DMA_CHECK_IRQ_OF_CHANNEL_(__CHANNEL)\
    if ((DMA1->ISR & DMA_ISR_GIF##__CHANNEL) != 0)\
    {\
        if ((DMA1->ISR & DMA_ISR_TEIF##__CHANNEL) != 0)\
        {\
            _bc_dma_irq_handler(BC_DMA_CHANNEL_##__CHANNEL, BC_DMA_EVENT_ERROR);\
        }\
        else if ((DMA1->ISR & DMA_ISR_HTIF##__CHANNEL) != 0)\
        {\
            _bc_dma_irq_handler(BC_DMA_CHANNEL_##__CHANNEL, BC_DMA_EVENT_HALF_DONE);\
            DMA1->IFCR |= DMA_IFCR_CHTIF##__CHANNEL;\
        }\
        else if ((DMA1->ISR & DMA_ISR_TCIF##__CHANNEL) != 0)\
        {\
            _bc_dma_irq_handler(BC_DMA_CHANNEL_##__CHANNEL, BC_DMA_EVENT_DONE);\
            DMA1->IFCR |= DMA_IFCR_CTCIF##__CHANNEL;\
        }\
    }

typedef struct
{
    uint8_t channel : 4;
    uint8_t event : 4;

} bc_dma_pending_event_t;

static bc_dma_pending_event_t _bc_dma_pending_event_buffer[2 * 7 * sizeof(bc_dma_pending_event_t)];

static struct
{
    bool is_initialized;
    struct
    {
        DMA_Channel_TypeDef *instance;
        uint32_t cselr_position;
        bc_dma_event_handler_t *event_handler;
        void *event_param;

    } channel[7];
    bc_fifo_t fifo_pending;
    bool task_planned;
    bc_scheduler_task_id_t task_id;

} _bc_dma;

static void _bc_dma_task(void *param);

static void _bc_dma_irq_handler(bc_dma_channel_t channel, bc_dma_event_t event);

static inline void _bc_dma_channel_enable(bc_dma_channel_t channel);

static inline void _bc_dma_channel_disable(bc_dma_channel_t channel);

void bc_dma_init()
{
    if (_bc_dma.is_initialized)
    {
        return;
    }

    _bc_dma.channel[BC_DMA_CHANNEL_1].instance = DMA1_Channel1;
    _bc_dma.channel[BC_DMA_CHANNEL_1].cselr_position = DMA_CSELR_C1S_Pos;

    _bc_dma.channel[BC_DMA_CHANNEL_2].instance = DMA1_Channel2;
    _bc_dma.channel[BC_DMA_CHANNEL_2].cselr_position = DMA_CSELR_C2S_Pos;

    _bc_dma.channel[BC_DMA_CHANNEL_3].instance = DMA1_Channel3;
    _bc_dma.channel[BC_DMA_CHANNEL_3].cselr_position = DMA_CSELR_C3S_Pos;

    _bc_dma.channel[BC_DMA_CHANNEL_4].instance = DMA1_Channel4;
    _bc_dma.channel[BC_DMA_CHANNEL_4].cselr_position = DMA_CSELR_C4S_Pos;

    _bc_dma.channel[BC_DMA_CHANNEL_5].instance = DMA1_Channel5;
    _bc_dma.channel[BC_DMA_CHANNEL_5].cselr_position = DMA_CSELR_C5S_Pos;

    _bc_dma.channel[BC_DMA_CHANNEL_6].instance = DMA1_Channel6;
    _bc_dma.channel[BC_DMA_CHANNEL_6].cselr_position = DMA_CSELR_C6S_Pos;

    _bc_dma.channel[BC_DMA_CHANNEL_7].instance = DMA1_Channel7;
    _bc_dma.channel[BC_DMA_CHANNEL_7].cselr_position = DMA_CSELR_C7S_Pos;

    bc_fifo_init(&_bc_dma.fifo_pending, _bc_dma_pending_event_buffer, sizeof(_bc_dma_pending_event_buffer));

    _bc_dma.task_id = bc_scheduler_register(_bc_dma_task, NULL, BC_TICK_INFINITY);

    // Enable DMA1
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    // Enable DMA 1 channel 1 interrupt
    NVIC_SetPriority(DMA1_Channel1_IRQn, 0);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    // Enable DMA 1 channel 2 and 3 interrupts
    NVIC_SetPriority(DMA1_Channel2_3_IRQn, 1);
    NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);

    // Enable DMA 1 channel 4, 5, 6 and 7 interrupts
    NVIC_SetPriority(DMA1_Channel4_5_6_7_IRQn, 2);
    NVIC_EnableIRQ(DMA1_Channel4_5_6_7_IRQn);
}

void bc_dma_channel_config(bc_dma_channel_t channel, bc_dma_channel_config_t *config)
{
    DMA_Channel_TypeDef *dma_channel = _bc_dma.channel[channel].instance;
    uint32_t dma_cselr_pos = _bc_dma.channel[channel].cselr_position;

    // Set DMA direction
    if (config->direction == BC_DMA_DIRECTION_TO_PERIPHERAL)
    {
        dma_channel->CCR |= DMA_CCR_DIR;
    }
    else
    {
        dma_channel->CCR &= DMA_CCR_DIR;
    }

    // Set data size
    dma_channel->CCR &= ~DMA_CCR_PSIZE_Msk;
    dma_channel->CCR &= ~DMA_CCR_MSIZE_Msk;
    if (config->size == BC_DMA_SIZE_2)
    {
        dma_channel->CCR |= DMA_CCR_PSIZE_0;
        dma_channel->CCR |= DMA_CCR_MSIZE_0;
    }
    else if (config->size == BC_DMA_SIZE_4)
    {
        dma_channel->CCR |= DMA_CCR_PSIZE_1;
        dma_channel->CCR |= DMA_CCR_MSIZE_1;
    }

    // Set DMA mode
    if (config->mode == BC_DMA_MODE_STANDARD)
    {
        dma_channel->CCR &= ~DMA_CCR_CIRC;
    }
    else if (config->mode == BC_DMA_MODE_CIRCULAR)
    {
        dma_channel->CCR |= DMA_CCR_CIRC;
    }

    // Set memory incrementation
    dma_channel->CCR |= DMA_CCR_MINC;

    // Set DMA channel priority
    dma_channel->CCR &= ~DMA_CCR_PL_Msk;
    dma_channel->CCR |= config->priority << DMA_CCR_PL_Pos;

    // Configure request selection for DMA1 Channel
    DMA1_CSELR->CSELR &= ~(0xf << dma_cselr_pos);
    DMA1_CSELR->CSELR |= (config->request << dma_cselr_pos);

    // Configure DMA channel data length
    dma_channel->CNDTR = config->length;

    // Configure DMA channel source address
    dma_channel->CPAR = (uint32_t)config->address_peripheral;

    // Configure DMA channel destination address
    dma_channel->CMAR = (uint32_t)config->address_memory;

    // Enable the transfer complete, half-complete and error interrupts
    dma_channel->CCR |= DMA_CCR_TCIE | DMA_CCR_HTIE | DMA_CCR_TEIE;

    // Enable the peripheral
    _bc_dma_channel_enable(channel);
}

void bc_dma_set_event_handler(bc_dma_channel_t channel, bc_dma_event_handler_t *event_handler, void *event_param)
{
    _bc_dma.channel[channel].event_handler = event_handler;
    _bc_dma.channel[channel].event_param = event_param;
}

void _bc_dma_task(void *param)
{
    (void) param;

    bc_dma_pending_event_t pending_event;

    while (bc_fifo_read(&_bc_dma.fifo_pending, &pending_event, sizeof(bc_dma_pending_event_t)) == sizeof(bc_dma_pending_event_t))
    {
        _bc_dma.channel[pending_event.channel].event_handler(pending_event.channel, pending_event.event, _bc_dma.channel[pending_event.channel].event_param);
    }
    
    bc_irq_disable();

    if (bc_fifo_is_empty(&_bc_dma.fifo_pending))
    {
        _bc_dma.task_planned = false;
    }
    else
    {
        bc_scheduler_plan_current_now();
    }

    bc_irq_enable();
}

void _bc_dma_irq_handler(bc_dma_channel_t channel, bc_dma_event_t event)
{
    if (event == BC_DMA_EVENT_DONE)
    {
        _bc_dma_channel_disable(channel);
    }

    if (_bc_dma.channel[channel].event_handler != NULL)
    {
        bc_dma_pending_event_t pending_event = { channel, event };

        bc_fifo_irq_write(&_bc_dma.fifo_pending, &pending_event, sizeof(bc_dma_pending_event_t));

        if (!_bc_dma.task_planned)
        {
            bc_scheduler_plan_now(_bc_dma.task_id);

            _bc_dma.task_planned = true;
        }
    }
}

static inline void _bc_dma_channel_enable(bc_dma_channel_t channel)
{
    _bc_dma.channel[channel].instance->CCR |= DMA_CCR_EN;
}

static inline void _bc_dma_channel_disable(bc_dma_channel_t channel)
{
    _bc_dma.channel[channel].instance->CCR &= ~DMA_CCR_EN;
}

void DMA1_Channel1_IRQHandler(void)
{
    _BC_DMA_CHECK_IRQ_OF_CHANNEL_(1);
}

void DMA1_Channel2_3_IRQHandler(void)
{
    _BC_DMA_CHECK_IRQ_OF_CHANNEL_(2);

    _BC_DMA_CHECK_IRQ_OF_CHANNEL_(3);
}

void DMA1_Channel4_5_6_7_IRQHandler(void)
{
    _BC_DMA_CHECK_IRQ_OF_CHANNEL_(4);

    _BC_DMA_CHECK_IRQ_OF_CHANNEL_(5);

    _BC_DMA_CHECK_IRQ_OF_CHANNEL_(6);

    _BC_DMA_CHECK_IRQ_OF_CHANNEL_(7);
}
