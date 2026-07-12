/*
MPU6050 VCC -> Nano 5V 또는 3.3V
MPU6050 GND -> Nano GND
MPU6050 SDA -> Nano A4 / PC4
MPU6050 SCL -> Nano A5 / PC5
*/

#define F_CPU 16000000UL

#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>
#include <math.h>

#define UART_BAUD 57600UL
#define TWI_CLOCK_HZ 400000UL

#define MPU6050_ADDR_7BIT 0x68
#define MPU6050_ADDR_W    0xD0
#define MPU6050_ADDR_R    0xD1

#define MPU6050_CONFIG       0x1A
#define MPU6050_GYRO_CONFIG  0x1B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_PWR_MGMT_1   0x6B
#define MPU6050_WHO_AM_I     0x75

#define LOOP_DT_SECONDS 0.004f
#define RAD_TO_DEG      57.2957795f
#define CALIB_SAMPLES   2000U
#define PRINT_DIVIDER   10U

int16_t AccXLSB, AccYLSB, AccZLSB;
int16_t GyroXLSB, GyroYLSB, GyroZLSB;

float RateRoll, RatePitch, RateYaw;
float RateCalibrationRoll, RateCalibrationPitch, RateCalibrationYaw;
// 가속도 센서 보정(Offset)을 위한 변수 추가
float AngleCalibrationRoll = 0.0f, AngleCalibrationPitch = 0.0f;

float AccX, AccY, AccZ;
float AngleRoll, AnglePitch;

float KalmanAngleRoll = 0.0f;
float KalmanAnglePitch = 0.0f;
float KalmanUncertaintyAngleRoll = 4.0f;
float KalmanUncertaintyAnglePitch = 4.0f;

void USART_Transmit(char data)
{
	while (!(UCSR0A & (1 << UDRE0))) {
	}
	UDR0 = data;
}

void USART_SendString(const char *str)
{
	while (*str) {
		USART_Transmit(*str++);
	}
}

void USART_SendUInt32(uint32_t value)
{
	char buffer[10];
	uint8_t index = 0;

	if (value == 0) {
		USART_Transmit('0');
		return;
	}

	while (value > 0) {
		buffer[index++] = (char)('0' + (value % 10));
		value /= 10;
	}

	while (index > 0) {
		USART_Transmit(buffer[--index]);
	}
}

void USART_SendInt32(int32_t value)
{
	if (value < 0) {
		USART_Transmit('-');
		value = -value;
	}
	USART_SendUInt32((uint32_t)value);
}

void USART_SendFloat(float value, uint8_t decimals)
{
	uint32_t scale = 1;

	for (uint8_t i = 0; i < decimals; i++) {
		scale *= 10;
	}

	if (value < 0.0f) {
		USART_Transmit('-');
		value = -value;
	}

	uint32_t whole = (uint32_t)value;
	uint32_t fraction = (uint32_t)(((value - (float)whole) * (float)scale) + 0.5f);

	if (fraction >= scale) {
		whole++;
		fraction -= scale;
	}

	USART_SendUInt32(whole);

	if (decimals == 0) {
		return;
	}

	USART_Transmit('.');

	for (uint32_t divisor = scale / 10; divisor > 0; divisor /= 10) {
		USART_Transmit((char)('0' + ((fraction / divisor) % 10)));
	}
}

void USART_Init(void)
{
	uint16_t ubrr = (uint16_t)((F_CPU / (8UL * UART_BAUD)) - 1UL);

	UCSR0A = (1 << U2X0);
	UBRR0H = (uint8_t)(ubrr >> 8);
	UBRR0L = (uint8_t)ubrr;
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
	UCSR0B = (1 << TXEN0);
}

uint8_t TWI_Wait(void)
{
	uint16_t timeout = 65000;

	while (!(TWCR & (1 << TWINT))) {
		if (--timeout == 0) {
			return 0;
		}
	}

	return 1;
}

void TWI_Init(void)
{
	TWSR = 0x00;
	TWBR = (uint8_t)(((F_CPU / TWI_CLOCK_HZ) - 16UL) / 2UL);
	TWCR = (1 << TWEN);
}

uint8_t TWI_Start(uint8_t address)
{
	uint8_t status;

	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	if (!TWI_Wait()) {
		return 0;
	}

	status = TWSR & 0xF8;
	if ((status != 0x08) && (status != 0x10)) {
		return 0;
	}

	TWDR = address;
	TWCR = (1 << TWINT) | (1 << TWEN);
	if (!TWI_Wait()) {
		return 0;
	}

	status = TWSR & 0xF8;

	if (address & 0x01) {
		return status == 0x40;
	}

	return status == 0x18;
}

