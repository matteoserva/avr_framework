
#include <avr/io.h>
#include <util/twi.h>
#include <avr/interrupt.h>
#include "i2clib.h"
#include <util/delay.h>
#include <util/atomic.h>


/* I2C Clock */
#define I2C_F_SCL 100000UL // SCL frequency
#define I2C_PRESCALER 1
#define TWBR_VAL_BASE  ( ((F_CPU / I2C_F_SCL) / I2C_PRESCALER)/2)
#define TWBR_VAL_OFFSET  ( 16  / 2 )

/* definizioni varie */
#define I2C_RESULT_SUCCESS 0
#define I2C_RESULT_FAILURE -1

#define NUM_BUS_QUIET_CHECKS 10

#define printf(...)

enum I2C_Status_e {
    I2C_STATUS_IDLE, I2C_STATUS_TRANSFER, I2C_STATUS_WAIT_REPEAT,I2C_STATUS_CHECK_BUS, I2C_STATUS_MASTER_CB
};

struct I2C_Buffer_s {
	volatile uint8_t* volatile addr;
	uint8_t  		 len;
	uint8_t  		 pos;
};

enum I2C_Master_cmd_t {
    I2C_MASTER_CMD_NONE, I2C_MASTER_CMD_EXEC_AND_STOP, I2C_MASTER_CMD_EXEC_AND_WAIT,I2C_MASTER_CMD_STOP
};


/*
 *
 * 		Qui iniziano le variabili statiche
 *
 */
struct I2C_Buffer_s master_transmit_buffer;
struct I2C_Buffer_s master_receive_buffer;
struct I2C_Buffer_s slave_transmit_buffer;
struct I2C_Buffer_s slave_receive_buffer;
struct I2C_Buffer_s gcall_transmit_buffer;
struct I2C_Buffer_s gcall_receive_buffer;

struct I2C_Buffer_s *current_transaction_buffer;
volatile enum i2c_TransferType_t current_transfer_type;


static volatile enum I2C_Master_cmd_t i2c_master_cmd = I2C_MASTER_CMD_NONE;
static volatile enum I2C_Master_cmd_t i2c_gcall_cmd = I2C_MASTER_CMD_NONE;
enum I2C_Master_type_t {I2C_MASTER_TYPE_NONE, I2C_MASTER_TYPE_NORMAL, I2C_MASTER_TYPE_GCALL, I2C_MASTER_TYPE_PREPARING};

static volatile enum I2C_Master_type_t i2c_master_type = I2C_MASTER_TYPE_NONE;

/* a sinistra l'indirizzo, a destra il global call enable */
static uint8_t  i2c_slave_own_address = 0;
static uint8_t  i2c_master_destination_address = 0;

static volatile enum I2C_Status_e current_operation = I2C_STATUS_IDLE;

static i2c_transfer_callback_t i2c_master_callback = 0;
static i2c_transfer_callback_t i2c_slave_callback = 0;
static i2c_transfer_callback_t i2c_gcall_callback = 0;


static uint8_t _master_command_terminate(uint8_t control);
/* INIZIAMO con le funzioni */


static uint8_t _handle_slave_enable(uint8_t control)
{
	if(i2c_slave_own_address)
		control |= _BV(TWEA);
	else
		control &= ~_BV(TWEA);
	return control;
}



static void _i2c_update_address()
{
	uint8_t old_slave_address = TWAR;
	uint8_t addr1_valid = (old_slave_address)? 1:0;
	uint8_t addr2_valid = (i2c_slave_own_address)? 1:0;

	TWAR = i2c_slave_own_address;
	printf("TWAR %u %u \n",TWAR,i2c_slave_own_address);
	if(addr1_valid ^ addr2_valid) {
		/* lui setta l'EA, poi posso trovarmi in due casi, o l'ha settato una interrupt routine
		 * nella handle slave enable, oppure l'ho settato mentre ero IDLE o REPEAT.
		 * Esco solo se sono sicuro che sia stato aggiornato il dato, altrimenti rischio race condition
		 * nella chiamata alla callback */




		// esegui se sei in idle o in wait

		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {

			uint8_t oldTwcr = _handle_slave_enable(TWCR);
			oldTwcr &= ~(_BV(TWINT));
			TWCR = oldTwcr;
		}

		while(current_operation != I2C_STATUS_IDLE && current_operation != I2C_STATUS_WAIT_REPEAT)
			;
	}
}


