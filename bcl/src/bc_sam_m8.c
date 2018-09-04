#include <bc_sam_m8.h>

void _bc_sam_m8_task(void *param);

void bc_sam_m8_init(bc_sam_m8_t *self, bc_i2c_channel_t channel, uint8_t i2c_address)
{
    
}

void bc_sam_m8_set_event_handler(bc_sam_m8_t *self, bc_sam_m8_event_handler_t event_handler, void *event_param)
{
    self->_event_handler = event_handler;
    self->_event_param = event_param;
}

void bc_sam_m8_set_timeout(bc_sam_m8_t *self, bc_tick_t timeout)
{
    // TODO Replan timeout event generation (take care of first setup of this value)

    self->_timeout = timeout;
}

void bc_sam_m8_update(bc_sam_m8_t *self)
{

}

void bc_sam_m8_abort(bc_sam_m8_t *self)
{

}

void _bc_sam_m8_task(void *param)
{

}