void TWI_Stop(void)
{
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

uint8_t TWI_Write(uint8_t data)
{
	TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN);

	if (!TWI_Wait()) {
		return 0;
	}

	return (TWSR & 0xF8) == 0x28;
}

uint8_t TWI_ReadACK(void)
{
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
	TWI_Wait();
	return TWDR;
}

uint8_t TWI_ReadNACK(void)
{
	TWCR = (1 << TWINT) | (1 << TWEN);
	TWI_Wait();
	return TWDR;
}

uint8_t MPU6050_Write(uint8_t reg, uint8_t data)
{
	if (!TWI_Start(MPU6050_ADDR_W)) {
		TWI_Stop();
		return 0;
	}

	if (!TWI_Write(reg)) {
		TWI_Stop();
		return 0;
	}

	if (!TWI_Write(data)) {
		TWI_Stop();
		return 0;
	}

	TWI_Stop();
	return 1;
}

uint8_t MPU6050_ReadRegister(uint8_t reg, uint8_t *data)
{
	if (!TWI_Start(MPU6050_ADDR_W)) {
		TWI_Stop();
		return 0;
	}

	if (!TWI_Write(reg)) {
		TWI_Stop();
		return 0;
	}

	if (!TWI_Start(MPU6050_ADDR_R)) {
		TWI_Stop();
		return 0;
	}

	*data = TWI_ReadNACK();
	TWI_Stop();

	return 1;
}

uint8_t MPU6050_ReadBurst(uint8_t start_reg, uint8_t *buffer, uint8_t length)
{
	if (!TWI_Start(MPU6050_ADDR_W)) {
		TWI_Stop();
		return 0;
	}

	if (!TWI_Write(start_reg)) {
		TWI_Stop();
		return 0;
	}

	if (!TWI_Start(MPU6050_ADDR_R)) {
		TWI_Stop();
		return 0;
	}

	for (uint8_t i = 0; i < length; i++) {
		if (i == (length - 1)) {
			buffer[i] = TWI_ReadNACK();
			} else {
			buffer[i] = TWI_ReadACK();
		}
	}

	TWI_Stop();
	return 1;
}

uint8_t MPU6050_Init(void)
{
	if (!MPU6050_Write(MPU6050_PWR_MGMT_1, 0x00)) {
		return 0;
	}

	_delay_ms(250);

	if (!MPU6050_Write(MPU6050_CONFIG, 0x05)) {
		return 0;
	}

	if (!MPU6050_Write(MPU6050_ACCEL_CONFIG, 0x10)) {
		return 0;
	}

	if (!MPU6050_Write(MPU6050_GYRO_CONFIG, 0x08)) {
		return 0;
	}

	return 1;
}

uint8_t ReadSensorSignals(void)
{
	uint8_t buffer[14];

	if (!MPU6050_ReadBurst(MPU6050_ACCEL_XOUT_H, buffer, 14)) {
		return 0;
	}

	AccXLSB = (int16_t)(((uint16_t)buffer[0] << 8) | buffer[1]);
	AccYLSB = (int16_t)(((uint16_t)buffer[2] << 8) | buffer[3]);
	AccZLSB = (int16_t)(((uint16_t)buffer[4] << 8) | buffer[5]);

	GyroXLSB = (int16_t)(((uint16_t)buffer[8] << 8) | buffer[9]);
	GyroYLSB = (int16_t)(((uint16_t)buffer[10] << 8) | buffer[11]);
	GyroZLSB = (int16_t)(((uint16_t)buffer[12] << 8) | buffer[13]);

	RateRoll = (float)GyroXLSB / 65.5f;
	RatePitch = (float)GyroYLSB / 65.5f;
	RateYaw = (float)GyroZLSB / 65.5f;

	AccX = (float)AccXLSB / 4096.0f;
	AccY = (float)AccYLSB / 4096.0f;
	AccZ = (float)AccZLSB / 4096.0f;

	AngleRoll = atanf(AccY / sqrtf((AccX * AccX) + (AccZ * AccZ))) * RAD_TO_DEG;
	AnglePitch = -atanf(AccX / sqrtf((AccY * AccY) + (AccZ * AccZ))) * RAD_TO_DEG;

	return 1;
}

void Kalman1D(float *state, float *uncertainty, float input_rate, float measurement)
{
	float kalman_gain;

	*state = *state + (LOOP_DT_SECONDS * input_rate);
	*uncertainty = *uncertainty + (LOOP_DT_SECONDS * LOOP_DT_SECONDS * 4.0f * 4.0f);

	kalman_gain = *uncertainty / (*uncertainty + (3.0f * 3.0f));

	*state = *state + kalman_gain * (measurement - *state);
	*uncertainty = (1.0f - kalman_gain) * (*uncertainty);
}