void i2c_master_set_callback(i2c_transfer_callback_t cb)
{
	i2c_master_callback = cb;
}
void i2c_slave_set_callback(i2c_transfer_callback_t cb)
{
	i2c_slave_callback = cb;
}

void i2c_set_transfer_callback(i2c_transfer_callback_t cb)
{
	i2c_master_set_callback(cb);
	i2c_slave_set_callback(cb);

}

void i2c_gcall_set_callback(i2c_transfer_callback_t cb)
{
	i2c_gcall_callback = cb;
	if(cb)
		i2c_slave_own_address = (i2c_slave_own_address & 0xfe) | 0x01;
	else
		i2c_slave_own_address = (i2c_slave_own_address & 0xfe) | 0x00;
	_i2c_update_address();
}



void i2c_set_buffer(enum i2c_TransferType_t type, volatile uint8_t* address, uint8_t len)
{
	struct I2C_Buffer_s * buf = 0;
	switch (type) {
	case I2C_SLAVE_TRANSMIT:
		buf = &slave_transmit_buffer;
		break;
	case I2C_SLAVE_RECEIVE:
		buf = &slave_receive_buffer;
		break;
	case I2C_MASTER_TRANSMIT:
		buf = &master_transmit_buffer;
		break;
	case I2C_MASTER_RECEIVE:
		buf = &master_receive_buffer;
		break;
	case I2C_GCALL_TRANSMIT:
		buf = &gcall_transmit_buffer;
		break;
	case I2C_GCALL_RECEIVE:
		buf = &gcall_receive_buffer;
		break;
	}
	buf->addr = address;
	buf->len = len;
	buf->pos = 0;
}




static void _i2c_transfer_complete_cb(enum i2c_TransferType_t type, enum i2c_TransferPhase_t phase, unsigned int transferred)
{
	i2c_transfer_callback_t cb = 0;
	switch(type) {
	case I2C_GCALL_RECEIVE:
	case I2C_GCALL_TRANSMIT:
		cb = i2c_gcall_callback;
		break;
	case I2C_MASTER_RECEIVE:
	case I2C_MASTER_TRANSMIT:
		cb = i2c_master_callback;

		break;

	case I2C_SLAVE_RECEIVE:
	case I2C_SLAVE_TRANSMIT:
		cb = i2c_slave_callback;
		break;

	}
	if(type == I2C_MASTER_RECEIVE && phase == I2C_TRANSFER_COMPLETE)
		master_transmit_buffer.pos = transferred;

	if(cb)
		cb(type,phase,transferred);
}

static uint8_t _handle_ack_enable(uint8_t control, uint8_t buffer_pos, uint8_t buffer_len)
{
	if( buffer_pos +1 >= buffer_len)
		control &= ~_BV(TWEA);
	else
		control |= _BV(TWEA);
	return control;
}

int8_t _handle_check_bus_busy()
{
	volatile uint8_t tests = NUM_BUS_QUIET_CHECKS;
	if(current_operation == I2C_STATUS_CHECK_BUS) {
		while(tests > 0) {
			uint8_t bus_busy = (PINC & 3) ^ 3;
			if(bus_busy)
				return I2C_RESULT_FAILURE;
			tests--;
		}

		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			if(current_operation == I2C_STATUS_CHECK_BUS)
				current_operation = I2C_STATUS_IDLE;
		}

	}
	return I2C_RESULT_SUCCESS;

}


void i2c_init()
{
	printf("i2c init inizio\n");
	TWCR = 0;
	TWCR = TWCR; // per pulire l'interrupt
	current_operation = I2C_STATUS_IDLE;


	DDRC &= ~(_BV(0) | _BV(1));
	PORTC |= _BV(0) | _BV(1);

	_i2c_update_address();

	uint16_t prescaler_base = TWBR_VAL_BASE;
	uint16_t prescaler_offset = TWBR_VAL_OFFSET;
	uint8_t prescalerVal = 0;

	while ( ( prescaler_base - prescaler_offset) > 255) {
		prescalerVal++;
		prescaler_base /= 4;
	}
	TWBR = (uint8_t)prescaler_base;

	TWSR = (prescalerVal & (_BV(TWPS0) | _BV(TWPS1)));

	TWCR |=  (1<<TWEN)| 1<<TWIE ; // != perché mi ero già preparato il TWEA, (1<<TWINT) | sottinteso
	printf("i2c init fine\n");
}



