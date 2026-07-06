# baremetal-drone-PID-control
학교 지원 사업을 통해 수행한 프로젝트로, 외부 라이브러리 없이 레지스터를 직접 제어하여 드론의 수평 유지 시스템을 구현했습니다. 센서 데이터 기반의 **확장 칼만 필터(EKF)**로 정밀한 자세를 추정하고, PID 제어 알고리즘을 통해 모터를 실시간으로 제어합니다.

> **현재 프로젝트 상황 공지**
> 
현재 컴퓨터 고장 이슈로 작성했던 코드 복원 중 입니다 ㅠㅠ

또한, 야외 테스트 중 추락해 대부분의 부품들이 파손되어 멀쩡한 부품들을 찾아서 테스트 중 입니다.

---------------

## 드론 테스트 하드웨어 제작

<video src="https://github.com/user-attachments/assets/7ba754b7-89ef-4e2c-ac61-23e5a5d4aa4c" width="60%" controls></video>


## ESC 캘리브레이션
- **모터의 최소/최대 제어 스로틀 범위를 설정하는 과정입니다.**
- 스로틀 신호의 듀티 사이클(Duty Cycle) 최소값과 최대값을 ESC에 기억시켜, 모든 모터가 동일한 타이밍과 속도로 회전할 수 있도록 기준을 잡아줍니다.
- 바인딩 성공 후 설정 완료 알림음이 발생합니다.

<video src="https://github.com/user-attachments/assets/b8bd7f2a-72bd-4a6e-aebf-ecaf0cd57ae3" width="60%" controls></video>

---

## BLDC 모터 속도 제어
- **캘리브레이션이 완료된 후, 입력 신호에 따라 BLDC 모터의 RPM을 실시간으로 제어하는 테스트입니다.**
- 타이머 PWM 인터럽트를 활용하여 각 모터에 들어가는 전력량을 정밀하게 조절합니다.
- 영상에서는 스위치를 누를때마다 두 개의 모터가 동기화되어 안정적으로 속도가 증가하는 것을 볼 수 있습니다.

<video src="https://github.com/user-attachments/assets/138a8aa1-6b8d-4985-909f-4a96413a1091" width="60%" controls></video>
