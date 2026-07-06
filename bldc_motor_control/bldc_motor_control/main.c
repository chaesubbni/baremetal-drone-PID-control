#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

// 핀 설정
#define TRIG_PIN PORTD6 // 초음파 Trig (D6)
#define ECHO_PIN PORTD7 // 초음파 Echo (D7)
#define SW_PIN   PORTD2 // 스위치 입력 핀 (D2)

// ESC PWM 설정 (Prescaler 8, 50Hz 기준)
#define PWM_MIN 2000 // 1ms 펄스 (모터 정지 / 스로틀 0%)
#define PWM_MAX 4000 // 2ms 펄스 (최대 속도 / 스로틀 100%)


// 1. USART 통신 관련 함수 (9600bps)
void USART_Init(void) {
	UBRR0H = 0;
	UBRR0L = 103;
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
	UCSR0B = (1 << TXEN0) | (1 << RXEN0);
}

void USART_transmit(char data) {
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = data;
}

void USART_transmit_string(const char* str) {
	while (*str) {
		USART_transmit(*str++);
	}
}

void USART_transmit_number(uint16_t num) {
	char buffer[10];
	int i = 0;
	if (num == 0) { USART_transmit('0'); return; }
	while (num > 0) { buffer[i++] = (num % 10) + '0'; num /= 10; }
	for (int j = i - 1; j >= 0; j--) { USART_transmit(buffer[j]); }
}

uint8_t USART_receive(void) {
	if (UCSR0A & (1 << RXC0)) return UDR0;
	return 0;
}


// 2. 초음파 센서 (HC-SR04) 제어 함수
uint16_t get_distance_cm(void) {
	uint32_t timeout = 30000;
	while (PIND & (1 << ECHO_PIN)) { if (--timeout == 0) break; }

	PORTD &= ~(1 << TRIG_PIN);
	_delay_us(2);
	PORTD |= (1 << TRIG_PIN);
	_delay_us(10);
	PORTD &= ~(1 << TRIG_PIN);

	timeout = 50000;
	while (!(PIND & (1 << ECHO_PIN))) { if (--timeout == 0) return 999; }

	uint32_t echo_time = 0;
	while (PIND & (1 << ECHO_PIN)) {
		echo_time++;
		_delay_us(1);
		if (echo_time > 35000) return 888;
	}
	return (uint16_t)(echo_time / 42);
}


// 3. 타이머1 (ESC 2개 제어용 Fast PWM)
void timer1_init(void) {
	// Fast PWM Mode 14 (TOP = ICR1)
	// COM1A1: OC1A (D9) Non-inverting 출력 활성화
	// COM1B1: OC1B (D10) Non-inverting 출력 활성화
	TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);
	TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11); // Prescaler 8
	
	// 50Hz (20ms) 주기 설정
	ICR1 = 39999;
}


// 4. ESC 자동 캘리브레이션
void calibrate_and_arm_escs(void) {
	USART_transmit_string("System Start. Calibration Mode (Delay Based).\r\n");
	
	// 1단계: 보드 켜지자마자 양쪽 모두 MAX 신호 쏘기
	OCR1A = PWM_MAX;
	OCR1B = PWM_MAX;
	
	USART_transmit_string("STEP 1: MAX PWM sent.\r\n");
	USART_transmit_string(">> Connect battery NOW! You have 5 seconds...\r\n");
	
	// 10초 대기 -> 이 10초 안에 ESC 메인 배터리를 연결해야 합니다!
	for(int i=10; i>0; i--) {
		USART_transmit_number(i);
		USART_transmit_string(" seconds left...\r\n");
		_delay_ms(1000);
	}

	// 2단계 -> 5초가 지나면 자동으로 양쪽 모두 MIN 신호로 떨어뜨리기
	USART_transmit_string("STEP 2: Sending MIN PWM. Wait for Arming...\r\n");
	OCR1A = PWM_MIN;
	OCR1B = PWM_MIN;
	
	// 최저점 인식 및 잠금 해제 소리가 날 때까지 4초 대기
	_delay_ms(4000);
	
	USART_transmit_string("Calibration & Arming Complete. Entering Speed Control Mode.\r\n");
}


int main(void) {
	// 입출력 방향 설정
	// PB1(D9), PB2(D10) 모두 출력으로 설정
	DDRB |= (1 << PORTB1) | (1 << PORTB2);
	DDRD |= (1 << TRIG_PIN);   // D6 - 초음파 Trig 출력
	DDRD &= ~(1 << ECHO_PIN);  // D7 - 초음파 Echo 입력
	
	// 스위치 설정
	DDRD &= ~(1 << SW_PIN);    // D2 - 스위치 입력
	PORTD |= (1 << SW_PIN);    // D2 - 내부 풀업 저항 활성화

	// 통신 및 타이머 초기화
	USART_Init();
	timer1_init();

	// ESC 초기화
	calibrate_and_arm_escs();

	uint16_t print_counter = 0;
	uint8_t last_sw_state = 1; // 풀업이므로 기본 상태는 1
	
	// 두 모터의 속도 변수
	uint16_t pwm_motor1 = PWM_MIN;
	uint16_t pwm_motor2 = PWM_MIN;

	while (1) {
		// 1. 스위치 입력 처리
		uint8_t sw_state = (PIND & (1 << SW_PIN)) ? 1 : 0;
		
		// 버튼이 눌리는 순간(하강 에지) 감지
		if (sw_state == 0 && last_sw_state == 1) {
			
			pwm_motor1 += 100;
			pwm_motor2 += 100;
			
			// 각각 최대 속도 제한 보호
			if (pwm_motor1 > PWM_MAX) pwm_motor1 = PWM_MAX;
			if (pwm_motor2 > PWM_MAX) pwm_motor2 = PWM_MAX;
			
			// 스위치 조작 시 완벽한 동시 업데이트
			while (TCNT1 > 39500);          // 리셋 임박 시 대기
			
			uint8_t sreg_save = SREG;       // 1. 현재 인터럽트 상태 백업
			SREG &= ~(1 << 7);              // 2. 전역 인터럽트 끄기 (I-bit 클리어)
			
			OCR1A = pwm_motor1;             // 3. 방해 없이 동시 업데이트
			OCR1B = pwm_motor2;
			
			SREG = sreg_save;               // 4. 인터럽트 원래 상태로 복원
			// =======================================================
			
			USART_transmit_string("Speed UP! M1: ");
			USART_transmit_number(pwm_motor1);
			USART_transmit_string(", M2: ");
			USART_transmit_number(pwm_motor2);
			USART_transmit_string("\r\n");
			
			_delay_ms(50); // 디바운싱
		}
		last_sw_state = sw_state;


		// 2. 초음파 센서 감지 및 정지 로직
		print_counter++;
		
		if (print_counter >= 30000) {
			uint16_t dist = get_distance_cm();
			
			// 장애물이 10cm 미만으로 가까워지면 두 모터 모두 즉시 정지
			if(dist < 10 && dist > 0) {
				USART_transmit_string("OBSTACLE DETECTED! STOP BOTH ESCs!\r\n");
				
				pwm_motor1 = PWM_MIN;
				pwm_motor2 = PWM_MIN;
				
				while (TCNT1 > 39500);
				
				uint8_t sreg_save = SREG;
				SREG &= ~(1 << 7);
				
				OCR1A = pwm_motor1;
				OCR1B = pwm_motor2;
				
				SREG = sreg_save;
			}
			print_counter = 0;
		}
	}
}