#include "stepper.h"
#include <stdlib.h> 
#include <stdio.h>  // Cần cho sprintf

#define PI 3.1415926535f
#define ARC_SEGMENT_LEN 0.5f // Chia nhỏ đường cong thành các đoạn thẳng 0.5mm

/*-----------------------------------------*/
/* BIẾN TOÀN CỤC CHO HỆ THỐNG              */
/*-----------------------------------------*/
volatile uint8_t sysAlarm = 0;   // 0 = Bình thường, 1 = Báo động
volatile uint8_t queueCount = 0; // Số lệnh đang có trong kho

/*-----------------------------------------*/
/* BIẾN CHO UART                           */
/*-----------------------------------------*/
static GCodeCmd_t cmdQueue[QUEUE_SIZE];
static volatile uint8_t queueHead = 0; 
static volatile uint8_t queueTail = 0;

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
    if (sysAlarm == 1) return;
    
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
    if (sysAlarm == 1) return;

    uint8_t dir = (distance_mm > 0) ? 1 : 0;
    float abs_distance = (distance_mm > 0) ? distance_mm : -distance_mm;
    uint32_t steps_to_move = (uint32_t)(abs_distance * motor->stepsPerMM);
    Stepper_Move(motor, steps_to_move, dir);
}

void Stepper_GoTo_mm(Stepper_t* motor, float target_mm) 
{
    if (sysAlarm == 1) return;

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
    sysAlarm = 0; 				// Tắt cờ báo động để cho phép Homing
    motor->isHoming = 1;  // Báo cho ngắt Timer biết đang Homing

    char msg[30];
    sprintf(msg, "HOMING %c\r\n", motor->axisName);
    UART_Send(msg);

    // Nếu vừa vào đã thấy đè lên sensor, tự động lùi ra trước 2mm
    if(Stepper_ReadHome(motor))
    {
        UART_Send("Already on HOME, backing off...\r\n");
        Stepper_Move_mm(motor, 2.0f); 
        while(motor->state != STATE_IDLE) HAL_Delay(10);
    }
        
    HAL_Delay(100);

    // 1. FAST SEEK
    UART_Send("Fast seek\r\n");
    Stepper_Move(motor, 100000, 0);   // Cho số bước cực lớn để đảm bảo tới nơi

    // Đổi while(1) thành kiểm tra state để chống treo
    while(motor->state != STATE_IDLE)
    {
        if(Stepper_ReadHome(motor))
        {
            motor->state = STATE_IDLE;
            HAL_TIM_Base_Stop_IT(motor->htim);
            HAL_TIM_PWM_Stop(motor->htim, motor->channel);
            break; // Chạm công tắc -> Thoát
        }
    }
    UART_Send("HOME HIT\r\n");

    HAL_Delay(200);

    // 2. BACKOFF (Lùi 2mm cho an toàn, 1mm nhiều khi công tắc chưa kịp nhả)
    UART_Send("Backoff\r\n");
    Stepper_Move_mm(motor, 2.0f);   
    while(motor->state != STATE_IDLE) HAL_Delay(10);

    // 3. SLOW SEEK
    UART_Send("Slow seek\r\n");
    Stepper_Move(motor, 5000, 0);   // Tăng lên 5000 bước (khoảng gần 4mm) để dư sức chạy tới

    while(motor->state != STATE_IDLE)
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

		// ---------------------------------------------------------
    // PULL-OFF (Tạo vùng đệm an toàn vì cmd X0, Y0 gây 
		// sysAlarm = 1)
    // ---------------------------------------------------------
    UART_Send("Pull-off to Safe Zero...\r\n");
    Stepper_Move_mm(motor, 1.0f);   // Lùi ra 1mm (hoặc 2mm tùy ý bác) để thoát hẳn cảm biến
    while(motor->state != STATE_IDLE) HAL_Delay(10);
		
		motor->isHoming = 0;
		
    // 4. SET ZERO (Chốt tọa độ 0 tại vị trí an toàn này)
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
        sysAlarm = 1;
				Queue_Clear();

        motor->state = STATE_IDLE;
        HAL_TIM_Base_Stop_IT(motor->htim);
        HAL_TIM_PWM_Stop(motor->htim, motor->channel);
        UART_Send("LIMIT TRIGGERED\r\n");  
        return;
    } 
    
		if (Stepper_ReadHome(motor) && motor->currentDir == GPIO_PIN_RESET)
    {
        motor->state = STATE_IDLE;
        HAL_TIM_Base_Stop_IT(motor->htim);
        HAL_TIM_PWM_Stop(motor->htim, motor->channel);
            
        // Nếu KHÔNG PHẢI đang Homing (isHoming == 0) thì mới rú còi báo động
        if (motor->isHoming == 0) 
        {
            sysAlarm = 1;
            queueCount = 0;
						Queue_Clear();
            UART_Send("HOME TRIGGERED\r\n");
        }
        return; // Thoát ngắt
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
							/*
                uint32_t remain = motor->stepTarget - motor->stepCount;
                uint16_t delta = (motor->arrMax - motor->arrCurrent) / (remain + 1);
                if(delta < 1) delta = 1;
                motor->arrCurrent += delta;
                if(motor->arrCurrent > motor->arrMax) motor->arrCurrent = motor->arrMax;        
							*/
							// Giảm tốc tuyến tính: Tăng ARR lên để chạy chậm lại
						if(motor->arrCurrent < motor->arrMax) {
							motor->arrCurrent += motor->arrChangePStep;
							if(motor->arrCurrent > motor->arrMax) 
								motor->arrCurrent = motor->arrMax;
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
void Stepper_LineTo(float target_X, float target_Y, float feedrate)
{
    if (sysAlarm == 1) return;

    // 1. Tính số bước cần đi cho mỗi trục
    int32_t targetStepsX = (int32_t)(target_X * MotorX.stepsPerMM);
    int32_t targetStepsY = (int32_t)(target_Y * MotorY.stepsPerMM);
    
    int32_t deltaX = targetStepsX - MotorX.currentPosSteps;
    int32_t deltaY = targetStepsY - MotorY.currentPosSteps;

    uint32_t absX = (deltaX > 0) ? deltaX : -deltaX;
    uint32_t absY = (deltaY > 0) ? deltaY : -deltaY;

    if (absX == 0 && absY == 0) return; // Đã ở đúng vị trí

		// 2. Tính tốc độ gốc từ Feedrate (F) (Quy đổi mm/phút ra giá trị nạp Timer)
    // Lấy stepsPerMM của trục X làm chuẩn
    float stepsPerSec = (feedrate / 60.0f) * MotorX.stepsPerMM;
    uint16_t base_arrMin;
    
    if (stepsPerSec <= 0) base_arrMin = MotorX.arrMinConfig; 
    else base_arrMin = (uint16_t)(1000000.0f / stepsPerSec); // Tần số timer 1MHz

    // CHỐT CHẶN AN TOÀN: Không cho chạy quá giới hạn phần cứng
    if (base_arrMin < MotorX.arrMinConfig) base_arrMin = MotorX.arrMinConfig;
    if (base_arrMin > 5000) base_arrMin = 5000; // Không cho chạy quá chậm

    // 3. Nội suy tốc độ: Rate Multiplier
		if (absX > absY) 
    {
        // TRỤC X ĐI DÀI HƠN (Lấy Full Cấu Hình)
        MotorX.arrMin = base_arrMin;
        MotorX.arrMax = MotorX.arrMaxConfig;
        MotorX.arrChangePStep = MotorX.arrChangePStepConfig;

        if (absY > 0) 
        {
            // Tính tỷ lệ (luôn >= 1.0) bằng float để tránh sai số làm tròn
            float ratio = (float)absX / (float)absY; 

            // Scale arrMin (Tốc độ chạy đều)
            uint32_t calcMin = (uint32_t)(base_arrMin * ratio);
            MotorY.arrMin = (calcMin > 65535) ? 65535 : calcMin;

            // Scale arrMax (Tốc độ xuất phát)
            uint32_t calcMax = (uint32_t)(MotorY.arrMaxConfig * ratio);
            MotorY.arrMax = (calcMax > 65535) ? 65535 : calcMax;
            
            // Chốt an toàn: Tránh lỗi toán học làm arrMax tụt thấp hơn arrMin
            if (MotorY.arrMax < MotorY.arrMin) MotorY.arrMax = MotorY.arrMin;

            // Scale Gia tốc theo BÌNH PHƯƠNG tỷ lệ (Ratio^2)
            uint32_t calcChange = (uint32_t)(MotorY.arrChangePStepConfig * ratio * ratio);
            MotorY.arrChangePStep = (calcChange > 65535) ? 65535 : calcChange;
            if (MotorY.arrChangePStep == 0) MotorY.arrChangePStep = 1;
        }
    } 
    else 
    {
        // TRỤC Y ĐI DÀI HƠN (Lấy Full Cấu Hình)
        MotorY.arrMin = base_arrMin;
        MotorY.arrMax = MotorY.arrMaxConfig;
        MotorY.arrChangePStep = MotorY.arrChangePStepConfig;

        if (absX > 0) 
        {
            // Tính tỷ lệ
            float ratio = (float)absY / (float)absX; 

            uint32_t calcMin = (uint32_t)(base_arrMin * ratio);
            MotorX.arrMin = (calcMin > 65535) ? 65535 : calcMin;

            uint32_t calcMax = (uint32_t)(MotorX.arrMaxConfig * ratio);
            MotorX.arrMax = (calcMax > 65535) ? 65535 : calcMax;
            
            if (MotorX.arrMax < MotorX.arrMin) MotorX.arrMax = MotorX.arrMin;

            // Scale Gia tốc theo BÌNH PHƯƠNG tỷ lệ (Ratio^2)
            uint32_t calcChange = (uint32_t)(MotorX.arrChangePStepConfig * ratio * ratio);
            MotorX.arrChangePStep = (calcChange > 65535) ? 65535 : calcChange;
            if (MotorX.arrChangePStep == 0) MotorX.arrChangePStep = 1;
        }
    }

    // 4. Bắn lệnh cho cả 2 timer xuất xung cùng một lúc
    if (absX > 0) Stepper_Move(&MotorX, absX, (deltaX > 0) ? 1 : 0);
    if (absY > 0) Stepper_Move(&MotorY, absY, (deltaY > 0) ? 1 : 0);
}

/*-----------------------------------------*/
/* HÀM QUẢN LÝ BỘ ĐỆM G-CODE (RING BUFFER) */
/*-----------------------------------------*/
uint8_t Queue_Enqueue(uint8_t type, float x, float y, float i, float j, float f) 
{
    if (queueCount >= QUEUE_SIZE) return 0; // Lỗi: Kho đầy
    
    cmdQueue[queueHead].cmd_type = type;
    cmdQueue[queueHead].x = x;
    cmdQueue[queueHead].y = y;
    cmdQueue[queueHead].i = i;
    cmdQueue[queueHead].j = j;
		cmdQueue[queueHead].f = f;
	
		//CRITICAL SECTION
		__disable_irq(); 
    queueHead = (queueHead + 1) % QUEUE_SIZE;
    queueCount++;
    __enable_irq();
	
    return 1; // Thành công
}

uint8_t Queue_Dequeue(GCodeCmd_t *cmd) 
{
    if (queueCount == 0) return 0; // Lỗi: Kho trống
    
    *cmd = cmdQueue[queueTail];
		
		//CRITICAL SECTION
		__disable_irq();
    queueTail = (queueTail + 1) % QUEUE_SIZE;
    queueCount--;
    __enable_irq();
	
    return 1; // Thành công
}

/* Hàm xóa sạch kho lệnh khi có báo động khẩn cấp */
void Queue_Clear(void)
{
    __disable_irq(); // Khóa ngắt để dọn dẹp cho an toàn
    queueCount = 0;
    queueHead = 0;
    queueTail = 0;
    __enable_irq();
}