void Timer1_Init_250Hz(void)
{
	TCCR1A = 0x00;
	TCCR1B = 0x00;
	TCNT1 = 0;

	OCR1A = 7999;
	TIFR1 = (1 << OCF1A);
	TCCR1B = (1 << WGM12) | (1 << CS11);
}

void Timer1_Wait4ms(void)
{
	while (!(TIFR1 & (1 << OCF1A))) {
	}
	TIFR1 = (1 << OCF1A);
}

void PrintHeader(void)
{
	USART_SendString("KalmanRoll,KalmanPitch\r\n");
}

void PrintData(void)
{
	USART_SendFloat(KalmanAngleRoll, 2); USART_Transmit(',');
	USART_SendFloat(KalmanAnglePitch, 2);

	USART_SendString("\r\n");
}

void FailForever(const char *message)
{
	USART_SendString(message);
	USART_SendString("\r\n");

	while (1) {
		PORTB ^= (1 << PORTB5);
		_delay_ms(250);
	}
}

int main(void)
{
	uint8_t who_am_i = 0;
	uint8_t print_count = 0;

	DDRB |= (1 << PORTB5);
	PORTB &= ~(1 << PORTB5);

	USART_Init();
	TWI_Init();

	USART_SendString("MPU6050 Carbon 1D Kalman, Atmel Studio main.c\r\n");

	if (!MPU6050_Init()) {
		FailForever("MPU6050 init failed");
	}

	if (!MPU6050_ReadRegister(MPU6050_WHO_AM_I, &who_am_i)) {
		FailForever("WHO_AM_I read failed at address 0x68");
	}

	USART_SendString("WHO_AM_I=");
	USART_SendInt32(who_am_i);
	USART_SendString("\r\n");

	if (who_am_i != MPU6050_ADDR_7BIT) {
		USART_SendString("Warning: WHO_AM_I is not 104, but I2C read succeeded. Continue anyway.\r\n");
	}

	// 자이로와 가속도(수평 영점)를 동시에 보정 시작
	USART_SendString("Keep MPU6050 still. Calibrating gyro and accel...\r\n");

	for (uint16_t i = 0; i < CALIB_SAMPLES; i++) {
		if (!ReadSensorSignals()) {
			FailForever("Calibration I2C read failed");
		}

		RateCalibrationRoll += RateRoll;
		RateCalibrationPitch += RatePitch;
		RateCalibrationYaw += RateYaw;

		// 가속도로부터 얻은 각도 누적
		AngleCalibrationRoll += AngleRoll;
		AngleCalibrationPitch += AnglePitch;

		if ((i % 200U) == 0U) {
			USART_Transmit('.');
		}

		_delay_ms(1);
	}

	USART_SendString("\r\n");

	// 각 값들의 평균 도출
	RateCalibrationRoll /= (float)CALIB_SAMPLES;
	RateCalibrationPitch /= (float)CALIB_SAMPLES;
	RateCalibrationYaw /= (float)CALIB_SAMPLES;

	AngleCalibrationRoll /= (float)CALIB_SAMPLES;
	AngleCalibrationPitch /= (float)CALIB_SAMPLES;

	if (!ReadSensorSignals()) {
		FailForever("Initial sensor read failed");
	}

	// 구한 보정값(Offset)을 빼서 초기 각도를 0에 가깝게 맞춤
	KalmanAngleRoll = AngleRoll - AngleCalibrationRoll;
	KalmanAnglePitch = AnglePitch - AngleCalibrationPitch;
	KalmanUncertaintyAngleRoll = 4.0f;
	KalmanUncertaintyAnglePitch = 4.0f;

	Timer1_Init_250Hz();
	PrintHeader();

	while (1) {
		Timer1_Wait4ms();

		if (!ReadSensorSignals()) {
			USART_SendString("I2C read error\r\n");
			continue;
		}

		// 자이로 오프셋 제거
		RateRoll -= RateCalibrationRoll;
		RatePitch -= RateCalibrationPitch;
		RateYaw -= RateCalibrationYaw;

		// 가속도 오프셋(영점) 제거
		AngleRoll -= AngleCalibrationRoll;
		AnglePitch -= AngleCalibrationPitch;

		// 영점 조절된 값들을 칼만 필터에 대입
		Kalman1D(&KalmanAngleRoll, &KalmanUncertaintyAngleRoll, RateRoll, AngleRoll);
		Kalman1D(&KalmanAnglePitch, &KalmanUncertaintyAnglePitch, RatePitch, AnglePitch);

		print_count++;

		if (print_count >= PRINT_DIVIDER) {
			print_count = 0;
			PORTB ^= (1 << PORTB5);
			PrintData();
		}
	}
}