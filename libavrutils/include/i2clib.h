#ifndef I2C_MASTER_H
#define I2C_MASTER_H


#ifdef __cplusplus
extern "C" {
#endif


enum i2c_TransferType_t { I2C_SLAVE_TRANSMIT, I2C_SLAVE_RECEIVE, I2C_MASTER_TRANSMIT, I2C_MASTER_RECEIVE,I2C_GCALL_TRANSMIT, I2C_GCALL_RECEIVE};
enum i2c_TransferPhase_t { I2C_TRANSFER_START, I2C_TRANSFER_PRESTOP, I2C_TRANSFER_COMPLETE};

typedef void (*i2c_transfer_callback_t) (enum i2c_TransferType_t type, enum i2c_TransferPhase_t phase,  unsigned int transferred);


void i2c_init();
void i2c_set_transfer_callback(i2c_transfer_callback_t); //deprecated
void i2c_set_buffer(enum i2c_TransferType_t type, volatile uint8_t* address, uint8_t len);

/*
 * MASTER
 */
uint8_t i2c_master_transmit(uint8_t address);
uint8_t i2c_master_receive(uint8_t address);
uint8_t i2c_master_transmit_multi(uint8_t address);
uint8_t i2c_master_receive_multi(uint8_t address);
uint8_t i2c_master_transfer_completed(void);
uint8_t i2c_master_get_result(void);

/* chiamabile prima della master start o nella callback, tranne che nella TRANSFER_COMPLETE */
void i2c_master_send_stop_condition();
void i2c_master_set_callback(i2c_transfer_callback_t cb);

/* 
 * SLAVE
 */
void i2c_slave_set_address(uint8_t address_right);
void i2c_slave_set_callback(i2c_transfer_callback_t cb);



/* 
 * GCALL
 */
uint8_t i2c_gcall_transmit();
void i2c_gcall_set_callback(i2c_transfer_callback_t);
uint8_t i2c_gcall_transmit_completed();
uint8_t i2c_gcall_transmit_get_result();


#ifdef __cplusplus
}
#endif

#endif // I2C_MASTER_H