void i2c_slave_set_address(uint8_t address_right)
{
	i2c_slave_own_address = (address_right <<1) | ( i2c_slave_own_address & 0x01) ;
	_i2c_update_address();
}






static uint8_t _starting_master_transaction()
{
	uint8_t next_address = i2c_master_destination_address;

	current_operation = I2C_STATUS_TRANSFER;
	if(i2c_master_type == I2C_MASTER_TYPE_GCALL) {
		current_transfer_type = I2C_GCALL_TRANSMIT;
		current_transaction_buffer = &gcall_transmit_buffer;
		next_address = 0;
	}

	else if( (next_address & 0x01) == TW_READ) {
		current_transfer_type = I2C_MASTER_RECEIVE;
		current_transaction_buffer = &master_receive_buffer;
	} else {
		current_transfer_type = I2C_MASTER_TRANSMIT;
		current_transaction_buffer = &master_transmit_buffer;
	}
	current_transaction_buffer->pos = 0;
	_i2c_transfer_complete_cb(current_transfer_type,I2C_TRANSFER_START,current_transaction_buffer->len);
	return next_address;
}

static void _starting_st_transaction()
{
	current_operation = I2C_STATUS_TRANSFER;
	current_transfer_type = I2C_SLAVE_TRANSMIT;
	current_transaction_buffer = &slave_transmit_buffer;
	_i2c_transfer_complete_cb(current_transfer_type,I2C_TRANSFER_START,current_transaction_buffer->len);
	current_transaction_buffer->pos = 0;
}

static void _starting_sr_transaction(uint8_t isGcall)
{
	current_operation = I2C_STATUS_TRANSFER;
	if(isGcall) {
		current_transfer_type = I2C_GCALL_RECEIVE;
		current_transaction_buffer = &gcall_receive_buffer;
	} else {
		current_transfer_type = I2C_SLAVE_RECEIVE;
		current_transaction_buffer = &slave_receive_buffer;
	}

	_i2c_transfer_complete_cb(current_transfer_type,I2C_TRANSFER_START,current_transaction_buffer->len);
	current_transaction_buffer->pos = 0;
}

/*static void _transaction_terminated_with_bus_error()
{
	master_transmit_buffer.pos = 0;
	master_receive_buffer.pos=0;
	current_operation = I2C_STATUS_CHECK_BUS;
	printf("bus error\n");
	
		if(i2c_master_cmd != I2C_MASTER_CMD_NONE) {
			if( TW_READ == (i2c_master_destination_address && 0x01))
				_i2c_transfer_complete_cb(I2C_MASTER_RECEIVE,I2C_TRANSFER_COMPLETE,0);
			else
				_i2c_transfer_complete_cb(I2C_MASTER_TRANSMIT,I2C_TRANSFER_COMPLETE,0);
		}
		if(i2c_gcall_cmd != I2C_MASTER_CMD_NONE) {
			_i2c_transfer_complete_cb(I2C_GCALL_TRANSMIT,I2C_TRANSFER_COMPLETE,0);
		}
		i2c_master_cmd = I2C_MASTER_CMD_NONE;
		i2c_gcall_cmd = I2C_MASTER_CMD_NONE;
		i2c_master_type = I2C_MASTER_TYPE_NONE;


}*/

static uint8_t _handle_master_enable(uint8_t control)
{
	if( i2c_master_type != I2C_MASTER_TYPE_NONE)
		control |=   _BV(TWSTA);
	else
		control &=  ~_BV(TWSTA);
	return control;
}

