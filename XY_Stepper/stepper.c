#include "stepper.h"
#include <stdlib.h> 
#include <stdio.h>  // Cần cho sprintf

#define PI 3.1415926535f
#define ARC_SEGMENT_LEN 0.5f // Chia nhỏ đường cong thành các đoạn thẳng 0.5mm

/*-----------------------------------------*/
/* Hàm đọc cảm biến Home và Limit chung    */
/*-----------------------------------------*/
uint8_t Stepper_ReadHome(Stepper_t* motor)
{
    if (motor->HOME_Port == NULL) return 0; // Tránh lỗi nếu chưa config port
    return HAL_GPIO_ReadPin(motor->HOME_Port, motor->HOME_Pin) == GPIO_PIN_RESET;
}

uint8_t Stepper_ReadLimit(Stepper_t* motor)
{
    if (motor->LIMIT_Port == NULL) return 0; // Tránh lỗi nếu chưa config port
    return HAL_GPIO_ReadPin(motor->LIMIT_Port, motor->LIMIT_Pin) == GPIO_PIN_RESET;
}

/*-----------------------------------------*/
/* Hàm chạy động cơ cơ bản                 */
/*-----------------------------------------*/
void Stepper_Move(Stepper_t* motor, uint32_t steps, uint8_t dir) 
{
    if(motor->state != STATE_IDLE)  return;
    if (steps == 0) return;
    
    motor->currentDir = dir;
    HAL_GPIO_WritePin(motor->DIR_Port, motor->DIR_Pin, dir ? GPIO_PIN_SET : GPIO_PIN_RESET);

    motor->stepTarget = steps;
    motor->stepCount = 0;
    motor->state = STATE_ACCEL;
    motor->arrCurrent = motor->arrMax; 
  
    uint32_t accelNeeded = (motor->arrMax - motor->arrMin) / motor->arrChangePStep * 2;

    if(accelNeeded * 2 > steps) {
        motor->accelStep = steps / 2;
    } else {
        motor->accelStep = accelNeeded;
    }

    motor->decelStart = steps - motor->accelStep;

    __HAL_TIM_SET_AUTORELOAD(motor->htim, motor->arrCurrent);
    __HAL_TIM_SET_COMPARE(motor->htim, motor->channel, motor->arrCurrent / 2); 
    
    HAL_TIM_Base_Start_IT(motor->htim);                   
    HAL_TIM_PWM_Start(motor->htim, motor->channel);       
}

void Stepper_Move_mm(Stepper_t* motor, float distance_mm) 
{
    if (distance_mm == 0.0f) return;
    uint8_t dir = (distance_mm > 0) ? 1 : 0;
    float abs_distance = (distance_mm > 0) ? distance_mm : -distance_mm;
    uint32_t steps_to_move = (uint32_t)(abs_distance * motor->stepsPerMM);
    Stepper_Move(motor, steps_to_move, dir);
}

void Stepper_GoTo_mm(Stepper_t* motor, float target_mm) 
{
    int32_t target_steps = (int32_t)(target_mm * motor->stepsPerMM);
    int32_t delta_steps = target_steps - motor->currentPosSteps;
    if (delta_steps == 0) return; 

    uint8_t dir = (delta_steps > 0) ? 1 : 0;
    uint32_t steps_to_move = (uint32_t)(delta_steps > 0 ? delta_steps : -delta_steps);
    Stepper_Move(motor, steps_to_move, dir);
}

/*-----------------------------------------*/
/* Hàm Homing dùng chung cho mọi trục      */
/*-----------------------------------------*/
void Stepper_Home(Stepper_t* motor)
{
    char msg[30];
    sprintf(msg, "HOMING %c\r\n", motor->axisName);
    UART_Send(msg);

    // Nếu đang đứng trên sensor thì thoát ra trước
    if(Stepper_ReadHome(motor))
    {
        UART_Send("Already HOME\r\n");
        return;
    }
        
    HAL_Delay(100);

    // 1. FAST SEEK
    UART_Send("Fast seek\r\n");
    Stepper_Move(motor, 80000, 0);   // chạy về HOME (dir=0)

    while(1)
    {
        if(Stepper_ReadHome(motor))
        {
            motor->state = STATE_IDLE;
            HAL_TIM_Base_Stop_IT(motor->htim);
            HAL_TIM_PWM_Stop(motor->htim, motor->channel);
            UART_Send("HOME HIT\r\n");          
            break;
        }
    }

    HAL_Delay(200);

    // 2. BACKOFF 
    UART_Send("Backoff\r\n");
    Stepper_Move_mm(motor, 1);   // lùi 1mm
    while(motor->state != STATE_IDLE) HAL_Delay(50);

    // 3. SLOW SEEK
    UART_Send("Slow seek\r\n");
    Stepper_Move(motor, 1500, 0);   // chạy lại về HOME chậm

    while(1)
    {
        if(Stepper_ReadHome(motor))
        {
            motor->state = STATE_IDLE;
            HAL_TIM_Base_Stop_IT(motor->htim);
            HAL_TIM_PWM_Stop(motor->htim, motor->channel);
            UART_Send("HOME PRECISE\r\n");
            break;
        }
    }

    // 4. SET ZERO
    motor->stepCount = 0;
    motor->currentPosSteps = 0; 
    sprintf(msg, "%c HOME DONE\r\n", motor->axisName);
    UART_Send(msg);
}

