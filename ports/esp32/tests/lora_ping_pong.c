/**
 * This a ping pong test program
 * 
 * 
 * 
*/
#include "modlora.h"
#if defined(LORA_MODE_PRIVATE)

//Blood ping-pong Test


/*!
 * Radio events function pointer
 */
static RadioEvents_t RadioEvents;

static void OnTxDone( void );
static void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr );
static void OnTxTimeout( void );
static void OnRxTimeout( void );
static void OnRxError( void );

//All stand params
#define RF_FREQUENCY                                433175000 // Hz
#define TX_OUTPUT_POWER                             10        // dBm
#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
                                                              //  1: 250 kHz,
                                                              //  2: 500 kHz,
                                                              //  3: Reserved]
#define LORA_SPREADING_FACTOR                       12         // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5,
                                                              //  2: 4/6,
                                                              //  3: 4/7,
                                                              //  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         5         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false


#define RX_TIMEOUT_VALUE                            5000
#define BUFFER_SIZE                                 40 //Define the payload size here

typedef enum
{
    LOWPOWER,
    RX,
    RX_TIMEOUT,
    RX_ERROR,
    TX,
    TX_TIMEOUT,
}States_t;

typedef struct _ping_s
{
  uint8_t type;
  uint8_t code;
  uint16_t crc;
  uint16_t id;
  uint16_t seq;
}Ping_head;

union Ping_buffer{
  uint8_t val[BUFFER_SIZE];
  struct {
    Ping_head head;
    uint8_t payload[BUFFER_SIZE - sizeof(Ping_head)];
  }data;
};

typedef enum
{
  PTYPE_PING_REPLY,
  PTYPE_1,
  PTYPE_2,
  PTYPE_3,
  PTYPE_4,
  PTYPE_5,
  PTYPE_6,
  PTYPE_7,
  PTYPE_PING_REQ
}Ping_type;

typedef enum
{
  PCODE_0
}Ping_code;

const uint8_t PingMsg[] = "PING";
const uint8_t PongMsg[] = "PONG";

uint16_t BufferSize = BUFFER_SIZE;
uint8_t Buffer[BUFFER_SIZE];
union Ping_buffer Send_buffer;
union Ping_buffer Recv_buffer;

States_t State = LOWPOWER;

int8_t RssiValue = 0;
int8_t SnrValue = 0;
uint32_t recvSucc = 0;
uint32_t sendSucc = 0;
uint32_t recvError = 0;
uint32_t sendError = 0;

/*********************** User function ****************/
static void ping_init_pck(Ping_type type, Ping_code code, uint16_t pid, uint16_t seq)
{
  Send_buffer.data.head.type = type;
  Send_buffer.data.head.code = code;
  Send_buffer.data.head.crc = 0;
  Send_buffer.data.head.id = pid;
  Send_buffer.data.head.seq = seq;
  Send_buffer.data.head.id = seq;
  memcpy(Send_buffer.data.payload, "abcdefghijklmnopqrstuvuvwxyzOKOK", 32);
}

static char* Device_state(States_t st)
{
  switch(st){
    case LOWPOWER:
      return "LOWPOWER";
    case RX:
      return "RX";
    case RX_TIMEOUT:
      return "RX_TIMEOUT";
    case RX_ERROR:
      return "RX_ERROR";
    case TX:
      return "TX";
    case TX_TIMEOUT:
      return "TX_TIMEOUT";
  }
  return "error";
}


static void  lora_private_init()
{


    RadioEvents.TxDone = OnTxDone;
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.RxError = OnRxError;
    Radio.Init( &RadioEvents );
    Radio.SetChannel( RF_FREQUENCY );
    Radio.SetPublicNetwork(true);

    Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 
                        0, LORA_BANDWIDTH,
                        LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                        LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                        true, 0, 0, LORA_IQ_INVERSION_ON, 3000 );

    Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                        LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                        LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                        0, true, 0, 0, LORA_IQ_INVERSION_ON, true );
}

static void OnTxDone( void )
{
    sendSucc+=1;
    Radio.Sleep( );
    // printf("\n[Radio FSM] Event: OnTxDone, State : %s -> %s \n", Device_state(State), Device_state(TX));
    State = TX;
}

