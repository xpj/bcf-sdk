#ifndef _BC_SAM_M8
#define _BC_SAM_M8

#include <bc_i2c.h>
#include <bc_scheduler.h>

//! @addtogroup bc_sam_m8 bc_sam_m8
//! @brief Driver for SAM M8 module
//! @{

//! @brief Callback events

typedef enum
{
    //! @brief Error event
    BC_SAM_M8_EVENT_ERROR = 0,
    
    //! @brief Abort event
    BC_SAM_M8_EVENT_ABORT = 1,
    
    //! @brief Update event
    BC_SAM_M8_EVENT_UPDATE = 2,
    
    //! @brief Timeout event
    BC_SAM_M8_EVENT_TIMEOUT = 3

} bc_sam_m8_event_t;

//! @brief SAM M8 instance

typedef struct bc_sam_m8_t bc_sam_m8_t;

//! @cond

typedef void (bc_sam_m8_event_handler_t)(bc_sam_m8_t *, bc_sam_m8_event_t, void *);

struct bc_sam_m8_t
{
    // TODO Consider driver

    bc_i2c_channel_t _i2c;

    uint8_t _i2c_address;

    bc_sam_m8_event_handler_t *_event_handler;

    void *_event_param;

    bc_tick_t _timeout;

    bc_scheduler_task_id_t _task_id;

};

//! @endcond

//! @brief Initialize SAM M8Q driver
//! @param[in] self Instance
//! @param[in] i2c_channel I2C channel
//! @param[in] i2c_address I2C device address

void bc_sam_m8_init(bc_sam_m8_t *self, bc_i2c_channel_t channel, uint8_t i2c_address);

//! @brief Set callback function
//! @param[in] self Instance
//! @param[in] event_handler Function address
//! @param[in] event_param Optional event parameter (can be NULL)

void bc_sam_m8_set_event_handler(bc_sam_m8_t *self, bc_sam_m8_event_handler_t event_handler, void *event_param);

//! @brief Set timeout interval
//! @param[in] self Instance
//! @param[in] timeout Timeout interval

void bc_sam_m8_set_timeout(bc_sam_m8_t *self, bc_tick_t timeout);

//! @brief Update data
//! @param[in] self Instance

void bc_sam_m8_update(bc_sam_m8_t *self);

//! @brief Abort data update
//! @param[in] self Instance

void bc_sam_m8_abort(bc_sam_m8_t *self);

#endif // _BC_SAM_M8