static void handleTwiInterrupt()
{
	uint8_t control = TWCR & ~_BV(TWSTO);

	printf("I2CS: %02X\n",TW_STATUS);
	switch( TW_STATUS) {
	case TW_REP_START:
	case TW_START: {
		uint8_t next_address = _starting_master_transaction();
		control &= ~_BV(TWSTA);
		TWDR = next_address;

		break;
	}
	/* MASTER TRANSMIT */
	case TW_MT_SLA_ACK:
		if(current_transaction_buffer->len < 1) {
			current_transaction_buffer->pos = 1;
			control = _master_command_terminate(control);
		} else {
			if(current_transaction_buffer->len -current_transaction_buffer->pos == 1) {
				_i2c_transfer_complete_cb(current_transfer_type,I2C_TRANSFER_PRESTOP,0);
			}

			TWDR= current_transaction_buffer->addr[current_transaction_buffer->pos++];
		}
		break;



	case TW_MT_DATA_ACK:
		if(current_transaction_buffer->len -current_transaction_buffer->pos == 1) {
			_i2c_transfer_complete_cb(current_transfer_type,I2C_TRANSFER_PRESTOP,current_transaction_buffer->pos);
		}

		if(current_transaction_buffer->pos >= current_transaction_buffer->len ) {
			control = _master_command_terminate(control);
		} else {
			TWDR= current_transaction_buffer->addr[current_transaction_buffer->pos++];
		}

		break;

		//case TW_MR_ARB_LOST:
	case TW_MT_ARB_LOST:

		current_operation = I2C_STATUS_IDLE;
		control |= _BV(TWSTA);
		control = _handle_slave_enable(control);
		break;

		/* MASTER RECEIVE */
	case TW_MR_SLA_ACK: {
		if(current_transaction_buffer->pos +1 >= current_transaction_buffer->len)
			_i2c_transfer_complete_cb(current_transfer_type,I2C_TRANSFER_PRESTOP,0);

		control = _handle_ack_enable(control, current_transaction_buffer->pos,current_transaction_buffer->len);
	}
	break;

	case TW_MR_DATA_NACK:
		current_transaction_buffer->addr[current_transaction_buffer->pos++] = TWDR;
		control = _master_command_terminate(control);
		break;

	case TW_MT_SLA_NACK: // slave non ha nemmeno risposto al suo indirizzo
	case TW_MT_DATA_NACK:
	case TW_MR_SLA_NACK:
		control = _master_command_terminate(control);
		break;

		/* SLAVE TRANSMIT */
	case TW_ST_ARB_LOST_SLA_ACK: // cade nel successivo
		control |= _BV(TWSTA);

	case TW_ST_SLA_ACK:  //cade nel successivo
		_starting_st_transaction();

	case TW_ST_DATA_ACK:
		TWDR = current_transaction_buffer->addr[current_transaction_buffer->pos++];
		if(current_transaction_buffer->pos >= current_transaction_buffer->len)
			_i2c_transfer_complete_cb(current_transfer_type,I2C_TRANSFER_PRESTOP,current_transaction_buffer->pos);

		control = _handle_ack_enable(control, current_transaction_buffer->pos-1,current_transaction_buffer->len);
		break;

		/* SLAVE RECEIVE */

	case TW_SR_ARB_LOST_GCALL_ACK:
	case TW_SR_ARB_LOST_SLA_ACK:
		control |= _BV(TWSTA);

	case TW_SR_GCALL_ACK:
	case TW_SR_SLA_ACK: {
		_starting_sr_transaction(TW_STATUS == TW_SR_ARB_LOST_GCALL_ACK || TW_STATUS == TW_SR_GCALL_ACK);
		if(current_transaction_buffer->len - current_transaction_buffer->pos == 1)
			_i2c_transfer_complete_cb(current_transfer_type,I2C_TRANSFER_PRESTOP,current_transaction_buffer->pos);
		control = _handle_ack_enable(control, current_transaction_buffer->pos,current_transaction_buffer->len);
	}
	break;

	case TW_SR_GCALL_DATA_NACK: // cade nel successivo
	case TW_SR_DATA_NACK: // cade nel successivo
		if(current_transaction_buffer->pos < current_transaction_buffer->len)
			current_transaction_buffer->addr[current_transaction_buffer->pos++] = TWDR;

	case TW_ST_DATA_NACK:
	case TW_ST_LAST_DATA:
		_i2c_transfer_complete_cb(current_transfer_type, I2C_TRANSFER_COMPLETE,current_transaction_buffer->pos);
		control = _handle_master_enable(control);
		current_operation = I2C_STATUS_IDLE;
		control = _handle_slave_enable(control);
		break;

	case TW_MR_DATA_ACK:
	case TW_SR_GCALL_DATA_ACK:
	case TW_SR_DATA_ACK:
		if(current_transaction_buffer->pos < current_transaction_buffer->len)
			current_transaction_buffer->addr[current_transaction_buffer->pos++] = TWDR;
		
		if(current_transaction_buffer->len - current_transaction_buffer->pos == 1)
			_i2c_transfer_complete_cb(current_transfer_type,I2C_TRANSFER_PRESTOP,current_transaction_buffer->pos);
		control = _handle_ack_enable(control, current_transaction_buffer->pos,current_transaction_buffer->len);
		break;

	case TW_SR_STOP:
		_i2c_transfer_complete_cb(current_transfer_type,I2C_TRANSFER_COMPLETE,current_transaction_buffer->pos);
		control = _handle_slave_enable(control);
		control &= ~_BV(TWSTA);
		break;
		/***************************/

	case TW_BUS_ERROR:
		control |=  _BV(TWSTO);
		control &= ~_BV(TWSTA);
		control = _handle_slave_enable(control);
		
		break;
	}
	printf("i2clibint %02X st %02X, TWDR: %02X\n",control, TWSR,TWDR);

	TWCR = control;
}