static void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{
    // int8_t i=0;
    recvSucc+=1;
    Radio.Sleep( );
    memcpy( Recv_buffer.val, payload, size );
    RssiValue = rssi;
    SnrValue = snr;
    // printf("\n[Radio FSM] Event: OnRxDone, State : %s -> %s , size:%d, seq:%d rssi:%d snr:%d\n",
    //     Device_state(State), Device_state(RX), size,Recv_buffer.data.head.seq, RssiValue, SnrValue);
    // for(i=0; i< size; i++){
    //     printf("%02x ", payload[i]&0xff);
    // }
    // printf("\n");

    State = RX;
}

static void OnTxTimeout( void )
{
    sendError+=1;
    Radio.Sleep( );
    // printf("\n[Radio FSM] Event: OnTxTimeout\n");
    State = TX_TIMEOUT;
}

static void OnRxTimeout( void )
{
    recvError+=1;
    Radio.Sleep( );
    // printf("\r\n[Radio FSM] Event: OnRxTimeout, State : %s -> %s \r\n", Device_state(State), Device_state(RX_TIMEOUT));
    State = RX_TIMEOUT;
}

static void OnRxError( void )
{
    recvError+=1;
    Radio.Sleep( );
    // printf("\r\n[Radio FSM] Event: OnRxError, State : %s -> %s \r\n", Device_state(State), Device_state(RX_ERROR));
    State = RX_ERROR;
}

void TASK_lora_ping_pong (void *parms) {
    printf("TASK lora_ping_pong .... .. \n");

    uint16_t pid = 6688;
    uint16_t seq = 0;
    bool isServer = true;

    lora_private_init();
    printf("Init Lora network: private\n");

    if(isServer){
        seq+=1;
        ping_init_pck(PTYPE_PING_REQ, PCODE_0, pid, seq);
        DelayMs(1);
        Radio.Send(Send_buffer.val, BUFFER_SIZE);
    }else{
        Radio.Rx( RX_TIMEOUT_VALUE );
    }
    State = LOWPOWER;

    for ( ; ; ) {
        vTaskDelay (2000 / portTICK_PERIOD_MS);
       
        switch( State )
        {
          case RX:
            //表示刚接收完成
            printf("Recv %d bytes RSSI=%d SNR=%d icmp_seq=%u pid=%u [send]ok:%u error:%u, [recv]:ok:%u error:%u\n",
                    BUFFER_SIZE, RssiValue, SnrValue, seq, pid,
                    sendSucc, sendError, recvSucc, recvError);
            if(isServer){
                seq+=1;
                ping_init_pck(PTYPE_PING_REQ, PCODE_0, pid, seq);
            }else{
                ping_init_pck(PTYPE_PING_REPLY, PCODE_0, pid, Recv_buffer.data.head.seq);
            }
            printf("Send a pkt start pid=%d seq=%d\n", pid, seq);
            Radio.Send(Send_buffer.val, BUFFER_SIZE);
            State = LOWPOWER;
            break;
          case TX:
            //表示刚发送完成
            printf("Send a pkt done.So waiting to recv pkt in %d ms\n", RX_TIMEOUT_VALUE);
            Radio.Rx( RX_TIMEOUT_VALUE );
            State = LOWPOWER;
            break;
          case RX_ERROR:
          case RX_TIMEOUT:
            //表示刚接收失败
            printf("Recv timeout on seq:%u [send]ok:%u error:%u, [recv]:ok:%u error:%u\n", seq,
                sendSucc, sendError, recvSucc, recvError);
            if(isServer){
                seq+=1;
                ping_init_pck(PTYPE_PING_REQ, PCODE_0, pid, seq);
                printf("Send a pkt pid=%d seq=%d\n", pid, seq);
                Radio.Send(Send_buffer.val, BUFFER_SIZE);
            }else{
                Radio.Rx( RX_TIMEOUT_VALUE );
                State = LOWPOWER;
            }
            State = LOWPOWER;
            break;
          case TX_TIMEOUT:
            //表示刚发送失败
            printf("Send timeout on seq:%u [send]ok:%u error:%u, [recv]:ok:%u error:%u\n", seq,
                sendSucc, sendError, recvSucc, recvError);
            if(isServer){
                //发送失败后，则等待超时，然后再发下一个
                State = TX;
            }else{
                Radio.Rx( RX_TIMEOUT_VALUE );
                State = LOWPOWER;
            }

            break;
          case LOWPOWER:
            break;
          default:
            //Nothing to do
            break;
        }
        TimerLowPowerHandler();
    }
}



#endif