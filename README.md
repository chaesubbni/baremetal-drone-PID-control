# baremetal-drone-PID-control
학교 지원 사업을 통해 수행한 프로젝트로, 외부 라이브러리 없이 레지스터를 직접 제어하여 드론의 수평 유지 시스템을 구현했습니다. 직접 구현한 칼만 필터로 IMU센서를 활용한 정밀한 자세를 추정했고, PID 제어 알고리즘을 통해 모터를 실시간으로 제어했습니다.

또한, 하드웨어 타이밍 오차로 인한 기체 쏠림을 방지하고자 모터 제어 신호의 동시 업데이트 및 인터럽트 제어 로직을 설계하여 구동 안정성을 높였습니다.

**⚠️현재 프로젝트 상황 공지**
- 현재 컴퓨터 고장 이슈로 작성했던 코드 복원 중 입니다 ㅠㅠ
- 또한, 야외 테스트 중 추락해 대부분의 부품들이 파손되어 멀쩡한 부품들을 찾아서 테스트 중 입니다.


---------------

## 드론 테스트 하드웨어 제작
<table border="0" cellpadding="0" cellspacing="0" width="100%">
  <tr>
    <td width="40%" align="center" style="vertical-align: middle; border: none;">
      <img src="https://github.com/user-attachments/assets/5f2f9a90-0f73-4d9f-b6b3-f2a348778121" alt="드론 하드웨어" style="display: block; width: 100%; height: auto;">
    </td>
    <td width="60%" align="center" style="vertical-align: middle; border: none;">
      <video src="https://github.com/user-attachments/assets/7ba754b7-89ef-4e2c-ac61-23e5a5d4aa4c" controls style="display: block; width: 100%; height: auto;"></video>
    </td>
  </tr>
</table>

## MPU6050_EKF_Roll_Pitch 코드

- 외부 라이브러리 없이 I2C(TWI) 통신 레지스터를 직접 제어해 MPU6050의 가속도 및 자이로 데이터를 읽어오는 코드입니다.
- 칼만 필터 알고리즘을 C언어로 직접 구현하여, 자이로 센서의 각속도 데이터로 자세를 예측하고 가속도 센서의 데이터로 실시간 보정해 정밀한 Roll, Pitch 각도를 추정합니다.
- 시스템 부팅 시 2,000개의 센서 샘플을 수집하여 자이로의 초기 누적 오차(Drift)와 가속도의 수평 영점(Offset)을 캘리브레이션하는 로직을 적용해 센서 편차를 잡았습니다.
- 하드웨어 타이머를 활용해 250Hz(4ms)의 엄격한 제어 주기를 강제함으로써, 적분 시간(dt) 오차로 인한 필터 발산을 방지하고 신뢰도 높은 각도 데이터를 산출합니다.
- 내장 기능 외에 UART 통신 프로토콜을 레지스터 레벨에서 직접 구현하여 필터링된 데이터를 PC로 전송하고, MATLAB과의 연동 및 데이터 시각화를 통해 드론의 현재 자세 상태를 실시간으로 모니터링할 수 있도록 설계했습니다.
--------
**🎥 아래 이미지를 클릭하여 테스트 영상을 확인해 보세요!**
<a href="https://youtu.be/IzBIH2YIv_E">
  <img src="https://img.youtube.com/vi/IzBIH2YIv_E/0.jpg" alt="MPU6050 테스트 영상" width="100%">
</a>
------------
## bldc_motor_control 코드

### ESC 캘리브레이션
- 모터의 최소/최대 제어 스로틀 범위를 설정하는 과정입니다.
- 스로틀 신호의 듀티 사이클 최소값과 최대값을 ESC에 기억시켜, 모든 모터가 동일한 타이밍과 속도로 회전할 수 있도록 기준을 잡아줍니다.
- 바인딩 성공 후 설정 완료 알림음이 발생합니다.

<video src="https://github.com/user-attachments/assets/b8bd7f2a-72bd-4a6e-aebf-ecaf0cd57ae3" width="60%" controls></video>

---

### BLDC 모터 속도 제어
- 캘리브레이션이 완료된 후, 입력 신호에 따라 BLDC 모터의 RPM을 실시간으로 제어하는 테스트입니다.
- 타이머 레지스터 동시 업데이트를 구현하기 위해 TCNT1 값을 확인해 TOP에 임박한 시점을 피해 안전하게 스로틀 값을 업데이트 했습니다. 이를 통해 두 모터의 듀티 사이클이 서로 다른 주기에 엇갈려 반영되는 현상을 방지하고 완벽한 동시 업데이트를 구현했습니다.
- 또한, 두 모터의 카운트 값을 저장하는 OCR1A/B 레지스터에 값을 쓰는 동안 다른 인터럽트가 개입하여 데이터가 꼬이는 것을 막기 위해 상태 레지스터 SREG를 백업하고 전역 인터럽트를 일시적으로 차단해 모터 제어 명령을 항상 최우선 순위로 안전하게 처리했습니다.

<video src="https://github.com/user-attachments/assets/138a8aa1-6b8d-4985-909f-4a96413a1091" width="60%" controls></video>

## MPU6050_EKF_Roll_Pitch