ISR(TWI_vect)
{
	handleTwiInterrupt();
}


uint8_t i2c_gcall_transmit_completed()
{
	uint8_t completed = (i2c_gcall_cmd == I2C_MASTER_CMD_NONE);
	return completed;
}

uint8_t i2c_master_transfer_completed()
{
	uint8_t completed = (i2c_master_cmd == I2C_MASTER_CMD_NONE);
	return completed;
}


uint8_t i2c_gcall_transmit_get_result()
{
	printf("get1\n");
	while(!i2c_gcall_transmit_completed())
		;
	printf("get2\n");
	uint8_t res = 0;
	res = gcall_transmit_buffer.pos;
	return res;
}

uint8_t i2c_master_get_result()
{
	printf("ge1\n");
	while(!i2c_master_transfer_completed())
		;
	printf("ge2\n");
	uint8_t res = 0;
	res = master_transmit_buffer.pos;
	return res;
}








void _i2c_set_new_master_command()
{
	if(i2c_master_type == I2C_MASTER_TYPE_NONE) {

		if(i2c_master_cmd != I2C_MASTER_CMD_NONE)
			i2c_master_type = I2C_MASTER_TYPE_NORMAL;
		else if(i2c_gcall_cmd != I2C_MASTER_CMD_NONE)
			i2c_master_type = I2C_MASTER_TYPE_GCALL;
	} else if(i2c_master_type == I2C_MASTER_TYPE_NORMAL && i2c_master_cmd == I2C_MASTER_CMD_NONE) {
		if(i2c_gcall_cmd != I2C_MASTER_CMD_NONE)
			i2c_master_type = I2C_MASTER_TYPE_GCALL;
		else
			i2c_master_type = I2C_MASTER_TYPE_NONE;
	} else if(i2c_master_type == I2C_MASTER_TYPE_GCALL && i2c_gcall_cmd == I2C_MASTER_CMD_NONE) {
		if(i2c_master_cmd != I2C_MASTER_CMD_NONE)
			i2c_master_type = I2C_MASTER_TYPE_NORMAL;
		else
			i2c_master_type = I2C_MASTER_TYPE_NONE;
	}
	//printf("master type %u, gcall %u, master %u\n",i2c_master_type, i2c_gcall_cmd, i2c_master_cmd);
}

void i2c_master_send_stop_condition()
{
	printf("stop:  currentOp: %d,  TW_STATUS: %02X, TWCR: %02X\n", current_operation,TW_STATUS,TWCR);


	if(current_operation == I2C_STATUS_MASTER_CB) {
		if(current_transfer_type == I2C_GCALL_TRANSMIT)
			i2c_gcall_cmd = I2C_MASTER_CMD_STOP;
		else
			i2c_master_cmd = I2C_MASTER_CMD_STOP;

	} else if( current_operation == I2C_STATUS_WAIT_REPEAT) {

		uint8_t control = TWCR;
		_i2c_set_new_master_command();

		if(i2c_master_type != I2C_MASTER_TYPE_NONE) {
			control |= _BV(TWSTA);
		}
		control = _handle_slave_enable(control);
		control |= _BV(TWSTO) | _BV(TWIE); //avevo disattivato gli interrupt
		current_operation = I2C_STATUS_IDLE;
		TWCR = control;
	}
}