/*-----------------------------------------------*/
/* Hàm Callback ngắt khi hoàn thành 1 chu kỳ PWM */
/*-----------------------------------------------*/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) 
{
    Stepper_t* motor = NULL;

    // Xác định timer nào đang ngắt
    if (htim->Instance == TIM2) motor = &MotorX;
    else if (htim->Instance == TIM3) motor = &MotorY;

    if(motor == NULL) return;

    GPIO_PinState currentDir = HAL_GPIO_ReadPin(motor->DIR_Port, motor->DIR_Pin);
    
    // Kiểm tra limit và home chung bằng hàm đọc
    if(Stepper_ReadLimit(motor) && currentDir == GPIO_PIN_SET)
    {
        motor->state = STATE_IDLE;
        HAL_TIM_Base_Stop_IT(motor->htim);
        HAL_TIM_PWM_Stop(motor->htim, motor->channel);
        UART_Send("LIMIT TRIGGERED\r\n");  
        return;
    } 
    
    if(Stepper_ReadHome(motor) && currentDir == GPIO_PIN_RESET)
    {
        motor->state = STATE_IDLE;
        HAL_TIM_Base_Stop_IT(motor->htim);
        HAL_TIM_PWM_Stop(motor->htim, motor->channel);
        UART_Send("HOME TRIGGERED\r\n");  
        return;
    }
    
    if (motor->state != STATE_IDLE) 
    {
        motor->stepCount++; 
  
        if (motor->currentDir == 1) {
            motor->currentPosSteps++; 
        } else {
            motor->currentPosSteps--; 
        }
        
        if (motor->stepCount >= motor->stepTarget) {
            motor->state = STATE_IDLE;
            moveDoneFlag = 1;
            HAL_TIM_Base_Stop_IT(motor->htim);             
            HAL_TIM_PWM_Stop(motor->htim, motor->channel); 
            return;
        }
        
        switch(motor->state)
        {
          case STATE_ACCEL:
            if(motor->arrCurrent > motor->arrMin) {
                motor->arrCurrent -= motor->arrChangePStep;
                if(motor->arrCurrent < motor->arrMin) motor->arrCurrent = motor->arrMin;
            }

            if(motor->stepCount >= motor->accelStep) {
                if(motor->stepCount >= motor->decelStart) motor->state = STATE_DECEL;
                else motor->state = STATE_RUN;
            }
            break;

          case STATE_RUN:
            if(motor->stepCount >= motor->decelStart) motor->state = STATE_DECEL;
            break;

          case STATE_DECEL:
            {
                uint32_t remain = motor->stepTarget - motor->stepCount;
                uint16_t delta = (motor->arrMax - motor->arrCurrent) / (remain + 1);
                if(delta < 1) delta = 1;
                motor->arrCurrent += delta;
                if(motor->arrCurrent > motor->arrMax) motor->arrCurrent = motor->arrMax;        
            }
            break;
            
          default:
            break;
        }

        __HAL_TIM_SET_AUTORELOAD(motor->htim, motor->arrCurrent);
        __HAL_TIM_SET_COMPARE(motor->htim, motor->channel, motor->arrCurrent / 2);
    }
}

/*-----------------------------------------*/
/* Chạy đồng bộ 2 trục X, Y (Nội suy)      */
/*-----------------------------------------*/
void Stepper_LineTo(float target_X, float target_Y)
{
    // 1. Tính số bước cần đi cho mỗi trục
    int32_t targetStepsX = (int32_t)(target_X * MotorX.stepsPerMM);
    int32_t targetStepsY = (int32_t)(target_Y * MotorY.stepsPerMM);
    
    int32_t deltaX = targetStepsX - MotorX.currentPosSteps;
    int32_t deltaY = targetStepsY - MotorY.currentPosSteps;

    uint32_t absX = (deltaX > 0) ? deltaX : -deltaX;
    uint32_t absY = (deltaY > 0) ? deltaY : -deltaY;

    if (absX == 0 && absY == 0) return; // Đã ở đúng vị trí

    // 2. Tốc độ gốc lớn nhất muốn đạt được (ARR nhỏ nhất = chạy nhanh nhất)
    uint16_t base_arrMin = 250; 

    // 3. Nội suy tốc độ: Trục nào đi ít hơn sẽ phải chạy chậm lại (ARR lớn hơn)
    if (absX > absY) 
    {
        MotorX.arrMin = base_arrMin; // Trục X đi dài hơn -> chạy max tốc
        if (absY > 0) 
        {
            MotorY.arrMin = base_arrMin * absX / absY;
            if (MotorY.arrMin > MotorY.arrMax) MotorY.arrMin = MotorY.arrMax;
        }
    } 
    else 
    {
        MotorY.arrMin = base_arrMin; // Trục Y đi dài hơn -> chạy max tốc
        if (absX > 0) 
        {
            MotorX.arrMin = base_arrMin * absY / absX;
            if (MotorX.arrMin > MotorX.arrMax) MotorX.arrMin = MotorX.arrMax;
        }
    }

    // 4. Bắn lệnh cho cả 2 timer xuất xung cùng một lúc
    if (absX > 0) Stepper_Move(&MotorX, absX, (deltaX > 0) ? 1 : 0);
    if (absY > 0) Stepper_Move(&MotorY, absY, (deltaY > 0) ? 1 : 0);
}
