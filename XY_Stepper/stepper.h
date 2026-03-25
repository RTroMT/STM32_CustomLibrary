#ifndef STEPPER_H
#define STEPPER_H

#include "main.h" 
#include <stdint.h>

/* Typedef & Structs ---------------------------------------------------------*/
typedef enum {
  STATE_IDLE = 0,
  STATE_ACCEL,
  STATE_RUN,
  STATE_DECEL
} StepperState_t;

// Cấu hình động cơ (Đã bổ sung các chân cảm biến Home/Limit)
typedef struct {
    char      axisName;       // Tên trục (ví dụ: 'X', 'Y', 'Z') để in ra debug
    
    uint32_t  stepTarget;     
    uint32_t  stepCount;      
    uint32_t  accelStep;      
    uint32_t  decelStep;      
  
    uint16_t  arrMax;
    uint16_t  arrMin;
    uint16_t  arrCurrent;
    uint16_t  arrChangePStep; 
    
    uint32_t  decelStart;
    StepperState_t state;
    
    // Config phần cứng 
    GPIO_TypeDef* DIR_Port; 
    uint16_t      DIR_Pin;
    
    GPIO_TypeDef* HOME_Port;  // Chân cắm cảm biến HOME
    uint16_t      HOME_Pin;
    
    GPIO_TypeDef* LIMIT_Port; // Chân cắm cảm biến LIMIT
    uint16_t      LIMIT_Pin;
    
    // TIMER config
    TIM_HandleTypeDef* htim;
    uint32_t           channel;
    
    // Tọa độ & Vị trí
    float     stepsPerMM;
    int32_t   currentPosSteps;  
    uint8_t   currentDir;
} Stepper_t;

/* Extern Variables & Functions ----------------------------------------------*/
extern Stepper_t MotorX, MotorY; 
extern volatile uint8_t moveDoneFlag;
extern void UART_Send(char *msg);

/* Function Prototypes -------------------------------------------------------*/
// Các hàm đọc cảm biến (nhận tham số motor)
uint8_t Stepper_ReadHome(Stepper_t* motor);
uint8_t Stepper_ReadLimit(Stepper_t* motor);

// Chức năng di chuyển & Homing
void Stepper_Move(Stepper_t* motor, uint32_t steps, uint8_t dir);
void Stepper_Move_mm(Stepper_t* motor, float distance_mm);
void Stepper_GoTo_mm(Stepper_t* motor, float target_mm);
void Stepper_Home(Stepper_t* motor);
void Stepper_LineTo(float target_X, float target_Y);
#endif /* STEPPER_H */