static uint8_t _master_command_terminate(uint8_t control)
{


	current_operation = I2C_STATUS_MASTER_CB;

	volatile enum I2C_Master_cmd_t * pMasterCmd = &i2c_gcall_cmd;
	if(i2c_master_type == I2C_MASTER_TYPE_NORMAL)
		pMasterCmd = &i2c_master_cmd;

	enum I2C_Master_cmd_t old_cmd = *pMasterCmd;

	*pMasterCmd = I2C_MASTER_CMD_NONE;


	_i2c_transfer_complete_cb(current_transfer_type,I2C_TRANSFER_COMPLETE,current_transaction_buffer->pos);


	if(*pMasterCmd == I2C_MASTER_CMD_NONE && old_cmd == I2C_MASTER_CMD_EXEC_AND_WAIT) {
		//aspetta e basta
		control &= ~_BV(TWINT);
		control &= ~_BV(TWIE); // così non rientra
		current_operation = I2C_STATUS_WAIT_REPEAT;
	} else {

		if(old_cmd == I2C_MASTER_CMD_EXEC_AND_STOP) {
			control |= _BV(TWSTO);
		}
		if(*pMasterCmd == I2C_MASTER_CMD_STOP) {
			*pMasterCmd = I2C_MASTER_CMD_NONE;
			control |= _BV(TWSTO);
		}

		_i2c_set_new_master_command();

		if(i2c_master_type != I2C_MASTER_TYPE_NONE) {
			control |= _BV(TWSTA);
		}
		current_operation = I2C_STATUS_IDLE;


	}

	control = _handle_slave_enable(control);
	return control;

}







void _i2c_notify_new_master_command()
{





	if(current_operation == I2C_STATUS_MASTER_CB) {

	} else if(current_operation == I2C_STATUS_WAIT_REPEAT) {
		_i2c_set_new_master_command();
		TWCR |= _BV(TWSTA);

	} else {
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			
			if(i2c_master_type == I2C_MASTER_TYPE_NONE && (i2c_master_cmd != I2C_MASTER_CMD_NONE || i2c_gcall_cmd != I2C_MASTER_CMD_NONE)) {
					_i2c_set_new_master_command();
					uint8_t oldTwcr = TWCR;
					oldTwcr &= ~(_BV(TWINT));
					oldTwcr |= _BV(TWSTA);
					TWCR = oldTwcr;


				}
		}
	}



}


static int8_t _i2c_master_execute_internal(enum I2C_Master_cmd_t cmd, uint8_t address)
{
	uint8_t result = I2C_RESULT_SUCCESS;
	if(_handle_check_bus_busy() != I2C_RESULT_SUCCESS)
		return I2C_RESULT_FAILURE;
	
	if(address > 0) {
		if( i2c_master_cmd != I2C_MASTER_CMD_NONE)
			return I2C_RESULT_FAILURE;

		i2c_master_destination_address = address;
		i2c_master_cmd = cmd;

		_i2c_notify_new_master_command();
		result = I2C_RESULT_SUCCESS;
	}
	return result;
}

static int8_t _i2c_gcall_execute_internal(enum I2C_Master_cmd_t cmd)
{
	uint8_t result = I2C_RESULT_SUCCESS;
	if(_handle_check_bus_busy() != I2C_RESULT_SUCCESS)
		return I2C_RESULT_FAILURE;
	{
		if( i2c_gcall_cmd != I2C_MASTER_CMD_NONE)
			return I2C_RESULT_FAILURE;
		i2c_gcall_cmd = cmd;

		_i2c_notify_new_master_command();
		result = I2C_RESULT_SUCCESS;
	}
	return result;
}


uint8_t i2c_master_transmit(uint8_t _address)
{
	uint8_t address = (_address <<1) | (TW_WRITE);
	uint8_t res = _i2c_master_execute_internal(I2C_MASTER_CMD_EXEC_AND_STOP,address);
	return (res == I2C_RESULT_SUCCESS)?1:0;
}
uint8_t i2c_gcall_transmit()
{

	uint8_t res = _i2c_gcall_execute_internal(I2C_MASTER_CMD_EXEC_AND_STOP);

	return (res == I2C_RESULT_SUCCESS)?1:0;
}

uint8_t i2c_master_receive(uint8_t _address)
{
	uint8_t address = (_address <<1) | (TW_READ);
	uint8_t res = _i2c_master_execute_internal(I2C_MASTER_CMD_EXEC_AND_STOP,address);
	return (res == I2C_RESULT_SUCCESS)?1:0;
}

uint8_t i2c_master_transmit_multi(uint8_t _address)
{
	uint8_t address = (_address <<1) | (TW_WRITE);
	uint8_t res = _i2c_master_execute_internal(I2C_MASTER_CMD_EXEC_AND_WAIT,address);
	return (res == I2C_RESULT_SUCCESS)?1:0;
}


uint8_t i2c_master_receive_multi(uint8_t _address)
{
	uint8_t address = (_address <<1) | (TW_READ);
	uint8_t res = _i2c_master_execute_internal(I2C_MASTER_CMD_EXEC_AND_WAIT,address);
	return (res == I2C_RESULT_SUCCESS)?1:0;
}
